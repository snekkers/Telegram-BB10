#include "ChatController.hpp"
#include <QDateTime>
bool ChatController::setPinned(const QString &id,bool value){
    for(int i=0;i<m_chatRows.size();++i){QVariantMap row=m_chatRows.at(i).toMap();if(row.value("chatId")==id){row.insert("pinned",value);m_chatRows[i]=row;refreshChats();return true;}}return false;
}
bool ChatController::setArchived(const QString &id,bool value){
    for(int i=0;i<m_chatRows.size();++i){QVariantMap row=m_chatRows.at(i).toMap();if(row.value("chatId")==id){row.insert("archived",value);m_chatRows[i]=row;refreshChats();return true;}}return false;
}
int ChatController::communityCount() const{
    int count=0;foreach(const QVariant &value,m_chatRows)if(!value.toMap().value("personal").toBool() && value.toMap().value("archived").toBool()==m_archiveView)++count;return count;
}
bool ChatController::beginMessageAction(const QString &id,const QString &action){
    if(m_context.value("mode")=="edit" || (!composerVisible() && action!="forward"))return false;
    foreach(const QVariant &value,m_history.value(m_currentChatId)){
        const QVariantMap row=value.toMap();if(row.value("messageId")!=id)continue;
        if(action=="edit" && !row.value("outgoing").toBool())return false;
        if(action=="forward"){
            m_forward.clear();m_forward.insert("messageId",id);m_forward.insert("sourceChatId",m_currentChatId);m_forward.insert("preview",row.value("text"));m_forward.insert("state","ready");++m_forwardSerial;
        }else{
            m_context.clear();m_context.insert("mode",action);m_context.insert("messageId",id);m_context.insert("preview",row.value("text").toString().left(240));m_context.insert("text",row.value("text"));m_context.insert("author",row.value("author"));
        }
        emit currentChatChanged();return true;
    }return false;
}
bool ChatController::forwardTo(const QString &id){
    if(!m_history.contains(id) || m_forward.value("state")!="ready")return false;
    const QVariantList source=m_history.value(m_forward.value("sourceChatId").toString());
    foreach(const QVariant &value,source){
        QVariantMap row=value.toMap();if(row.value("messageId")!=m_forward.value("messageId") && !m_forward.value("messageIds").toStringList().contains(row.value("messageId").toString()))continue;
        QVariantList &history=m_history[id];row.insert("order",history.size());row.insert("messageId",QString::number(history.size()+1));
        row.insert("forwardAuthor",row.value("author"));row.insert("outgoing",true);row.insert("author",QString::fromUtf8("Вы"));row.insert("forwarded",true);row.insert("date",QDateTime::currentDateTime().toTime_t());history.append(row);
        for(int i=0;i<m_chatRows.size();++i){QVariantMap chat=m_chatRows.at(i).toMap();if(chat.value("chatId")==id){chat.insert("preview",row.value("text"));m_chatRows[i]=chat;break;}}
        m_forward.insert("state","sent");m_forward.insert("status",QString::fromUtf8("Переслано локально · демо"));
    }refreshChats();return m_forward.value("state")=="sent";
}
QVariantMap ChatController::chatInfo() const{
    QVariantMap result;
    foreach(const QVariant &value,m_chatRows)if(value.toMap().value("chatId")==m_currentChatId)result=value.toMap();
    result.insert("avatarKey",currentAvatarKey());
    result.insert("group",!result.value("personal").toBool());result.insert("channel",m_currentChatId=="demo-design");
    result.insert("memberCount",result.value("personal").toBool()?2:48);
    result.insert("description",QString::fromUtf8("Демонстрационный чат Berrygram. Все действия выполняются локально."));
    result.insert("notificationsKnown",true);result.insert("notificationsOn",!m_muted.value(m_currentChatId));return result;
}
void ChatController::setInfoSection(int section){
    m_infoSection=section;m_infoRows->clear();
    if(section==1 && chatInfo().value("group").toBool()){
        const QStringList names=QStringList()<<QString::fromUtf8("Анна")<<QString::fromUtf8("Вы");
        for(int i=0;i<names.size();++i){QVariantMap row;row.insert("chatId",QString("demo-member-%1").arg(i));row.insert("title",names.at(i));row.insert("avatarKey",i==0?"demo:anna":"");row.insert("preview",i==0?QString::fromUtf8("Администратор"):QString::fromUtf8("Участник"));m_infoRows->append(QVariant(row));}
    }
    emit currentChatChanged();
}
bool ChatController::injectIncomingDemo(){
    if(m_currentChatId.isEmpty())return false;
    QVariantList &history=m_history[m_currentChatId];QVariantMap row;row.insert("messageId",QString::number(history.size()+1));row.insert("order",history.size());
    row.insert("date",QDateTime::currentDateTime().toTime_t());row.insert("author",QString::fromUtf8("Анна"));row.insert("text",QString::fromUtf8("Новое входящее сообщение · демо"));row.insert("outgoing",false);row.insert("textMessage",true);
    history.append(row);refreshMessages();++m_incomingSerial;emit currentChatChanged();return true;
}

