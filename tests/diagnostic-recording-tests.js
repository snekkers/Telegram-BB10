/* Exercise microphone cancellation and explicit-send behavior with fake devices. */
'use strict';
const fs=require('node:fs'), vm=require('node:vm'), assert=require('node:assert/strict'), path=require('node:path');
const root=path.resolve(__dirname,'..'), elements=new Map(), timers=new Map();
const reportPath=path.join(root,'build/macos-development/recording-tests.json');
fs.rmSync(reportPath,{force:true});
let uploads=0, sends=0;
let nextTimer=1, permissionCalls=0, resolvePermission, rejectPermission, recorderCount=0;
function element(){return {value:'',hidden:false,disabled:false,textContent:'',firstChild:{textContent:''},
  className:'',scrollHeight:100,scrollTop:0,clientHeight:100,append(){},replaceChildren(){},setAttribute(){},
  removeAttribute(){},focus(){},click(){},remove(){}};}
const document={activeElement:null,getElementById(id){if(!elements.has(id))elements.set(id,element());return elements.get(id);},
  querySelectorAll(){return [];},createElement:element,addEventListener(){}};
const stream=()=>({stops:0,getTracks(){return [{stop:()=>{this.stops++;}}];}});
class Recorder {
  static isTypeSupported(mime){return mime.startsWith('audio/mp4');}
  constructor(){recorderCount++;this.state='inactive';}
  start(){this.state='recording';}
  stop(){this.state='inactive';queueMicrotask(()=>{
    this.ondataavailable({data:new Blob(['synthetic-audio'],{type:'audio/mp4'})});this.onstop();
  });}
}
const sandbox={document,MediaRecorder:Recorder,window:{MediaRecorder:Recorder},Blob,File,
  navigator:{mediaDevices:{getUserMedia(){permissionCalls++;return new Promise((resolve,reject)=>{resolvePermission=resolve;rejectPermission=reject;});}}},
  URL:{createObjectURL(){return 'blob:synthetic';},revokeObjectURL(){}},location:{hash:'#fixture',pathname:'/'},history:{replaceState(){}},
  sessionStorage:{setItem(){},getItem(){return 'fixture';}},fetch(url, options){
    if(url==='/api/state')return new Promise(()=>{});
    if(url==='/api/upload'){uploads++;return Promise.resolve({ok:true,json:async()=>({uploadId:'a'.repeat(32)})});}
    const command=JSON.parse(options.body);if(command.action==='sendAttachment'||command.action==='sendText')sends++;
    return Promise.resolve({ok:true,json:async()=>({ok:true})});
  },
  setTimeout(callback,delay){const id=nextTimer++;timers.set(id,{callback,delay});return id;},clearTimeout(id){timers.delete(id);}};
vm.createContext(sandbox);vm.runInContext(fs.readFileSync(path.join(root,'diagnostic/web/app.js'),'utf8'),sandbox);
const run=code=>vm.runInContext(code,sandbox), record=()=>elements.get('record').onclick();
const tick=()=>new Promise(resolve=>setImmediate(resolve));
function chat(id='1') {sandbox.nextState={phase:'ready',ready:true,available:true,chatId:id,canSend:true,canSendDocument:true,
  canSendPhoto:true,canSendVoice:true,chats:[],messages:[],failedAttachment:{}};run('render(nextState)');}
async function main(){
  chat();
  let starting=record();assert.equal(elements.get('record').disabled,true);await record();assert.equal(permissionCalls,1);
  chat('2');chat('1');const cancelled=stream();resolvePermission(cancelled);await starting;
  assert.equal(cancelled.stops,1);assert.equal(recorderCount,0);assert.equal(run('attachment'),null);
  starting=record();const active=stream();resolvePermission(active);await starting;
  assert.equal(run('recorder.state'),'recording');assert.equal(elements.get('send').disabled,true);
  await record();await tick();
  assert.ok(active.stops>0);assert.equal(run('attachment.kind'),'voice');assert.equal(run('attachment.duration'),1);
  assert.equal(run('attachment.uploadId'),'a'.repeat(32));assert.equal(uploads,1);assert.equal(sends,0); // Only a private local copy is created.
  await elements.get('cancel-attachment').onclick();assert.equal(run('attachment'),null);
  starting=record();const interrupted=stream();resolvePermission(interrupted);await starting;
  chat('2');await tick();assert.ok(interrupted.stops>0);assert.equal(run('attachment'),null);
  assert.equal(run('recorder'),null);assert.ok(!Array.from(timers.values()).some(timer=>timer.delay===300000));
  chat('1');starting=record();rejectPermission(new Error('microphone denied'));await starting;chat('1');
  assert.equal(run('recordingStarting'),false);assert.equal(run('recorder'),null);assert.equal(elements.get('record').disabled,false);
  assert.match(elements.get('local-error').textContent,/microphone denied/);
  const report={passed:true,hardware_microphone_used:false,checks:['single pending permission request',
    'late permission after chat change is cancelled, including return to same chat','stop prepares preview without sending',
    'chat change stops active microphone and discards recording','duration timer cancelled','denied permission releases controls']};
  fs.writeFileSync(reportPath,JSON.stringify(report,null,2)+'\n');
  process.stdout.write(JSON.stringify(report,null,2)+'\n');
}
main().catch(error=>{process.stderr.write(error.stack+'\n');process.exitCode=1;});
