#!/usr/bin/env python3
import sys,json,time,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/reading';sys.path.insert(0,str(BASE/'python'))
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

def memory(label):
 rows=cmd('pidin arguments').decode().splitlines();ids=[r.split(None,1)[0] for r in rows if len(r.split(None,1))==2 and r.split(None,1)[1].split()[0]==installed['app_id']]
 assert ids;data=cmd('pidin -p '+ids[0]+' mem').decode();(WORK/(label+'-memory.txt')).write_text(data);print(label,data.splitlines()[1],flush=True)
try:
 with c.open_sftp() as s:
  path=folder+'/enabled'
  with s.open(path,'wb') as f:f.write(b'offline-ui-checks\n')
  s.chown(path,installed['uid'],installed['gid']);s.chmod(path,0o600)
 cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::photo-cycles')+' > /pps/services/launcher/control');time.sleep(3)
 state=step('photos-session','session-state');step('photos-stop','stop-session');time.sleep(2);step('photos-list','demo-list');step('photos-chat','demo-chat')
 memory('photos-start')
 for i in range(10):
  opened=step('photos-open-'+str(i),'photo-demo');assert opened['photo_loaded']
  step('photos-next-'+str(i),'photo-next');step('photos-back-'+str(i),'back')
 memory('photos-end');step('photos-home','back')
 if state.get('has_saved_api'):step('photos-resume','resume-saved')
 step('photos-disable','disable-checks');(WORK/'photo-cycles.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
finally:
 try:
  rows=cmd('pidin arguments').decode().splitlines()
  for r in rows:
   f=r.split(None,1)
   if len(f)==2 and f[0].isdigit() and f[1].split()[0]==installed['app_id']:cmd('kill -TERM '+f[0])
  time.sleep(1)
  with c.open_sftp() as s:
   try:s.remove(folder+'/enabled')
   except FileNotFoundError:pass
  cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::berrygram-ready')+' > /pps/services/launcher/control')
 finally:c.close()
