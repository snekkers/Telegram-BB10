#!/usr/bin/env python3
"""Check offline emoji, photos and selected-message actions on the paired Classic."""
import sys,json,time,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/productivity';WORK.mkdir(parents=True,exist_ok=True);sys.path.insert(0,str(BASE/'python'))
import paramiko
installed=json.loads((ROOT/'build/people/installation.json').read_text())
c=paramiko.SSHClient();c.load_host_keys(str(BASE/'ssh/known_hosts'));c.set_missing_host_key_policy(paramiko.RejectPolicy())
c.connect('169.254.0.1',username='root',pkey=paramiko.RSAKey.from_private_key_file(str(BASE/'ssh/classic_rsa')),look_for_keys=False,allow_agent=False,timeout=8,disabled_algorithms={'pubkeys':['rsa-sha2-512','rsa-sha2-256']})
def cmd(text):
 i,o,e=c.exec_command(text,timeout=20);data=o.read();assert o.channel.recv_exit_status()==0;e.close();o.close();i.close();return data
folder=installed['qa_directory'];results=[]
def step(name,action,**args):
 with c.open_sftp() as s:
  p=folder+'/request.json'
  with s.open(p+'.new','wb') as f:f.write(json.dumps(dict(args,id=name,action=action),ensure_ascii=False).encode())
  s.chown(p+'.new',installed['uid'],installed['gid']);s.chmod(p+'.new',0o600);s.posix_rename(p+'.new',p)
  deadline=time.monotonic()+25
  while time.monotonic()<deadline:
   try:
    with s.open(folder+'/result.json','rb') as f:r=json.loads(f.read())
   except FileNotFoundError:r={}
   if r.get('id')==name:
    assert r.get('ok'),r;results.append(r)
    if r.get('capture_ok'):s.get(folder+'/'+name+'.png',str(WORK/(name+'.png')))
    print(name,{k:r.get(k) for k in ['page','composer_focused','composer_enabled','capture_ok']},flush=True);return r
   time.sleep(.3)
  raise RuntimeError('No result for '+name)
def memory(name):
 rows=cmd('pidin arguments').decode().splitlines()
 ids=[r.split(None,1)[0] for r in rows if len(r.split(None,1))==2 and r.split(None,1)[1].split()[0]==installed['app_id']]
 assert ids
 data=cmd('pidin -p '+ids[0]+' mem').decode();(WORK/(name+'-memory.txt')).write_text(data)
 print(name,data.splitlines()[1],flush=True)
try:
 cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::productivity-check')+' > /pps/services/launcher/control')
 time.sleep(3)
 state=step('features-session','session-state');step('features-stop','stop-session');time.sleep(2)
 step('features-list','demo-list');memory('cold')
 step('features-notice','notification-test');step('features-notice-clear','notification-clear')
 step('features-chat','demo-chat');step('features-compose','compose',text='Привет ')
 reactions=step('features-reactions','feature-preview',feature='reactions');assert reactions['page']=='reactionPage'
 step('features-reactions-back','back')
 schedule=step('features-schedule','feature-preview',feature='schedule');assert schedule['page']=='schedulePage'
 step('features-schedule-back','back')
 picker=step('features-picker','emoji-picker');assert picker['page']=='emojiPage' and picker['emoji_count']==30
 picked=step('features-picked','emoji-pick',index=0);assert picked['page']=='chatPage' and picked['composer_focused'] and len(picked['draft'])>len('Привет ')
 step('features-picker-recent','emoji-picker');recent=step('features-recents','emoji-category',index=0);assert recent['emoji_count']>=1
 step('features-picker-back','back')
 photo=step('features-photo','photo-demo');assert photo['page']=='photoPage' and photo['photo_loaded']
 step('features-photo-zoom','photo-zoom');next=step('features-photo-next','photo-next');assert next['photo_position'].startswith('2 ')
 step('features-photo-previous','photo-previous');memory('photo')
 with c.open_sftp() as s:
  directory='/accounts/1000/shared/photos/Berrygram'
  try:before=set(s.listdir(directory))
  except FileNotFoundError:before=set()
 step('features-photo-save','photo-save')
 with c.open_sftp() as s:
  created=set(s.listdir(directory))-before;assert len(created)==1
  for name in created:
   assert s.stat(directory+'/'+name).st_size>1000
   s.remove(directory+'/'+name)
 step('features-photo-back','back')
 initial=step('features-select-one','select-message',message_id='1');selected=step('features-select-two','select-message',message_id='2');assert selected['selection_count']==2
 prepared=step('features-delete-prepare','selected-delete');assert prepared['selection_count']==2 and prepared['selection_state']['state']=='ready'
 deleted=step('features-delete-confirm','selected-confirm-delete');assert deleted['selection_count']==0 and deleted['message_count']==initial['message_count']-2
 step('features-select-three','select-message',message_id='3');step('features-select-four','select-message',message_id='4')
 forward=step('features-forward','selected-forward');assert forward['page']=='forwardPage'
 step('features-recipient','forward-recipient',index=0);step('features-forward-confirm','forward-confirm');step('features-forward-back','back');step('features-selection-clear','selected-cancel')
 for i in range(3):
  step('features-cycle-picker-'+str(i),'emoji-picker');step('features-cycle-back-'+str(i),'back')
  step('features-cycle-photo-'+str(i),'photo-demo');step('features-cycle-photo-back-'+str(i),'back')
 step('features-back','back');memory('after-cycles')
 if state.get('has_saved_api'):step('features-resume','resume-saved')
 step('features-disable','disable-checks')
 (WORK/'device-checks.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
finally:c.close()
