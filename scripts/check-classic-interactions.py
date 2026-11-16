#!/usr/bin/env python3
"""Verify native chat tabs, offline search, exact hit navigation and composer on Classic."""
import json
import shlex
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/auto-scroll'
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
    personal=step('personal','demo-list');assert personal['chat_count']==2 and not personal['persistent_search_button']
    menu=step('main-menu','main-menu');assert menu['menu_state']!=0
    step('search-empty','search')
    step('messages-mode','search-mode',index=2)
    first=step('search-page-one','search-query',text='о')
    assert first['search_count']==8 and first['can_load_search']
    second=step('search-auto-page-two','scroll-bottom')
    assert second['search_count']>first['search_count']
    stable=step('search-no-loop','capture');assert stable['search_count']==second['search_count']
    step('search-specific','search-query',text='интерфейс')
    hit=step('message-hit','search-open');assert hit['target_index']>=0
    step('back-search','back');step('back-root','back')
    opened=step('chat','demo-chat',group=True)
    reply=step('reply-context','reply',message_id='7');assert reply['composer_mode']=='reply'
    step('reply-text','compose',text='Ответ с цитатой')
    replied=step('reply-sent','send');assert replied['message_count']==opened['message_count']+1
    assert replied['demo_messages'][-1]['replyQuote']
    step('ordinary-draft','compose',text='Черновик, сохранить')
    edited=step('edit-context','edit',message_id='8');assert edited['composer_mode']=='edit' and edited['draft']=='Ответ с цитатой'
    step('edit-text','compose',text='Изменённый ответ')
    saved=step('edit-saved','double-send');assert saved['message_count']==replied['message_count']
    assert saved['demo_messages'][-1]['text']=='Изменённый ответ' and saved['demo_messages'][-1]['edited']
    assert saved['draft']=='Черновик, сохранить'
    step('clear-draft','compose',text='')
    forwarded=step('forward-select','forward',message_id='7');assert forwarded['page']=='forwardPage' and forwarded['forward_state']=='ready'
    step('forward-recipient','forward-recipient',index=1)
    step('recipient-filter','recipient-query',text='Анна')
    sent=step('forward-sent','forward-confirm');assert sent['forward_state']=='sent'
    step('back-forward','back')
    repeated=step('forward-again','forward',message_id='7');assert repeated['page']=='forwardPage'
    step('cancel-repeated-forward','back')
    before_info=step('read-before-info','scroll-top');assert not before_info['history_at_end']
    info=step('chat-info','chat-info');assert info['page']=='chatInfoPage'
    muted=step('notifications-off','notifications');assert not muted['info']['notificationsOn']
    unmuted=step('notifications-on','notifications');assert unmuted['info']['notificationsOn']
    pinned=step('pinned','pin');assert pinned['info']['pinned']
    step('back-pinned-info','back');step('back-pinned-chat','back')
    pinned_list=step('pinned-list','chat-tab',index=1)
    assert pinned_list['chat_rows'][0]['chatId']=='demo-project' and pinned_list['chat_rows'][0]['pinned']
    step('open-pinned','demo-chat',group=True);step('pinned-info','chat-info')
    unpinned=step('unpinned','pin');assert not unpinned['info']['pinned']
    archived=step('archived','archive');assert archived['info']['archived']
    step('back-archived-info','back');step('back-archived-chat','back')
    archive_list=step('archived-list','archive-view');assert archive_list['chat_count']==1
    assert archive_list['chat_rows'][0]['chatId']=='demo-project' and archive_list['chat_rows'][0]['archived']
    step('open-archived','demo-chat',group=True);step('archived-info','chat-info')
    unarchived=step('unarchived','archive');assert not unarchived['info']['archived']
    step('back-unarchived-info','back');step('back-unarchived-chat','back');step('exit-archive','archive-view')
    step('resume-chat','demo-chat',group=True);step('resume-reading','scroll-top');step('resume-info','chat-info')
    members=step('members','info-tab',index=1);assert members['info_count']==2
    step('photos','info-tab',index=2);step('files','info-tab',index=3)
    step('incoming-during-info','incoming')
    back_info=step('back-info','back');assert back_info['pending_new_count']==1
    step('show-info-incoming','new-messages')
    top=step('reading-history','scroll-top');assert not top['history_at_end']
    incoming=step('new-incoming','incoming');assert incoming['pending_new_count']==1
    assert not incoming['history_at_end'] and incoming['first_visible']==top['first_visible']
    bottom=step('show-new','new-messages');assert bottom['pending_new_count']==0 and bottom['history_at_end']
    latest=step('incoming-at-bottom','incoming');assert latest['pending_new_count']==0 and latest['history_at_end']
    step('back-chat','back')
    archive=step('archive-view','archive-view');assert archive['chat_count']==0
    step('main-view','archive-view')
    ready=step('ready','chat-tab',index=0);assert ready['chat_count']==2
    step('checks-disabled','disable-checks')
finally:
    path=WORK/'native-interface-checks.json';path.write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
    client.close()
print('Native search menu, auto paging, reply/edit/forward, chat info, archive and incoming counter checked')
