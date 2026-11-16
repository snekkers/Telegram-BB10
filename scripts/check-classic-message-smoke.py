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
 cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::message-smoke')+' > /pps/services/launcher/control')
 time.sleep(3)
 step('message-session','session-state');step('message-stop','stop-session');time.sleep(2)
 try:step('message-list','demo-list')
 except AssertionError:
  step('message-login-back','back');step('message-list-again','demo-list')
 step('message-chat','demo-chat');step('message-stress','stress-demo');memory('messages-loaded')
 step('message-compose','compose',text='Проверка ввода без отправки')
 step('message-scroll-top','scroll-top');step('message-scroll-bottom','scroll-bottom')
 step('message-back','back');step('message-chat-again','demo-chat');step('message-home','back')
 step('message-disable','disable-checks')
 (WORK/'message-smoke.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
finally:
 try:
  with c.open_sftp() as s:
   try:s.remove(folder+'/enabled')
   except FileNotFoundError:pass
  rows=cmd('pidin arguments').decode().splitlines()
  for row in rows:
   fields=row.split(None,1)
   if len(fields)==2 and fields[0].isdigit() and fields[1].split()[0]==installed['app_id']:cmd('kill -TERM '+fields[0])
  time.sleep(2)
  cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::telegram-ready')+' > /pps/services/launcher/control')
 finally:c.close()
