#!/usr/bin/env python3
"""Verify native demo avatars, contacts, presence and read-only channels on Classic."""
import json
import shlex
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/people'
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
try:
    message='msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::berrygram-ui-qa'
    command('print '+shlex.quote(message)+' > /pps/services/launcher/control')
    time.sleep(3)
    state=step('session','session-state',capture=False)
    restore_session=False
    if state.get('has_saved_api'):
        step('resume-existing','resume-saved',capture=False)
        for index in range(20):
            time.sleep(.5);state=step('auth-'+str(index),'session-state',capture=False)
            if state['phase'] not in ['starting','parameters','configuring']:break
        if state.get('ready'):
            restore_session=True
            for index in range(15):
                real=step('read-contacts-'+str(index),'live-people',capture=False)
                if not real.get('live_contacts_loading'):break
                time.sleep(.5)
            (WORK/'account-read-check.json').write_text(json.dumps({k:real.get(k) for k in ['ready','phase','live_contact_count','live_contacts_loading','live_contacts_error']},indent=2)+chr(10))
        step('stop-for-demo','stop-session',capture=False)
        for index in range(15):
            state=step('stopping-'+str(index),'session-state',capture=False)
            if state['phase']=='stopped':break
            time.sleep(.5)
    root=step('personal','demo-list');assert root['chat_count']==2 and root['demo_avatar_cached']
    contacts=step('contacts','contacts');assert contacts['page']=='contactsPage' and contacts['contact_count']==3
    query=step('find-anna','contact-query',text='@anna');assert query['contact_count']==1
    assert query['contact_rows'][0]['title']=='Анна'
    step('clear-contact-query','contact-query',text='')
    recent=step('recent-contacts','contact-tab',index=1);assert recent['contact_count']==2
    step('all-contacts','contact-tab',index=0)
    anna=step('open-anna','contact-open',index=0)
    assert anna['page']=='chatPage' and anna['status_active'] and 'онлайн' in anna['status_text'] and anna['photo_count']>=1
    typing=step('typing','presence',status='typing');assert 'печатает' in typing['status_text'] and typing['status_active']
    time.sleep(6.5)
    expired=step('typing-expired','capture');assert 'печатает' not in expired['status_text']
    offline=step('last-seen','presence',status='offline');assert 'вчера' in offline['status_text'] and not offline['status_active']
    hidden=step('hidden-status','presence',status='hidden');assert 'недоступен' in hidden['status_text']
    step('online','presence',status='online')
    reply=step('reply','reply',message_id='7');assert reply['composer_mode']=='reply'
    step('reply-text','compose',text='Проверка аватара и статуса')
    sent=step('reply-sent','send');assert sent['demo_messages'][-1]['replyQuote']
    step('back-contacts','back');step('back-root','back')
    step('contacts-again','contacts')
    boris_query=step('find-boris','contact-query',text='Борис');assert boris_query['contact_count']==1
    boris=step('new-dialog','contact-open',index=0);assert boris['page']=='chatPage' and boris['message_count']==0 and boris['photo_count']>=1
    step('new-dialog-text','compose',text='Привет')
    first=step('first-message','send');assert first['message_count']==1 and first['draft']==''
    step('back-new-contacts','back');step('back-new-root','back')
    step('channel-tab','chat-tab',index=1)
    channel=step('channel-reading','demo-channel')
    assert channel['composer_visible'] is False
    assert channel['message_list_height']>=channel['content_height']-1
    assert channel['message_list_height']>=620
    step('back-channel','back')
    step('search-menu','search')
    found=step('search-anna','search-query',text='Анна');assert found['search_count']==1
    step('back-search','back')
    ready=step('ready','chat-tab',index=0);assert ready['chat_count']==3
    if restore_session:
        step('restore-existing-session','resume-saved',capture=False)
        for index in range(20):
            state=step('restoring-'+str(index),'session-state',capture=False)
            if state['phase'] not in ['starting','parameters','configuring']:break
            time.sleep(.5)
    step('checks-disabled','disable-checks',capture=False)
finally:
    path=WORK/'native-people-checks.json';path.write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
    client.close()
print('Native avatars, contacts, presence, typing expiry and read-only channel layout checked')
