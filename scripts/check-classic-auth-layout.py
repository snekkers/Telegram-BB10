#!/usr/bin/env python3
"""Inspect group/channel avatars and shared chat-page layout using offline demo data."""
import json
import shutil
import shlex
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/auth-layout'
sys.path.insert(0,str(BASE/'python'))
import paramiko
installed=json.loads((ROOT/'build/people/installation.json').read_text())
client=paramiko.SSHClient();client.load_host_keys(str(BASE/'ssh/known_hosts'))
client.set_missing_host_key_policy(paramiko.RejectPolicy())
client.connect('169.254.0.1',username='root',pkey=paramiko.RSAKey.from_private_key_file(str(BASE/'ssh/classic_rsa')),
    look_for_keys=False,allow_agent=False,timeout=8,
    disabled_algorithms={'pubkeys':['rsa-sha2-512','rsa-sha2-256']})
folder=installed['qa_directory'];captures=WORK/'captures';captures.mkdir(parents=True,exist_ok=True)
results=[]
def command(value):
    i,o,e=client.exec_command(value,timeout=30)
    data=o.read();errors=e.read();code=o.channel.recv_exit_status();i.close();o.close();e.close()
    assert code==0,errors.decode(errors='replace')[:200]
    return data
def step(name,action,capture=True,**fields):
    request=dict(fields,id=name,action=action)
    with client.open_sftp() as s:
        path=folder+'/request.json'
        with s.open(path+'.new','wb') as f:f.write(json.dumps(request,ensure_ascii=False).encode())
        s.chown(path+'.new',installed['uid'],installed['gid']);s.chmod(path+'.new',0o600)
        s.posix_rename(path+'.new',path)
    deadline=time.monotonic()+25
    while time.monotonic()<deadline:
        with client.open_sftp() as s:
            try:
                with s.open(folder+'/result.json','rb') as f:result=json.loads(f.read())
            except FileNotFoundError:result={}
            if result.get('id')==name:
                results.append(result)
                assert result.get('ok'),result
                if capture:assert result.get('capture_ok'),'Could not capture the native demo window'
                if capture and result.get('multiline') and result.get('composer_visible',True):
                    assert result.get('content_height',0)>0 and result['content_height']<700,'Sticky title bar is missing'
                    assert result['composer_bottom']<=result['content_height']+1,'Composer is outside the native viewport'
                if result.get('capture_ok'):s.get(folder+'/'+name+'.png',str(captures/(name+'.png')))
                print(name,':',json.dumps({k:result.get(k) for k in ['page','multiline','max_lines','composer_height','message_count','capture_ok','menu_state']}),flush=True)
                if capture and action!='capture':
                    time.sleep(1)
                    settled=step(name+'-settled','capture',capture=True)
                    shutil.copyfile(captures/(name+'-settled.png'),captures/(name+'.png'))
                    return settled
                return result
        time.sleep(.4)
    raise RuntimeError('No native UI result for '+name)
restore_session=False
try:
    rows=command('pidin arguments').decode().splitlines()
    app_pids=[line.split(None,1)[0] for line in rows if len(line.split(None,1))==2 and line.split(None,1)[1].split()[0]==installed['app_id']]
    assert len(app_pids)<=1,'Stop duplicate app instances before UI checks'
    if not app_pids:
        command('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::auth-layout-qa')+' > /pps/services/launcher/control')
    time.sleep(3)
    state=step('communities-session','session-state',capture=False)
    restore_session=state.get('has_saved_api',False)
    step('communities-stop','stop-session',capture=False)
    for index in range(15):
        state=step('communities-stopped-'+str(index),'session-state',capture=False)
        if state.get('phase')=='stopped':break
        time.sleep(.3)
    assert state.get('phase')=='stopped'
    if state.get('page') and state.get('page')!='chatsPage':step('communities-login-back','back',capture=False)
    step('auth-welcome','auth-welcome')
    for phase in ['password','phone','code']:
        preview=step('auth-'+phase,'auth-preview',phase=phase)
        assert preview.get('auth_preview_phase')==phase
        step('auth-back-'+phase,'back',capture=False)
        time.sleep(1.5)
    step('communities-demo','demo-list')
    communities=step('communities-list','chat-tab',index=1)
    assert communities['chat_count']==3
    group=step('communities-group','demo-chat',group=True)
    assert group['photo_count']>=1 and group['composer_visible']
    assert group.get('wide_messages') and group.get('widest_bubble',0)>=group['row_width']-24,group
    step('communities-group-back','back')
    channel=step('communities-channel','demo-channel')
    assert channel['photo_count']>=1 and not channel['composer_visible']
    assert channel.get('wide_messages') and channel.get('widest_bubble',0)>=channel['row_width']-24,channel
    assert channel['message_list_height']>=620
    step('communities-channel-back','back')
    private=step('private-chat','demo-chat')
    assert not private.get('wide_messages') and private.get('widest_bubble',0)>300,private
    step('private-back','back')
    (WORK/'ui-results.json').write_text(json.dumps({'version':installed['version'],'offline_demo_only':True,'checks':results},ensure_ascii=False,indent=2)+'\n')
    print('Checked auth states and wide group/channel messages on Classic; inspect captures separately')
finally:
    try:
        if restore_session:step('communities-resume','resume-saved',capture=False)
        step('communities-checks-disabled','disable-checks',capture=False)
    finally:client.close()
