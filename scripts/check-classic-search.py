#!/usr/bin/env python3
"""Verify native chat tabs, offline search, exact hit navigation and composer on Classic."""
import json
import shlex
import sys
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/search-tabs'
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
    personal=step('personal','demo-list')
    assert personal['chat_count']==2 and personal['category']=='personal'
    groups=step('communities','chat-tab',index=1)
    assert groups['chat_count']==3 and groups['category']=='community'
    personal=step('personal-again','chat-tab',index=0);assert personal['chat_count']==2
    step('search-empty','search')
    found=step('search-chats','search-query',text='Анна')
    assert found['search_count']==1 and found['search_chat_ids']==['demo-anna']
    global_result=step('search-global','search-mode',index=1)
    assert global_result['search_count']==1 and global_result['search_mode']==1
    step('search-messages-mode','search-mode',index=2)
    messages=step('search-messages','search-query',text='интерфейс')
    assert messages['search_count']>=5
    hit=step('message-hit','search-open')
    assert hit['page']=='chatPage' and hit['target_message'] and hit['target_index']>=0
    back=step('back-to-results','back')
    assert back['page']=='searchPage' and back['search_count']==messages['search_count']
    recent=step('search-recent','search-query',text='')
    assert recent['search_count']>=1
    photo=step('search-photos','search-filter',index=1)
    assert photo['search_filter']==1 and photo['search_count']==0
    files=step('search-files','search-filter',index=2);assert files['search_count']==0
    links=step('search-links','search-filter',index=3);assert links['search_count']==0
    step('search-all','search-filter',index=0)
    missing=step('search-missing','search-query',text='несуществующий запрос')
    assert missing['search_count']==0
    step('back-to-tabs','back')
    step('search-return','search')
    recent=step('recent-visible','capture');assert recent['search_count']>=1
    step('back-for-composer','back')
    empty=step('composer','demo-chat')
    assert empty['multiline'] and empty['max_lines']==4 and empty['action_bar_hidden']
    first=step('single-line','compose',text='Первая строка')
    newline=step('newline','newline');assert newline['draft']=='Первая строка\n'
    multiple=step('four-lines','compose',text='Раз\nДва\nТри\nЧетыре\nПять')
    assert multiple['composer_height']>first['composer_height']
    sent=step('sent','send');assert sent['draft']=='' and sent['message_count']==empty['message_count']+1
    step('back-ready','back')
    ready=step('ready','chat-tab',index=0);assert ready['chat_count']==2
    step('checks-disabled','disable-checks')
finally:
    path=WORK/'native-interface-checks.json';path.write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
    client.close()
print('Native tabs, offline search, message jump, filters and composer checked')
