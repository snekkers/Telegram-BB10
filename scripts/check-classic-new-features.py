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
 cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::new-features')+' > /pps/services/launcher/control');time.sleep(3)
 step('new-session','session-state');step('new-stop','stop-session');time.sleep(2)
 try:step('new-list','demo-list')
 except AssertionError:
  step('new-login-back','back');step('new-list-again','demo-list')
 step('new-chat','demo-chat')
 for feature,expected in [('diagnostics','diagnosticsPage'),('downloads','downloadsPage'),('mute','mutePage'),('scheduled','scheduledMessagesPage'),('scheduled-editor','scheduledEditorPage')]:
  result=step('new-'+feature,'feature-preview',feature=feature);assert result['page']==expected
  step('new-'+feature+'-back','back')
 opened=step('new-voice','feature-preview',feature='voice');assert opened['page']=='voicePage'
 for i in range(12):
  if not opened.get('voice_loading') and opened.get('voice_duration',0)>59000:break
  opened=step('new-voice-wait-'+str(i),'capture')
 assert opened.get('voice_playing'),{k:v for k,v in opened.items() if k.startswith('voice')}
 assert opened['voice_duration']>59000
 paused=step('new-voice-pause','voice-action',command='toggle');assert not paused['voice_playing']
 seeked=step('new-voice-seek','voice-action',command='seek',value=20000);assert abs(seeked['voice_position']-20000)<2000
 rate=step('new-voice-rate15','voice-action',command='speed',value=1.5);assert rate['voice_speed']==1.5
 rate=step('new-voice-rate2','voice-action',command='speed',value=2);assert rate['voice_speed']==2
 resumed=step('new-voice-resume','voice-action',command='toggle');assert resumed['voice_playing']
 time.sleep(2);progress=step('new-voice-progress','capture');assert progress['voice_position']>resumed['voice_position']+2000
 step('new-voice-back','back')
 restored=step('new-voice-reopen','feature-preview',feature='voice')
 for i in range(12):
  if not restored.get('voice_loading') and restored.get('voice_duration',0)>59000:break
  restored=step('new-voice-restore-wait-'+str(i),'capture')
 for i in range(3):
  if restored.get('voice_playing') and restored.get('voice_position',0)>=progress['voice_position']-1000:break
  restored=step('new-voice-restored-position-'+str(i),'capture')
 assert restored.get('voice_playing') and restored.get('voice_position',0)>=progress['voice_position']-1000,{k:v for k,v in restored.items() if k.startswith('voice')}
 step('new-voice-close','back');step('new-home','back');step('new-disable','disable-checks')
 (WORK/'new-features-device.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
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
  cmd('print '+shlex.quote('msg::start\ndat::'+installed['app_id']+',ORIENTATION=0\nid::telegram-normal')+' > /pps/services/launcher/control')
 finally:c.close()
