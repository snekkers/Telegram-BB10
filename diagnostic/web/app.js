"use strict";
const $ = id => document.getElementById(id);
const initialToken = location.hash.slice(1);
if(initialToken)sessionStorage.setItem("berrygram-local-token",initialToken);
const token = initialToken || sessionStorage.getItem("berrygram-local-token") || "";
history.replaceState(null, "", location.pathname);
let state = {}, previousPhase = "", chatId = "", pendingChat = null, messagesSignature = "", draftTimer, queue = Promise.resolve();
let mediaUrls=[];
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
  await flushDraft(); pendingChat=id; $("send").disabled=true; $("draft").disabled=true;
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
$("send-form").onsubmit=async event=>{
  event.preventDefault(); clearTimeout(draftTimer);
  const text=$("draft").value, destination=chatId;
  if (!state.canSend || pendingChat || !text.trim()) return;
  if(await command("sendText",{chatId:destination,text})) {
    if (chatId===destination && $("draft").value===text) $("draft").value="";
    $("messages").scrollTop=$("messages").scrollHeight;
  }
};
$("draft").oninput=()=>{clearTimeout(draftTimer); draftTimer=setTimeout(flushDraft,350); $("send").disabled=!state.canSend || !$("draft").value.trim() || !!pendingChat;};
$("draft").onkeydown=event=>{if(event.key==="Enter" && !event.shiftKey && !event.isComposing){event.preventDefault();$("send-form").requestSubmit();}};
$("search").oninput=()=>command("search",{text:$("search").value});
$("more-chats").onclick=()=>command("loadChats");
$("history").onclick=()=>command("loadHistory");
$("mark-read").onclick=()=>command("markRead",{chatId});
$("recover").onclick=()=>{if(!$("draft").value)command("recoverDraft");};
$("next-unread").onclick=async()=>{await flushDraft();command("nextUnread");};
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
  if (state.chatId!==chatId) {clearTimeout(draftTimer);chatId=state.chatId||"";$("draft").value=state.draft||"";messagesSignature="";}
  else if(document.activeElement!==$("draft") && state.draft && !$("draft").value)$("draft").value=state.draft;
  if (pendingChat===chatId)pendingChat=null;
  $("draft").disabled=!state.canSend||!!pendingChat; $("send").disabled=!state.canSend||!!pendingChat||!$("draft").value.trim();
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
        button.textContent=media.path?(media.kind==="photo"?"Показать фото":"Сохранить файл"):(media.downloading?"Загружается…":"Скачать вложение");
        button.disabled=!!media.downloading;
        button.onclick=async()=>{
          if(!media.path){await command("download",{chatId:destination,messageId:message.messageId});return;}
          try {
            const response=await fetch("/api/media/"+encodeURIComponent(destination)+"/"+encodeURIComponent(message.messageId),{headers:authHeaders});
            if(!response.ok)throw new Error("Файл пока недоступен. Повторите загрузку.");
            const url=URL.createObjectURL(await response.blob());mediaUrls.push(url);
            if(media.kind==="photo") {const img=document.createElement("img");img.className="media";img.alt="Фото из сообщения";img.src=url;card.insertBefore(img,button);button.hidden=true;}
            else {const link=document.createElement("a");link.href=url;link.download=String(media.name||"attachment").replace(/[\\/]/g,"_");link.click();}
          } catch(error){$("local-error").textContent=error.message;}
        };
        card.append(button);
      }
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
