#include "TelegramStore.hpp"
#include "TelegramSession.hpp"
#include <QDateTime>
namespace {
QString ru(const char *value){return QString::fromUtf8(value);}
QVariantMap method(const char *type){QVariantMap result;result.insert("@type",type);return result;}
QVariant number(const QString &id){return QVariant::fromValue(id.toLongLong());}
QString identifier(const QVariant &id){return QString::number(id.toLongLong());}
QVariantMap textContent(const QString &text){
    QVariantMap formatted=method("formattedText");formatted.insert("text",text);formatted.insert("entities",QVariantList());
    QVariantMap content=method("inputMessageText");content.insert("text",formatted);content.insert("clear_draft",false);return content;
}
QVariantMap quote(const QVariantMap &row){
    const QVariantMap source=row.value("formattedText").toMap();
    QString text=source.value("text").toString().left(128);
    if(!text.isEmpty() && text.at(text.size()-1).unicode()>=0xd800 && text.at(text.size()-1).unicode()<=0xdbff)text.chop(1);
    QVariantList entities;
    foreach(const QVariant &value,source.value("entities").toList()) {
        QVariantMap entity=value.toMap();const QString type=entity.value("type").toMap().value("@type").toString();
        const int offset=entity.value("offset").toInt(),length=entity.value("length").toInt();
        if(offset<0 || length<=0 || offset>=text.size())continue;
        if(type!="textEntityTypeBold" && type!="textEntityTypeItalic" && type!="textEntityTypeUnderline" && type!="textEntityTypeStrikethrough" && type!="textEntityTypeSpoiler" && type!="textEntityTypeCustomEmoji" && type!="textEntityTypeDateTime")continue;
        if((type=="textEntityTypeCustomEmoji" || type=="textEntityTypeDateTime") && offset+length>text.size()) {text=text.left(offset);continue;}
        entity.insert("length",qMin(length,text.size()-offset));entities.append(entity);
    }
    QVariantList clipped;
    foreach(const QVariant &value,entities) {
        QVariantMap entity=value.toMap();const int offset=entity.value("offset").toInt();
        if(offset>=text.size())continue;
        entity.insert("length",qMin(entity.value("length").toInt(),text.size()-offset));clipped.append(entity);
    }
    entities=clipped;
    if(text.isEmpty())return QVariantMap();
    QVariantMap formatted=method("formattedText");formatted.insert("text",text);formatted.insert("entities",entities);
    QVariantMap result=method("inputTextQuote");result.insert("text",formatted);result.insert("rich_texts",QVariantList());result.insert("position",0);return result;
}
}
void TelegramStore::setArchiveView(bool value){
    if(value==m_archiveView)return;
    m_archiveView=value;m_error.clear();rebuildChats();emit changed();
}
bool TelegramStore::setPinned(const QString &id,bool value){
    if(!m_ready || !m_chats.contains(id))return false;
    const QVariantMap chat=m_chats.value(id);const bool archive=chat.value("archiveOrder").toLongLong()>0;
    QVariantMap call=method("toggleChatIsPinned");call.insert("chat_id",number(id));call.insert("is_pinned",value);
    call.insert("chat_list",method(archive?"chatListArchive":"chatListMain"));
    return !submit(call,"pin",id).isEmpty();
}
bool TelegramStore::setArchived(const QString &id,bool value){
    if(!m_ready || !m_chats.contains(id))return false;
    QVariantMap call=method("addChatToList");call.insert("chat_id",number(id));call.insert("chat_list",method(value?"chatListArchive":"chatListMain"));
    return !submit(call,"archive",id).isEmpty();
}
bool TelegramStore::beginMessageAction(const QString &id,const QString &action){
    const QVariantMap row=m_messages.value(id.toLongLong());
    if(!m_ready || m_current.isEmpty() || row.isEmpty() || !row.value("sendingState").toString().isEmpty() ||
        (action!="reply" && action!="edit" && action!="forward"))return false;
    if(m_composerContext.value("mode")=="edit") {m_error=ru("Сначала сохраните или отмените редактирование.");emit changed();return false;}
    QVariantMap call=method("getMessageProperties");call.insert("chat_id",number(m_current));call.insert("message_id",number(id));
    const QString request=submit(call,"message-action",m_current);if(request.isEmpty())return false;
    ++m_actionSerial;m_requests[request].insert("action",action);m_requests[request].insert("messageId",id);m_requests[request].insert("actionSerial",m_actionSerial);
    m_error.clear();emit changed();return true;
}
void TelegramStore::applyMessageAction(const QString &id,const QString &action){
    QVariantMap row=m_messages.value(id.toLongLong());if(row.isEmpty())return;
    foreach(const QVariant &value,m_messageRows)if(value.toMap().value("messageId").toString()==id){row=value.toMap();break;}
    if(action=="forward") {
        m_forward.clear();m_forward.insert("sourceChatId",m_current);m_forward.insert("messageId",id);m_forward.insert("preview",row.value("text"));
        m_forward.insert("state","ready");emit changed();emit forwardReady();return;
    }
    if(action=="edit" && !row.value("textMessage").toBool()) {m_error=ru("Пока можно редактировать текстовые сообщения.");emit changed();return;}
    m_composerContext.clear();m_composerContext.insert("mode",action);m_composerContext.insert("messageId",id);
    m_composerContext.insert("author",row.value("author"));m_composerContext.insert("preview",row.value("text").toString().left(240));
    m_composerContext.insert("text",row.value("text"));
    if(action=="reply") {
        QVariantMap reply=method("inputMessageReplyToMessage");reply.insert("message_id",number(id));
        const QVariantMap quoted=quote(row);reply.insert("quote",quoted.isEmpty()?QVariant():QVariant(quoted));reply.insert("checklist_task_id",0);reply.insert("poll_option_id",QString());
        m_composerContext.insert("replyTo",reply);
    }
    emit changed();
}
void TelegramStore::cancelComposer(){++m_actionSerial;m_composerContext.clear();emit changed();}
bool TelegramStore::editText(const QString &text){
    if(!canSend() || text.trimmed().isEmpty() || text.size()>4096)return false;
    const QString target=m_composerContext.value("messageId").toString();
    QVariantMap intent;intent.insert("type","edit");intent.insert("text",text);intent.insert("sourceMessageId",target);
    const QString record=m_local.prepare(m_account,m_current,intent);
    if(record.isEmpty() || !m_local.transition(record,"submitting",target)){m_error=m_local.error();emit changed();return false;}
    QVariantMap call=method("editMessageText");call.insert("chat_id",number(m_current));call.insert("message_id",number(target));
    call.insert("reply_markup",QVariant());call.insert("input_message_content",textContent(text));
    const QString request=submit(call,"edit",m_current);
    if(request.isEmpty()){m_local.transition(record,"uncertain");return false;}
    m_requests[request].insert("recordId",record);m_requests[request].insert("messageId",target);m_requests[request].insert("actionSerial",m_actionSerial);
    m_error.clear();emit changed();return true;
}
bool TelegramStore::forwardTo(const QString &id){
    if(!m_ready || m_forward.value("state")!="ready" || m_forward.value("sourceChatId")!=m_current)return false;
    const QVariantMap chat=m_chats.value(id);
    if(!chat.value("canSend").toBool() || chat.value("topics").toBool() || chat.value("type")=="chatTypeSecret")return false;
    QVariantMap intent;intent.insert("type","forward");intent.insert("sourceMessageId",m_forward.value("messageId"));intent.insert("sourceChatId",m_current);
    const QString record=m_local.prepare(m_account,id,intent);
    if(record.isEmpty() || !m_local.transition(record,"submitting")){m_error=m_local.error();emit changed();return false;}
    QVariantMap call=method("forwardMessages");call.insert("chat_id",number(id));call.insert("from_chat_id",number(m_current));call.insert("topic_id",QVariant());
    call.insert("message_ids",QVariantList()<<number(m_forward.value("messageId").toString()));
    QVariantMap options=method("messageSendOptions");options.insert("paid_message_star_count",0);call.insert("options",options);call.insert("send_copy",false);call.insert("remove_caption",false);
    const QString request=submit(call,"forward",id);
    if(request.isEmpty()){m_local.transition(record,"uncertain");return false;}
    m_requests[request].insert("recordId",record);
    m_forward.insert("recordId",record);m_forward.insert("state","sending");m_forward.insert("status",ru("Пересылка…"));emit changed();return true;
}
QVariantMap TelegramStore::chatInfo() const {
    QVariantMap result=m_info,chat=m_chats.value(m_current);
    result.insert("title",chat.value("title"));result.insert("avatarKey","chat:"+m_current);result.insert("chatId",m_current);result.insert("group",chat.value("type")=="chatTypeBasicGroup" || chat.value("type")=="chatTypeSupergroup");
    result.insert("channel",chat.value("isChannel"));const bool archive=chat.value("archiveOrder").toLongLong()>0;
    result.insert("archived",archive);result.insert("pinned",chat.value(archive?"archivePinned":"mainPinned"));
    const QVariantMap notifications=chat.value("notificationSettings").toMap();
    const bool defaults=notifications.value("use_default_mute_for").toBool();
    result.insert("notificationsKnown",!notifications.isEmpty() && (!defaults || result.contains("defaultMute")));
    result.insert("notificationsOn",(defaults?result.value("defaultMute"):notifications.value("mute_for")).toInt()==0);
    result.remove("basicMembers");return result;
}
void TelegramStore::loadChatInfo(){
    if(!m_ready || m_current.isEmpty())return;
    ++m_infoSerial;m_info.clear();m_infoError.clear();m_infoSection=0;m_infoRows.clear();m_members.clear();m_shared.clear();m_infoLoading=false;m_infoEnd=true;
    const QVariantMap chat=m_chats.value(m_current);const QString type=chat.value("type").toString();
    QVariantMap call=method(type=="chatTypePrivate"?"getUserFullInfo":type=="chatTypeBasicGroup"?"getBasicGroupFullInfo":"getSupergroupFullInfo");
    call.insert(type=="chatTypePrivate"?"user_id":type=="chatTypeBasicGroup"?"basic_group_id":"supergroup_id",number(chat.value("peerId").toString()));
    if(type!="chatTypeSecret") {const QString request=submit(call,"info",m_current);if(!request.isEmpty())m_requests[request].insert("infoSerial",m_infoSerial);}
    const QVariantMap settings=chat.value("notificationSettings").toMap();
    if(settings.value("use_default_mute_for").toBool()) {
        call=method("getScopeNotificationSettings");call.insert("scope",method(type=="chatTypePrivate" || type=="chatTypeSecret"?"notificationSettingsScopePrivateChats":chat.value("isChannel").toBool()?"notificationSettingsScopeChannelChats":"notificationSettingsScopeGroupChats"));
        const QString request=submit(call,"info-scope",m_current);if(!request.isEmpty())m_requests[request].insert("infoSerial",m_infoSerial);
    }
    emit infoChanged();emit changed();
}
void TelegramStore::setInfoSection(int section){
    ++m_infoSerial;m_infoSection=qBound(0,section,3);m_infoCursor.clear();m_memberOffset=0;m_infoLoading=false;m_infoEnd=m_infoSection==0;
    m_infoRows.clear();m_infoError.clear();m_shared.clear();m_members.clear();
    emit infoChanged();if(!m_infoEnd)loadMoreInfo();
}
void TelegramStore::loadMoreInfo(){
    if(!m_ready || !canLoadInfo())return;
    const QVariantMap chat=m_chats.value(m_current);const QString type=chat.value("type").toString();
    QVariantMap call;QString kind;
    if(m_infoSection==1) {
        if(type=="chatTypeBasicGroup") {
            m_members=m_info.value("basicMembers").toList();m_infoEnd=true;updateInfoRows();return;
        }
        if(type!="chatTypeSupergroup" || !m_info.value("canGetMembers").toBool()) {m_infoEnd=true;m_infoError=ru("Telegram не разрешает просматривать участников этого чата.");emit infoChanged();return;}
        call=method("getSupergroupMembers");call.insert("supergroup_id",number(chat.value("peerId").toString()));call.insert("filter",QVariant());call.insert("offset",m_memberOffset);call.insert("limit",50);kind="info-members";
    } else {
        if(type=="chatTypeSecret") {m_infoEnd=true;m_infoError=ru("Общие медиа секретных чатов пока недоступны.");emit infoChanged();return;}
        call=method("searchChatMessages");call.insert("chat_id",number(m_current));call.insert("topic_id",QVariant());call.insert("query",QString());call.insert("sender_id",QVariant());
        call.insert("from_message_id",number(m_infoCursor));call.insert("offset",0);call.insert("limit",30);call.insert("filter",method(m_infoSection==2?"searchMessagesFilterPhoto":"searchMessagesFilterDocument"));kind="info-media";
    }
    const QString request=submit(call,kind,m_current);if(request.isEmpty())return;
    m_requests[request].insert("infoSerial",m_infoSerial);m_infoLoading=true;m_infoError.clear();emit infoChanged();
}
void TelegramStore::updateInfoRows(){
    QVariantList rows;
    if(m_infoSection==1)foreach(const QVariant &value,m_members) {
        const QVariantMap member=value.toMap();const QVariantMap sender=member.value("member_id").toMap();const QString id=identifier(sender.value("user_id"));
        QVariantMap row;row.insert("key",id);row.insert("chatId","member:"+id);row.insert("avatarKey","user:"+id);row.insert("title",m_users.value(id).value("name",ru("Участник")));
        const QString status=member.value("status").toMap().value("@type").toString();
        row.insert("preview",status=="chatMemberStatusCreator"?ru("Владелец"):status=="chatMemberStatusAdministrator"?ru("Администратор"):ru("Участник"));rows.append(row);
    } else foreach(const QVariant &value,m_shared) {
        const QVariantMap message=value.toMap();QVariantMap row=messageRow(message);
        row.insert("key",row.value("messageId"));row.insert("title",messageText(message.value("content").toMap()).left(100));row.insert("preview",QDateTime::fromTime_t(message.value("date").toUInt()).toString("dd.MM.yyyy HH:mm"));rows.append(row);
    }
    m_infoRows=rows;emit infoChanged();
}
bool TelegramStore::setNotifications(bool enabled){
    if(!m_ready || m_current.isEmpty())return false;
    QVariantMap settings=m_chats.value(m_current).value("notificationSettings").toMap();if(settings.isEmpty())return false;
    settings.insert("use_default_mute_for",false);settings.insert("mute_for",enabled?0:2147483647);
    QVariantMap call=method("setChatNotificationSettings");call.insert("chat_id",number(m_current));call.insert("notification_settings",settings);
    const QString request=submit(call,"notifications",m_current);if(request.isEmpty())return false;
    m_requests[request].insert("settings",settings);return true;
}
bool TelegramStore::actionResponse(const QVariantMap &object,const QVariantMap &pending){
    const QString kind=pending.value("kind").toString(),type=object.value("@type").toString(),chat=pending.value("chatId").toString();
    const bool current=chat==m_current && pending.value("generation").toUInt()==m_generation,error=type=="error";
    if(kind=="message-action") {
        if(current && pending.value("actionSerial").toUInt()==m_actionSerial) {
            const QString action=pending.value("action").toString();
            if(error || !object.value(action=="reply"?"can_be_replied":action=="edit"?"can_be_edited":"can_be_forwarded").toBool())m_error=ru("Telegram не разрешает это действие с сообщением.");
            else applyMessageAction(pending.value("messageId").toString(),action);
        }
    } else if(kind=="edit") {
        const QString record=pending.value("recordId").toString();
        if(error) {m_local.transition(record,"failed");if(current)m_error=ru("Не удалось изменить сообщение (код %1). Текст оставлен в поле.").arg(object.value("code").toInt());}
        else if(type=="message") {
            m_local.transition(record,"sent",identifier(object.value("id")));if(current){upsertMessage(object);rebuildMessages();}
            if(current && pending.value("actionSerial").toUInt()==m_actionSerial) {m_composerContext.clear();m_error.clear();}
        }
    } else if(kind=="forward") {
        const QString record=pending.value("recordId").toString();const QVariantList messages=object.value("messages").toList();
        if(error || messages.isEmpty() || messages.first().toMap().isEmpty()) {
            m_local.transition(record,"failed");if(m_forward.value("recordId")==record) {m_forward.insert("state","failed");m_forward.insert("status",ru("Telegram отклонил пересылку. Сообщение не переслано."));}
        } else {
            QVariantMap message=messages.first().toMap();
            const QString earlyKey=chat+":"+identifier(message.value("id"));
            if(m_earlyForwards.contains(earlyKey))message=m_earlyForwards.take(earlyKey);
            recordSendResult(message,record);upsertMessage(message);rebuildMessages();
            if(m_forward.value("recordId")==record) {const QString state=m_local.entry(record).value("state").toString();m_forward.insert("state",state);m_forward.insert("status",state=="sent"?ru("Сообщение переслано."):state=="failed"?ru("Пересылка отклонена."):ru("Передано Telegram. Ожидание подтверждения."));}
        }
    } else if(kind=="pin" || kind=="archive" || kind=="notifications") {
        if(error)m_error=ru("Не удалось изменить настройки чата (код %1).").arg(object.value("code").toInt());
        else if(kind=="notifications" && m_chats.contains(chat)) {m_chats[chat].insert("notificationSettings",pending.value("settings"));}
        emit infoChanged();
    } else if(kind.startsWith("info")) {
        if(!current || (kind!="info" && kind!="info-scope" && pending.value("infoSerial").toUInt()!=m_infoSerial))return true;
        if(error) {m_infoError=ru("Не удалось загрузить сведения (код %1).").arg(object.value("code").toInt());m_infoLoading=false;}
        else if(kind=="info") {
            m_info.insert("description",type=="userFullInfo"?object.value("bio").toMap().value("text"):object.value("description"));
            m_info.insert("memberCount",object.value("member_count",object.value("members").toList().size()));
            m_info.insert("canGetMembers",type=="basicGroupFullInfo" || object.value("can_get_members").toBool());
            m_info.insert("basicMembers",object.value("members"));
            if(m_infoSection==1 && !m_infoLoading) {m_infoEnd=false;loadMoreInfo();}
        } else if(kind=="info-scope")m_info.insert("defaultMute",object.value("mute_for"));
        else if(kind=="info-members") {
            const QVariantList members=object.value("members").toList();m_members+=members;m_memberOffset+=members.size();
            m_infoLoading=false;m_infoEnd=members.isEmpty() || m_memberOffset>=object.value("total_count").toInt();updateInfoRows();
        } else if(kind=="info-media") {
            const QString next=identifier(object.value("next_from_message_id"));m_infoEnd=next=="0" || next==m_infoCursor;m_infoCursor=next;
            foreach(const QVariant &value,object.value("messages").toList()) {
                bool exists=false;foreach(const QVariant &old,m_shared)if(old.toMap().value("id")==value.toMap().value("id")){exists=true;break;}
                if(!exists)m_shared.append(value);
            }
            m_infoLoading=false;updateInfoRows();
        }
        emit infoChanged();
    } else return false;
    emit changed();return true;
}
