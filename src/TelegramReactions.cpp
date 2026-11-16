#include "TelegramStore.hpp"
namespace {
QString ru(const char *v){return QString::fromUtf8(v);}
QVariantMap method(const char *type){QVariantMap v;v.insert("@type",type);return v;}
QVariant number(const QString &v){return QVariant::fromValue(v.toLongLong());}
}
bool TelegramStore::beginReactions(const QString &id){
    const QVariantMap row=m_messages.value(id.toLongLong());if(!m_ready || row.isEmpty() || !row.value("sendingState").toString().isEmpty())return false;
    cancelReactions();m_reactionState.insert("messageId",id);m_reactionState.insert("state","loading");
    QVariantMap call=method("getMessageAvailableReactions");call.insert("chat_id",number(m_current));call.insert("message_id",number(id));call.insert("row_size",8);
    const QString request=submit(call,"reaction-available",m_current);if(request.isEmpty()){m_reactionState.clear();return false;}
    m_requests[request].insert("reactionSerial",m_reactionSerial);emit changed();return true;
}
void TelegramStore::cancelReactions(){++m_reactionSerial;m_reactionState.clear();emit changed();}
bool TelegramStore::chooseReaction(const QString &emoji){
    if(!m_ready || m_reactionState.value("state")!="ready")return false;
    QVariantMap option;foreach(const QVariant &v,m_reactionState.value("options").toList())if(v.toMap().value("emoji")==emoji)option=v.toMap();
    if(option.isEmpty() || option.value("premium").toBool())return false;
    QVariantMap type=method("reactionTypeEmoji");type.insert("emoji",emoji);
    QVariantMap call=method(option.value("chosen").toBool()?"removeMessageReaction":"addMessageReaction");
    call.insert("chat_id",number(m_current));call.insert("message_id",number(m_reactionState.value("messageId").toString()));call.insert("reaction_type",type);
    if(!option.value("chosen").toBool()){call.insert("is_big",false);call.insert("update_recent_reactions",true);}
    const QString request=submit(call,"reaction-change",m_current);if(request.isEmpty())return false;
    m_requests[request].insert("reactionSerial",m_reactionSerial);m_reactionState.insert("state","sending");emit changed();return true;
}
bool TelegramStore::loadReactors(bool more){
    if(!m_ready || m_reactionState.value("messageId").toString().isEmpty() || m_reactionState.value("peopleLoading").toBool())return false;
    const QString offset=more?m_reactionState.value("nextOffset").toString():QString();if(more && offset.isEmpty())return false;
    if(!more){m_reactionState.insert("people",QVariantList());m_reactionState.insert("names",QString());}
    QVariantMap call=method("getMessageAddedReactions");call.insert("chat_id",number(m_current));call.insert("message_id",number(m_reactionState.value("messageId").toString()));call.insert("reaction_type",QVariant());call.insert("offset",offset);call.insert("limit",50);
    const QString request=submit(call,"reaction-people",m_current);if(request.isEmpty())return false;
    m_requests[request].insert("reactionSerial",m_reactionSerial);m_reactionState.insert("peopleLoading",true);emit changed();return true;
}
void TelegramStore::updateReactionNames(){
    QStringList names;foreach(const QVariant &value,m_reactionState.value("people").toList()){
        const QVariantMap r=value.toMap(),sender=r.value("sender_id").toMap(),type=r.value("type").toMap();const QString id=QString::number(sender.value("user_id").toLongLong());
        QString name=m_users.value(id).value("name").toString();if(name.isEmpty())name=sender.value("@type")=="messageSenderChat"?ru("Канал или группа"):ru("Участник %1").arg(id);
        names<<type.value("emoji").toString()+"  "+name;
    }
    m_reactionState.insert("names",names.join("\n"));
}
bool TelegramStore::reactionResponse(const QVariantMap &object,const QVariantMap &pending){
    const QString kind=pending.value("kind").toString();if(!kind.startsWith("reaction-"))return false;
    if(pending.value("chatId").toString()!=m_current || pending.value("generation").toUInt()!=m_generation || pending.value("reactionSerial").toUInt()!=m_reactionSerial)return true;
    const bool error=object.value("@type")=="error";
    if(kind=="reaction-available"){
        if(error){m_reactionState.insert("state","error");m_reactionState.insert("status",ru("Не удалось загрузить доступные реакции."));}
        else{
            QVariantList options;QStringList seen;const QStringList groups=QStringList()<<"top_reactions"<<"recent_reactions"<<"popular_reactions";
            const QVariantList existing=m_messages.value(m_reactionState.value("messageId").toLongLong()).value("reactions").toList();
            foreach(const QString &group,groups)foreach(const QVariant &value,object.value(group).toList()){
                const QVariantMap entry=value.toMap(),type=entry.value("type").toMap();const QString emoji=type.value("emoji").toString();
                if(type.value("@type")!="reactionTypeEmoji" || emoji.isEmpty() || seen.contains(emoji) || options.size()>=12)continue;
                QVariantMap option;option.insert("emoji",emoji);option.insert("premium",entry.value("needs_premium").toBool());
                foreach(const QVariant &reaction,existing)if(reaction.toMap().value("type").toMap().value("emoji")==emoji)option.insert("chosen",reaction.toMap().value("is_chosen"));
                options<<option;seen<<emoji;
            }
            m_reactionState.insert("options",options);m_reactionState.insert("state","ready");if(options.isEmpty())m_reactionState.insert("status",ru("В этом сообщении нет доступных обычных реакций."));
        }
    }else if(kind=="reaction-change"){
        m_reactionState.insert("state","ready");m_reactionState.insert("status",error?ru("Telegram не принял реакцию."):ru("Реакция отправлена."));
        if(!error){const QString id=m_reactionState.value("messageId").toString();beginReactions(id);}
    }else if(kind=="reaction-people"){
        m_reactionState.insert("peopleLoading",false);
        if(error)m_reactionState.insert("peopleError",ru("Telegram не показывает список реакций."));
        else{
            QVariantList people=object.value("reactions").toList();while(people.size()>50)people.removeLast();
            m_reactionState.insert("people",people);m_reactionState.insert("nextOffset",object.value("next_offset"));m_reactionState.insert("peopleError",people.isEmpty()?ru("Список реакций пуст или скрыт."):QString());
            updateReactionNames();int count=0;foreach(const QVariant &value,people){const QVariantMap sender=value.toMap().value("sender_id").toMap();const QString id=QString::number(sender.value("user_id").toLongLong());if(sender.value("@type")!="messageSenderUser" || m_users.contains(id) || ++count>20)continue;QVariantMap call=method("getUser");call.insert("user_id",number(id));const QString request=submit(call,"reaction-user",m_current);if(!request.isEmpty())m_requests[request].insert("reactionSerial",m_reactionSerial);}
        }
    }else if(kind=="reaction-user" && !error){upsertUser(object);updateReactionNames();}
    emit changed();return true;
}
