"use strict";
const $ = id => document.getElementById(id);
const initialToken = location.hash.slice(1);
if(initialToken)sessionStorage.setItem("berrygram-local-token",initialToken);
const token = initialToken || sessionStorage.getItem("berrygram-local-token") || "";
history.replaceState(null, "", location.pathname);
let state = {}, previousPhase = "", chatId = "", pendingChat = null, messagesSignature = "", draftTimer, queue = Promise.resolve();
let mediaUrls=[], attachment=null, attachmentUrl="", recorder=null, recordingStream=null, recordingTimer=null, sendingAttachment=false, recordingStarting=false, recordingGeneration=0;
const authHeaders = {"Authorization": "Bearer " + token};
const labels = {phone:"Номер телефона с кодом страны", code:"Код входа", password:"Пароль двухэтапной проверки", email:"Адрес почты", emailCode:"Код из письма"};
function command(action, extra={}) {
  const task = queue.then(async () => {
    const response = await fetch("/api/command", {method:"POST",headers:{...authHeaders,"Content-Type":"application/json"},body:JSON.stringify({action,...extra})});
    if (!response.ok) throw new Error("Команда не выполнена. Проверьте состояние клиента; текст остаётся в поле.");
    $("local-error").textContent="";
    return true;
  });
  queue = task.catch(error => {$("local-error").textContent=error.message;});
  return task.catch(() => false);
}
function flushDraft() { clearTimeout(draftTimer); return chatId ? command("draft", {chatId,text:$("draft").value}) : Promise.resolve(true); }
async function openChat(id) {
  stopRecording(true);await flushDraft(); pendingChat=id; $("send").disabled=true; $("draft").disabled=true;
  if (!await command("openChat",{chatId:id})) pendingChat=null;
}
$("start").onclick=()=>command(state.testFixture?"demoLogin":"start");
$("stop").onclick=async()=>{await flushDraft(); command("stop");};
$("saved").onclick=()=>command("useSaved");
$("forget").onclick=()=>command("forgetApi");
$("api-form").onsubmit=async event=>{
  event.preventDefault();
  if(await command("configure",{apiId:$("api-id").value,apiHash:$("api-hash").value,remember:$("remember").checked})) $("api-hash").value="";
};
$("credential-form").onsubmit=async event=>{
  event.preventDefault(); if(await command("submit",{value:$("credential").value})) $("credential").value="";
};
function canCompose() { return attachment ? !!state[{document:"canSendDocument",photo:"canSendPhoto",voice:"canSendVoice"}[attachment.kind]] : !!state.canSend; }
function updateSend() { $("send").disabled=!canCompose()||!!pendingChat||sendingAttachment||recordingStarting||!!recorder||(!attachment&&!$("draft").value.trim()); }
function clearAttachment() {
  if(attachmentUrl)URL.revokeObjectURL(attachmentUrl);attachmentUrl="";attachment=null;
  $("attachment").hidden=true;$("attachment-image").hidden=true;$("attachment-audio").hidden=true;$("attachment-audio").removeAttribute("src");
  $("file").value="";$("draft").maxLength=4096;updateSend();
}
async function chooseAttachment(file, kind="document", duration=0) {
  clearAttachment();
  if(file.size<=0 || file.size>50*1024*1024)throw new Error("Выберите непустой файл до 50 МБ.");
  attachment={file,kind,duration,chatId,name:file.name,width:0,height:0};
  attachmentUrl=URL.createObjectURL(file);
  if(file.type==="image/jpeg" || file.type==="image/png") {
    const image=new Image();image.src=attachmentUrl;await image.decode();
    if(!attachment || attachment.file!==file)return;
    attachment.width=image.naturalWidth;attachment.height=image.naturalHeight;
    $("attachment-image").src=attachmentUrl;$("attachment-image").hidden=false;
  }
  if(file.type.startsWith("audio/")){$("attachment-audio").src=attachmentUrl;$("attachment-audio").hidden=false;}
  $("attachment-name").textContent=file.name+" · "+Math.ceil(file.size/1024)+" КБ";
  $("attachment-kind").value=kind;$("attachment").hidden=false;$("draft").maxLength=1024;updateSend();
}
$("attach").onclick=()=>$("file").click();
$("file").onchange=async()=>{const file=$("file").files[0];if(file)try{await chooseAttachment(file);}catch(error){clearAttachment();$("local-error").textContent=error.message;}};
$("attachment-kind").onchange=()=>{if(attachment){attachment.kind=$("attachment-kind").value;updateSend();}};
$("attachment-audio").onloadedmetadata=()=>{if(attachment && !attachment.duration && Number.isFinite($("attachment-audio").duration))attachment.duration=Math.ceil($("attachment-audio").duration);};
$("cancel-attachment").onclick=()=>{if(attachment?.uploadId)command("discardUpload",{chatId:attachment.chatId,uploadId:attachment.uploadId});clearAttachment();};
$("recover-attachment").onclick=()=>{
  const failed=state.failedAttachment;if(!failed?.uploadId)return;
  clearAttachment();attachment={...failed,chatId, name:"Неотправленное вложение"};
  $("attachment-name").textContent=attachment.name;$("attachment-kind").value=attachment.kind;$("attachment").hidden=false;
  $("draft").value=failed.caption||"";$("draft").maxLength=1024;updateSend();
};
function stopRecording(cancel=false) {
  if(cancel){recordingGeneration++;recordingStarting=false;}
  if(recorder?.state==="recording")recorder.stop();
  if(recordingStream)recordingStream.getTracks().forEach(track=>track.stop());
  clearTimeout(recordingTimer);
}
$("record").onclick=async()=>{
  if(recorder){stopRecording();return;}
  if(recordingStarting)return;
  const destination=chatId, generation=++recordingGeneration;
  recordingStarting=true;updateSend();$("record").disabled=true;
  try {
    const mime=["audio/mp4;codecs=mp4a.40.2","audio/ogg;codecs=opus"].find(type=>window.MediaRecorder?.isTypeSupported(type));
    if(!mime)throw new Error("Этот браузер не записывает M4A или Ogg Opus. Можно прикрепить готовое голосовое в M4A, MP3 или Ogg Opus.");
    const stream=await navigator.mediaDevices.getUserMedia({audio:{channelCount:1},video:false});
    if(chatId!==destination||generation!==recordingGeneration){stream.getTracks().forEach(track=>track.stop());return;}
    recordingStream=stream;recordingStarting=false;
    const chunks=[], activeRecorder=new MediaRecorder(stream,{mimeType:mime});recorder=activeRecorder;const started=Date.now();
    activeRecorder.ondataavailable=event=>{if(event.data.size)chunks.push(event.data);};
    activeRecorder.onerror=()=>{stopRecording(true);$("local-error").textContent="Не удалось записать голос.";};
    activeRecorder.onstop=async()=>{
      stream.getTracks().forEach(track=>track.stop());
      const duration=Math.min(300,Math.max(1,Math.ceil((Date.now()-started)/1000)));
      if(recorder===activeRecorder){recorder=null;recordingStream=null;$("record").textContent="Записать голос";$("record-status").textContent="";updateSend();}
      if(chatId!==destination||generation!==recordingGeneration||!chunks.length)return;
      try{await chooseAttachment(new File(chunks,["voice",mime.startsWith("audio/mp4")?"m4a":"ogg"].join("."),{type:mime.split(";")[0]}),"voice",duration);}catch(error){$("local-error").textContent=error.message;}
    };
    activeRecorder.start();recordingTimer=setTimeout(()=>stopRecording(),300000);$("record").disabled=false;
    $("record").textContent="Завершить запись";$("record-status").textContent="Идёт запись · до 5 минут";updateSend();
  } catch(error){if(generation===recordingGeneration){stopRecording(true);recorder=null;$("local-error").textContent=error.message;updateSend();}}
  finally{if(generation===recordingGeneration)recordingStarting=false;}
};
$("send-form").onsubmit=async event=>{
  event.preventDefault(); clearTimeout(draftTimer);
  const text=$("draft").value, destination=chatId, selected=attachment;
  if(!canCompose()||pendingChat||sendingAttachment||recordingStarting||recorder||(!selected&&!text.trim()))return;
  let accepted=false;
  if(selected) {
    sendingAttachment=true;updateSend();
    try {
      if(selected.chatId!==destination)throw new Error("Заново выберите вложение для этого чата.");
      if(!selected.uploadId){
        const response=await fetch("/api/upload",{method:"POST",headers:{...authHeaders,"Content-Type":"application/octet-stream","X-File-Name":encodeURIComponent(selected.name),"X-Chat-Id":destination},body:selected.file});
        if(!response.ok)throw new Error("Не удалось подготовить вложение. Проверьте размер файла и свободное место.");
        selected.uploadId=(await response.json()).uploadId;
      }
      if(chatId!==destination)return;
      accepted=await command("sendAttachment",{chatId:destination,uploadId:selected.uploadId,kind:selected.kind,caption:text,duration:selected.duration,width:selected.width,height:selected.height});
      if(accepted&&attachment===selected)clearAttachment();
    } catch(error){$("local-error").textContent=error.message;}
    finally{sendingAttachment=false;updateSend();}
  } else accepted=await command("sendText",{chatId:destination,text});
  if(accepted&&chatId===destination&&$("draft").value===text)$("draft").value="";
  updateSend();
};
$("draft").oninput=()=>{clearTimeout(draftTimer); draftTimer=setTimeout(flushDraft,350); updateSend();};
$("draft").onkeydown=event=>{if(event.key==="Enter" && !event.shiftKey && !event.isComposing){event.preventDefault();$("send-form").requestSubmit();}};
$("search").oninput=()=>command("search",{text:$("search").value});
$("more-chats").onclick=()=>command("loadChats");
$("history").onclick=()=>command("loadHistory");
$("mark-read").onclick=()=>command("markRead",{chatId});
$("recover").onclick=()=>{if(!$("draft").value)command("recoverDraft");};
$("next-unread").onclick=async()=>{stopRecording(true);await flushDraft();command("nextUnread");};
document.addEventListener("keydown",event=>{if((event.metaKey||event.ctrlKey)&&event.key.toLowerCase()==="f"&&state.ready){event.preventDefault();$("search").focus();}});
function render(next) {
  state=next;
  $("fixture").hidden=!state.testFixture;
  const phase=state.phase||"stopped", ready=!!state.ready, changedPhase=phase!==previousPhase;
  $("auth").hidden=ready; $("client").hidden=!ready;
  $("status").textContent=state.status||""; $("auth-error").textContent=state.authError||"";
  $("version").textContent=state.version||""; $("connection").textContent=state.testFixture?"Демо · без соединения":ready?state.connection||"":state.status||"";
  $("start").hidden=!["stopped","failed"].includes(phase); $("start").disabled=!state.available||state.busy;
  $("stop").hidden=["stopped","failed"].includes(phase); $("stop").disabled=phase==="closing";
  $("api-form").hidden=phase!=="parameters"||state.testFixture; $("saved").hidden=!state.hasSavedApi; $("saved").disabled=state.busy;
  $("start").textContent=state.testFixture?"Открыть демо":"Запустить движок";
  if(state.testFixture&&phase==="parameters")$("start").hidden=false;
  $("forget").hidden=!state.hasSavedApi; $("credential-form").hidden=!labels[phase];
  if (changedPhase) {$("credential").value="";$("api-hash").value=""; if(state.savedApiId)$("api-id").value=state.savedApiId;}
  if (labels[phase]) {
    $("credential-label").firstChild.textContent=labels[phase];
    $("credential").type=phase==="phone"?"tel":phase==="email"?"email":"password";
  }
  for(const element of document.querySelectorAll("#api-form input,#api-form button,#credential-form input,#credential-form button"))element.disabled=!!state.busy;
  if (state.chatId!==chatId) {stopRecording(true);clearAttachment();clearTimeout(draftTimer);chatId=state.chatId||"";$("draft").value=state.draft||"";messagesSignature="";}
  else if(document.activeElement!==$("draft") && state.draft && !$("draft").value)$("draft").value=state.draft;
  if (pendingChat===chatId)pendingChat=null;
  $("draft").disabled=!canCompose()||!!pendingChat;updateSend();
  $("attach").disabled=(!state.canSendDocument&&!state.canSendPhoto&&!state.canSendVoice)||!!pendingChat||sendingAttachment||recordingStarting||!!recorder;
  $("record").disabled=!state.canSendVoice||!!pendingChat||sendingAttachment||recordingStarting;
  $("attachment-kind").disabled=sendingAttachment;$("cancel-attachment").disabled=sendingAttachment;
  $("recover-attachment").hidden=!state.failedAttachment?.uploadId;$("recover-attachment").disabled=!!attachment||sendingAttachment;
  $("draft").placeholder=state.composerHint||"Сообщение Telegram";
  $("chat-title").textContent=state.chatTitle||"Выберите чат"; $("chat-hint").textContent=state.loadingHistory?"Загрузка истории…":state.testFixture?"Тестовые данные":state.connection||"";
  $("chat-error").textContent=state.error||""; $("recover").hidden=!state.failedDraft; $("recover").disabled=!!$("draft").value;
  $("history").disabled=!state.canLoadHistory; $("more-chats").disabled=!state.canLoadChats; $("more-chats").textContent=state.loadingChats?"Загрузка…":"Загрузить ещё";
  $("mark-read").disabled=!ready||!chatId||!(state.messages||[]).length;
  $("chats").replaceChildren();
  for(const chat of state.chats||[]) {
    const button=document.createElement("button"),top=document.createElement("span"),title=document.createElement("span"),preview=document.createElement("span");
    button.className="chat"+(chat.chatId===chatId?" active":"");top.className="chat-top";title.className="chat-title";title.textContent=chat.title;
    preview.className="preview";preview.textContent=chat.preview||"Нет сообщений";top.append(title);
    if(chat.unreadCount){const badge=document.createElement("span");badge.className="badge";badge.textContent=chat.unreadCount;top.append(badge);}
    button.append(top,preview);button.onclick=()=>openChat(chat.chatId);const item=document.createElement("div");item.setAttribute("role","listitem");item.append(button);$("chats").append(item);
  }
  const signature=JSON.stringify(state.messages||[]);
  if(signature!==messagesSignature) {
    for(const url of mediaUrls)URL.revokeObjectURL(url);mediaUrls=[];
    const list=$("messages"),atBottom=list.scrollHeight-list.scrollTop-list.clientHeight<80,initial=!messagesSignature;
    list.replaceChildren();
    for(const message of state.messages||[]) {
      const card=document.createElement("article"),author=document.createElement("p"),body=document.createElement("p"),meta=document.createElement("div");
      card.className="message"+(message.outgoing?" outgoing":"");author.className="author";author.textContent=message.author||"Участник";
      body.className="body";body.textContent=message.text||"";meta.className="meta";meta.textContent=[message.time,message.edited?"Изменено":"",message.delivery].filter(Boolean).join(" · ");
      card.append(author,body);
      if(message.media && message.media.fileId) {
        const media=message.media,button=document.createElement("button"),destination=chatId;
        button.className="secondary media-button";
        button.textContent=media.path?(media.kind==="photo"?"Показать фото":["voice","audio"].includes(media.kind)?"Прослушать":"Сохранить файл"):(media.downloading?"Загружается…":"Скачать вложение");
        button.disabled=!!media.downloading;
        button.onclick=async()=>{
          if(!media.path){await command("download",{chatId:destination,messageId:message.messageId});return;}
          try {
            const response=await fetch("/api/media/"+encodeURIComponent(destination)+"/"+encodeURIComponent(message.messageId),{headers:authHeaders});
            if(!response.ok)throw new Error("Файл пока недоступен. Повторите загрузку.");
            const url=URL.createObjectURL(await response.blob());mediaUrls.push(url);
            if(media.kind==="photo") {const img=document.createElement("img");img.className="media";img.alt="Фото из сообщения";img.src=url;card.insertBefore(img,button);button.hidden=true;}
            else if(["voice","audio"].includes(media.kind)){const audio=document.createElement("audio");audio.controls=true;audio.src=url;card.insertBefore(audio,button);button.hidden=true;}
            else {const link=document.createElement("a");link.href=url;link.download=String(media.name||"attachment").replace(/[\\/]/g,"_");link.click();}
          } catch(error){$("local-error").textContent=error.message;}
        };
        card.append(button);
      }
      if(message.media?.uploading)meta.textContent+=" · Загружено "+Math.round(100*(message.media.uploaded||0)/Math.max(1,message.media.size))+"%";
      card.append(meta);list.append(card);
    }
    if(initial||atBottom)list.scrollTop=list.scrollHeight;
    messagesSignature=signature;
  }
  previousPhase=phase;
}
async function poll(){
  if(!token){$("local-error").textContent="Откройте адрес, выданный скриптом запуска стенда.";return;}
  try{const response=await fetch("/api/state",{headers:authHeaders});if(!response.ok)throw new Error("Стенд недоступен. Проверьте адрес запуска.");render(await response.json());}
  catch(error){$("local-error").textContent=error.message;}
  setTimeout(poll,500);
}
poll();
