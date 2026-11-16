#!/usr/bin/env python3
"""Inspect the native offline demo, capture its own window, and exercise composer bindings."""
import json
import shlex
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/ui-refresh'
sys.path.insert(0,str(BASE/'python'))
import paramiko
installed=json.loads((WORK/'installation.json').read_text())
client=paramiko.SSHClient();client.load_host_keys(str(BASE/'ssh/known_hosts'))
client.set_missing_host_key_policy(paramiko.RejectPolicy())
client.connect('169.254.0.1',username='root',pkey=paramiko.RSAKey.from_private_key_file(str(BASE/'ssh/classic_rsa')),
    look_for_keys=False,allow_agent=False,timeout=8,
    disabled_algorithms={'pubkeys':['rsa-sha2-512','rsa-sha2-256']})
folder=installed['qa_directory'];captures=WORK/'captures';captures.mkdir(exist_ok=True)
results=[]
def command(value):
    i,o,e=client.exec_command(value,timeout=30)
    data=o.read();errors=e.read();code=o.channel.recv_exit_status();i.close();o.close();e.close()
    assert code==0,errors.decode(errors='replace')[:200]
    return data
def step(name,action,**fields):
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
                assert result.get('capture_ok'),'Could not capture the native demo window'
                if result.get('multiline'):
                    assert result.get('content_height',0)>0 and result['content_height']<700,'Sticky title bar is missing'
                    assert result['composer_bottom']<=result['content_height']+1,'Composer is outside the native viewport'
                if result.get('capture_ok'):s.get(folder+'/'+name+'.png',str(captures/(name+'.png')))
                print(name,':',json.dumps({k:result.get(k) for k in ['page','multiline','max_lines','composer_height','message_count','capture_ok','menu_state']}),flush=True)
                return result
        time.sleep(.4)
    raise RuntimeError('No native UI result for '+name)
try:
    message='msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::berrygram-ui-qa'
    command('print '+shlex.quote(message)+' > /pps/services/launcher/control')
    time.sleep(3)
    reference=step('before','reference')
    step('back-from-reference','back')
    empty=step('after','demo-chat')
    assert empty['multiline'] and empty['max_lines']==4 and empty['action_bar_hidden']
    assert not empty['send_enabled']
    first=step('single-line','compose',text='Первая строка')
    assert first['send_enabled']
    newline=step('newline','newline')
    assert newline['draft']=='Первая строка\n'
    multiple=step('four-lines','compose',text='Первая строка\nВторая строка\nТретья строка\nЧетвёртая строка\nПятая строка')
    assert multiple['composer_height']>first['composer_height']
    sent=step('sent','send')
    assert sent['draft']=='' and not sent['send_enabled'] and sent['message_count']==empty['message_count']+1
    step('button-draft','compose',text='Проверка кнопки отправки')
    clicked=step('sent-button','send-button')
    assert clicked['draft']=='' and clicked['message_count']==empty['message_count']+2
    menu=step('menu','menu');assert menu['menu_state']!=0
    step('back-from-chat','back')
    group=step('group','demo-chat',group=True)
    step('back-from-group','back')
    # Leave an uncluttered demo open and turn off polling after the checks.
    step('ready','demo-chat',group=True)
    step('checks-disabled','disable-checks')
finally:
    path=WORK/'native-interface-checks.json';path.write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
    client.close()
print('Offline native composer, menu, navigation and own-window captures checked')
