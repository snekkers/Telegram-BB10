#!/usr/bin/env python3
"""Update Berrygram on this workspace's already paired Classic; preserve app data."""
import hashlib
import json
import sys
import time
import uuid
import zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/root-preparation';WORK=ROOT/'build/people'
sys.path.insert(0,str(BASE/'python'))
import paramiko
bar=ROOT/'build/device/berrygram-device.bar'
with zipfile.ZipFile(bar) as archive:
    manifest=archive.read('META-INF/MANIFEST.MF').decode()
    fields=dict(line.split(': ',1) for line in manifest.splitlines() if ': ' in line)
    assert fields['Package-Name']=='org.example.berrygram'
    assert fields['Package-Architecture']=='armle-v7'
    assert fields['Package-Version']=='0.13.0.15'
    app_id=fields['Package-Name']+'.'+fields['Package-Id']
    expected={name:hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist()
        if name.startswith('native/') and not name.endswith('/')}
verification=json.loads((ROOT/'build/verification.json').read_text())
digest=hashlib.sha256(bar.read_bytes()).hexdigest()
assert next(x for x in verification['artifacts'] if x['target']=='device')['sha256']==digest
client=paramiko.SSHClient();client.load_host_keys(str(BASE/'ssh/known_hosts'))
client.set_missing_host_key_policy(paramiko.RejectPolicy())
client.connect('169.254.0.1',username='root',pkey=paramiko.RSAKey.from_private_key_file(str(BASE/'ssh/classic_rsa')),
    look_for_keys=False,allow_agent=False,timeout=8,
    disabled_algorithms={'pubkeys':['rsa-sha2-512','rsa-sha2-256']})
def command(value):
    i,o,e=client.exec_command(value,timeout=30)
    data=o.read();errors=e.read();code=o.channel.recv_exit_status();i.close();o.close();e.close()
    assert code==0,errors.decode(errors='replace')[:200]
    return data
def app_pids():
    result=[]
    for row in command('pidin arguments').decode().splitlines():
        f=row.split(None,1)
        if len(f)==2 and f[0].isdigit() and f[1].split()[0]==app_id:result.append(int(f[0]))
    return result
try:
    for pid in app_pids():command('kill -TERM '+str(pid))
    deadline=time.monotonic()+15
    while app_pids() and time.monotonic()<deadline:time.sleep(.5)
    assert not app_pids(),'Berrygram did not stop for the update'
    with client.open_sftp() as s:
        remote='/var/tmp/berrygram-ui-update.bar'
        s.put(str(bar),remote);s.chmod(remote,0o644)
        job='/pps/system/installer/upd/current/job.berrygram-ui-'+uuid.uuid4().hex
        with s.open(job,'wb') as f:
            s.chmod(job,0o666)
            f.write(('action::install\npackage_location::'+remote+'\nextras::\n').encode())
        deadline=time.monotonic()+600
        while time.monotonic()<deadline:
            with s.open(job,'rb') as f:status=f.read().decode()
            if 'result::success\n' in status:break
            assert not any(line.startswith('result::') for line in status.splitlines()),'Installer returned an error'
            time.sleep(1)
        assert 'result::success\n' in status and 'progress::100\n' in status
        assert 'actual_app_version::'+fields['Package-Version']+'\n' in status
        appdir='/apps/'+app_id;appdata='/accounts/1000/appdata/'+app_id
        for name,wanted in expected.items():
            with s.open(appdir+'/'+name,'rb') as f:
                f.prefetch();h=hashlib.sha256()
                while True:
                    block=f.read(262144)
                    if not block:break
                    h.update(block)
            assert h.hexdigest()==wanted,name
        uid=s.stat('/accounts/devuser').st_uid;gid=s.stat(appdir).st_gid
        assert uid==100
        qa=appdata+'/data/ui-check'
        if '--qa' in sys.argv:
            for directory in [qa,qa+'/reference']:
                try:s.mkdir(directory,0o700)
                except OSError:s.stat(directory)
                s.chown(directory,uid,gid);s.chmod(directory,0o700)
            for name in ['ChatPage.qml','AppHeader.qml','Ui.js']:
                path=qa+'/reference/'+name
                with s.open(path,'wb') as f:f.write((ROOT/'build/ui-refresh/reference'/name).read_bytes())
                s.chown(path,uid,gid);s.chmod(path,0o600)
            with s.open(qa+'/enabled','wb') as f:f.write(b'offline-ui-checks\n')
            s.chown(qa+'/enabled',uid,gid);s.chmod(qa+'/enabled',0o600)
        report={'app_id':app_id,'version':fields['Package-Version'],'app_directory':appdir,'app_data':appdata,
            'qa_directory':qa,'uid':uid,'gid':gid,'installed_files':expected,'bar_sha256':digest,
            'data_preserved':True,'telegram_login_tested':False}
        (WORK/'installation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Installed Berrygram',fields['Package-Version'],'and verified',len(expected),'native files; app data preserved')
finally:client.close()
