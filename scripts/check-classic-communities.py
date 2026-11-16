#!/usr/bin/env python3
"""Inspect group/channel avatars and shared chat-page layout using offline demo data."""
import json
import shlex
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/community-comments'
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
                return result
        time.sleep(.4)
    raise RuntimeError('No native UI result for '+name)
restore_session=False
try:
    command('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::community-qa')+' > /pps/services/launcher/control')
    time.sleep(3)
    state=step('communities-session','session-state',capture=False)
    restore_session=state.get('has_saved_api',False)
    step('communities-stop','stop-session',capture=False)
    for index in range(15):
        state=step('communities-stopped-'+str(index),'session-state',capture=False)
        if state.get('phase')=='stopped':break
        time.sleep(.3)
    assert state.get('phase')=='stopped'
    if state.get('page')!='chatsPage':step('communities-login-back','back',capture=False)
    step('communities-demo','demo-list')
    communities=step('communities-list','chat-tab',index=1)
    assert communities['chat_count']==3
    group=step('communities-group','demo-chat',group=True)
    assert group['photo_count']>=1 and group['composer_visible']
    step('communities-group-back','back')
    channel=step('communities-channel','demo-channel')
    assert channel['photo_count']>=1 and not channel['composer_visible']
    assert channel['message_list_height']>=620
    step('communities-channel-back','back')
    (WORK/'ui-results.json').write_text(json.dumps({'version':installed['version'],'offline_demo_only':True,'checks':results},ensure_ascii=False,indent=2)+'\n')
    print('Verified native group/channel avatars and chat layout on Classic')
finally:
    try:
        if restore_session:step('communities-resume','resume-saved',capture=False)
        step('communities-checks-disabled','disable-checks',capture=False)
    finally:client.close()