QString ChatController::selectedText() const{QStringList result;foreach(const QVariant &value,m_history.value(m_currentChatId))if(m_selected.contains(value.toMap().value("messageId").toString()))result<<value.toMap().value("text").toString();return result.join("\n\n");}
bool ChatController::toggleSelected(const QString &id){
    bool exists=false;foreach(const QVariant &value,m_history.value(m_currentChatId))if(value.toMap().value("messageId")==id)exists=true;
    if(!exists)return false;if(m_selected.contains(id))m_selected.remove(id);else if(m_selected.size()<50)m_selected.insert(id);else return false;
    m_selectionState.clear();refreshMessages();emit currentChatChanged();return true;
}
bool ChatController::prepareSelection(const QString &action){
    if(m_selected.isEmpty())return false;
    m_selectionState.insert("state","ready");m_selectionState.insert("action",action);m_selectionState.insert("all",true);m_selectionState.insert("self",true);
    if(action=="forward"){QStringList ids;foreach(const QVariant &value,m_history.value(m_currentChatId))if(m_selected.contains(value.toMap().value("messageId").toString()))ids<<value.toMap().value("messageId").toString();
        m_forward.clear();m_forward.insert("messageIds",ids);m_forward.insert("messageId",ids.first());m_forward.insert("sourceChatId",m_currentChatId);m_forward.insert("preview",QString::fromUtf8("Выбрано сообщений: %1").arg(ids.size()));m_forward.insert("state","ready");++m_forwardSerial;}
    emit currentChatChanged();return true;
}
bool ChatController::deleteSelected(bool){
    if(m_selectionState.value("state")!="ready" || m_selectionState.value("action")!="delete")return false;
    QVariantList remaining;foreach(const QVariant &value,m_history.value(m_currentChatId))if(!m_selected.contains(value.toMap().value("messageId").toString()))remaining<<value;
    m_history[m_currentChatId]=remaining;clearSelection();return true;
}

bool ChatController::injectRichDemo(){
    if(m_currentChatId.isEmpty())return false;QVariantList &history=m_history[m_currentChatId];int id=1;foreach(const QVariant &v,history)id=qMax(id,v.toMap().value("messageId").toString().toInt()+1);
    const QString text=QString::fromUtf8("Важный текст, код и ссылка https://example.org 😀 <текст>");QVariantMap row;row.insert("messageId",QString::number(id));row.insert("order",history.size());row.insert("date",QDateTime::currentDateTime().toTime_t());row.insert("author",QString::fromUtf8("Анна"));row.insert("text",text);row.insert("outgoing",false);row.insert("textMessage",true);row.insert("link","https://example.org");
    QVariantList entities;QVariantMap bold,kind;kind.insert("@type","textEntityTypeBold");bold.insert("type",kind);bold.insert("offset",0);bold.insert("length",QString::fromUtf8("Важный").size());entities<<bold;
    QVariantMap code;kind.insert("@type","textEntityTypeCode");code.insert("type",kind);code.insert("offset",text.indexOf(QString::fromUtf8("код")));code.insert("length",3);entities<<code;
    QVariantMap link;kind.insert("@type","textEntityTypeUrl");link.insert("type",kind);link.insert("offset",text.indexOf("https://"));link.insert("length",19);entities<<link;QVariantMap formatted;formatted.insert("text",text);formatted.insert("entities",entities);row.insert("formattedText",formatted);
    history<<row;refreshMessages();++m_incomingSerial;emit currentChatChanged();return true;
}
bool ChatController::injectStressDemo(){
    if(m_currentChatId.isEmpty())return false;QVariantList &history=m_history[m_currentChatId];int id=1;foreach(const QVariant &v,history)id=qMax(id,v.toMap().value("messageId").toString().toInt()+1);
    for(int i=0;i<300;++i){QVariantMap row;row.insert("messageId",QString::number(id+i));row.insert("order",history.size());row.insert("date",QDateTime::currentDateTime().toTime_t());row.insert("author",QString::fromUtf8("Демо"));row.insert("text",QString::fromUtf8("Проверка длинной истории %1 😀").arg(i+1));row.insert("outgoing",false);row.insert("textMessage",true);history<<row;}
    refreshMessages();++m_incomingSerial;emit currentChatChanged();return true;
}
