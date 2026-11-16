#!/usr/bin/env python3
import sys,json,time,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/reading';WORK.mkdir(parents=True,exist_ok=True);sys.path.insert(0,str(BASE/'python'))
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

prefs=installed['app_data']+'/data/settings/preferences.json'
with c.open_sftp() as s:
 try:
  with s.open(prefs,'rb') as f:original=f.read()
 except FileNotFoundError:original=None
try:
 cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::reading-check')+' > /pps/services/launcher/control')
 time.sleep(3);state=step('reading-session','session-state');step('reading-stop','stop-session');time.sleep(2)
 step('reading-list','demo-list');settings=step('reading-settings','preferences');assert settings['page']=='settingsPage'
 dark=step('reading-dark','settings-toggle',field='dark',value=True);assert dark['dark']
 with c.open_sftp() as s:
  with s.open(prefs,'rb') as f:assert json.loads(f.read()).get('dark') is True
 rows=cmd('pidin arguments').decode().splitlines()
 for r in rows:
  f=r.split(None,1)
  if len(f)==2 and f[0].isdigit() and f[1].split()[0]==installed['app_id']:cmd('kill -TERM '+f[0])
 time.sleep(2);cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::reading-restart')+' > /pps/services/launcher/control');time.sleep(3)
 step('reading-restart-stop','stop-session');time.sleep(2);step('reading-restart-list','demo-list');again=step('reading-restart-settings','preferences');assert again['dark']
 step('reading-large','settings-toggle',field='font',value=2)
 step('reading-enter-off','settings-toggle',field='enter',value=False)
 step('reading-hide-preview','settings-toggle',field='preview',value=True)
 step('reading-settings-back','back');chat=step('reading-dark-chat','demo-chat');assert chat['page']=='chatPage'
 rich=step('reading-rich','rich-demo');assert rich['message_count']>=8
 jump=step('reading-quote-jump','reply-jump',message_id='7');assert jump['target_message']=='7' and jump['target_index']>=0
 step('reading-bottom','scroll-bottom');stress=step('reading-stress','stress-demo');assert stress['message_count']>=308
 step('reading-stress-top','scroll-top');step('reading-stress-bottom','scroll-bottom')
 step('reading-back','back');step('reading-settings-again','preferences')
 step('reading-dark-off','settings-toggle',field='dark',value=False)
 step('reading-enter-on','settings-toggle',field='enter',value=True)
 step('reading-preview-off','settings-toggle',field='preview',value=False)
 step('reading-font-normal','settings-toggle',field='font',value=1)
 step('reading-settings-close','back')
 if state.get('has_saved_api'):step('reading-resume','resume-saved')
 step('reading-disable','disable-checks')
 (WORK/'device-checks.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
finally:
 # Stop the test instance before restoring the user's original preferences.
 try:
  rows=cmd('pidin arguments').decode().splitlines()
  for r in rows:
   f=r.split(None,1)
   if len(f)==2 and f[0].isdigit() and f[1].split()[0]==installed['app_id']:cmd('kill -TERM '+f[0])
  time.sleep(1)
  with c.open_sftp() as s:
   try:s.remove(folder+'/enabled')
   except FileNotFoundError:pass
   if original is None:
    try:s.remove(prefs)
    except FileNotFoundError:pass
   else:
    with s.open(prefs,'wb') as f:f.write(original)
    s.chown(prefs,installed['uid'],installed['gid']);s.chmod(prefs,0o600)
  cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::berrygram-reading-ready')+' > /pps/services/launcher/control')
 finally:c.close()
