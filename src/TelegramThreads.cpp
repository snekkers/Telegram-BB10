#include "TelegramStore.hpp"
#include "TelegramSession.hpp"

namespace {
QString ru(const char *text){return QString::fromUtf8(text);}
QString idOf(const QVariant &id){return QString::number(id.toLongLong());}
QVariantMap method(const char *type){QVariantMap call;call.insert("@type",type);return call;}
}

void TelegramStore::applyReplyInfo(QVariantMap *row,const QVariantMap &interaction) const {
    const QVariantMap info=interaction.value("reply_info").toMap();
    row->insert("hasComments",!info.isEmpty());
    row->insert("commentCount",qMax(0,info.value("reply_count").toInt()));
}

bool TelegramStore::openComments(const QString &id){
    const QVariantMap row=m_messages.value(id.toLongLong());
    if(!m_ready || !m_online || m_current.isEmpty() || inComments() || commentsLoading() ||
       row.isEmpty() || (!row.value("hasComments").toBool() &&
       (m_chats.value(m_current).value("isChannel").toBool() ||
        (m_chats.value(m_current).value("type")!="chatTypeSupergroup" && m_chats.value(m_current).value("type")!="chatTypeBasicGroup"))) ||
       !row.value("sendingState").toString().isEmpty())return false;
    if(!m_composerContext.isEmpty()){m_error=ru("Сначала завершите ответ или редактирование.");emit changed();return false;}
    QVariantMap call=method("getMessageProperties");call.insert("chat_id",m_current.toLongLong());call.insert("message_id",id.toLongLong());
    const QString request=submit(call,"comments-check",m_current);if(request.isEmpty())return false;
    m_pendingComment=id;m_requests[request].insert("messageId",id);m_error.clear();emit changed();return true;
}

bool TelegramStore::belongsToThread(const QVariantMap &message) const {
    if(!inComments())return true;
    if(idOf(message.value("id"))==m_threadRoot)return true;
    const QVariantMap topic=message.value("topic_id").toMap();
    return (topic.value("@type")=="messageTopicThread" && idOf(topic.value("message_thread_id"))==m_threadId) ||
        idOf(message.value("message_thread_id"))==m_threadId;
}

void TelegramStore::activateThread(const QVariantMap &thread,const QVariantMap &pending){
    const QString chat=idOf(thread.value("chat_id")),topic=idOf(thread.value("message_thread_id"));
    const QVariantList messages=thread.value("messages").toList();
    if(chat=="0" || topic.toLongLong()<=0 || messages.isEmpty() || !m_chats.contains(chat)){
        m_pendingComment.clear();m_error=ru("Обсуждение этой публикации недоступно.");emit changed();return;
    }
    // TDLib returns the root as the last element, after any album messages.
    const QString root=idOf(messages.last().toMap().value("id"));
    if(root.toLongLong()<=0){m_pendingComment.clear();m_error=ru("Не удалось открыть обсуждение.");emit changed();return;}
    const QString source=m_current,sourceMessage=pending.value("messageId").toString();
    closeChat();m_current=chat;m_threadId=topic;m_threadRoot=root;
    m_threadSourceChat=source;m_threadSourceMessage=sourceMessage;
    restoreChatDraft(draftKey(),thread.value("draft_message"));
    QVariantMap call=method("openChat");call.insert("chat_id",chat.toLongLong());submit(call,"open",chat);
    foreach(const QVariant &value,messages)upsertMessage(value.toMap());
    rebuildMessages();history(true);emit changed();requestScroll();
}

bool TelegramStore::returnToPost(){
    if(!inComments())return false;
    const QString chat=m_threadSourceChat,message=m_threadSourceMessage;
    closeChat();return openChatAt(chat,message);
}

bool TelegramStore::threadResponse(const QVariantMap &object,const QVariantMap &pending){
    const QString kind=pending.value("kind").toString();
    if(!kind.startsWith("comments-"))return false;
    if(pending.value("generation").toUInt()!=m_generation || pending.value("chatId")!=m_current ||
       pending.value("messageId")!=m_pendingComment || m_pendingComment.isEmpty())return true;
    const bool error=object.value("@type")=="error";
    if(error || (kind=="comments-check" && !object.value("can_get_message_thread").toBool())){
        m_pendingComment.clear();m_error=ru("Комментарии недоступны или у вас нет доступа к обсуждению.");emit changed();return true;
    }
    QVariantMap call;QString next;
    if(kind=="comments-check"){
        call=method("getMessageThread");call.insert("chat_id",m_current.toLongLong());call.insert("message_id",m_pendingComment.toLongLong());next="comments-thread";
    }else if(kind=="comments-thread" && object.value("@type")=="messageThreadInfo"){
        const QString chat=idOf(object.value("chat_id"));
        if(m_chats.contains(chat)){activateThread(object,pending);return true;}
        call=method("getChat");call.insert("chat_id",chat.toLongLong());next="comments-chat";
    }else if(kind=="comments-chat" && object.value("@type")=="chat" &&
             idOf(object.value("id"))==idOf(pending.value("thread").toMap().value("chat_id"))){
        consume(object);activateThread(pending.value("thread").toMap(),pending);return true;
    }else {m_pendingComment.clear();m_error=ru("Не удалось загрузить обсуждение. Повторите.");emit changed();return true;}
    const QString request=submit(call,next,m_current);
    if(request.isEmpty()){m_pendingComment.clear();m_error=ru("Не удалось открыть комментарии. Повторите.");emit changed();return true;}
    m_requests[request].insert("messageId",m_pendingComment);
    if(next=="comments-chat")m_requests[request].insert("thread",object);
    return true;
}
