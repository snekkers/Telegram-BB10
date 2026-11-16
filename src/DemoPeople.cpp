#include "ChatController.hpp"
#include <QMetaObject>
QString ChatController::currentAvatarKey() const{
    return m_currentChatId=="demo-anna"?"demo:anna":m_currentChatId=="demo-design"?"demo:channel":m_currentChatId=="demo-project"?"demo:group":m_currentChatId=="demo-bb10" || m_currentChatId=="demo-boris"?"demo:classic":"";
}
QString ChatController::currentChatStatus() const{
    if(!m_demoStatus.isEmpty())return m_demoStatus+QString::fromUtf8(" · демо");
    if(m_currentChatId=="demo-anna")return QString::fromUtf8("онлайн · демо");
    if(m_currentChatId=="demo-boris")return QString::fromUtf8("недавно в сети · демо");
    if(m_currentChatId=="demo-saved")return QString::fromUtf8("сообщения для себя · демо");
    return composerVisible()?QString::fromUtf8("группа · демо"):QString::fromUtf8("канал · демо");
}
bool ChatController::currentStatusActive() const{return m_demoStatus==QString::fromUtf8("печатает…") || (m_demoStatus.isEmpty() && m_currentChatId=="demo-anna") || m_demoStatus==QString::fromUtf8("онлайн");}
void ChatController::demoPresence(const QString &status){
    m_typingTimer.stop();
    if(status=="typing"){m_demoStatus=QString::fromUtf8("печатает…");m_typingTimer.start(6000);}
    else if(status=="offline")m_demoStatus=QString::fromUtf8("в сети вчера в 18:40");
    else if(status=="hidden")m_demoStatus=QString::fromUtf8("статус недоступен");
    else m_demoStatus=QString::fromUtf8("онлайн");
    emit currentChatChanged();
}
void ChatController::clearDemoTyping(){m_demoStatus.clear();emit currentChatChanged();}
void ChatController::loadContacts(){refreshContacts();}
void ChatController::refreshContacts(){
    QVariantList rows;
    if(m_contactSection==1){
        foreach(const QVariant &value,m_chatRows){QVariantMap row=value.toMap();if(!row.value("personal").toBool() || row.value("archived").toBool())continue;
            row.insert("userId",row.value("chatId"));row.insert("key",row.value("chatId"));row.insert("preview",QString::fromUtf8("недавний диалог · демо"));row.insert("unreadLabel",QString());row.insert("time",QString());rows.append(row);}
    }else{
        const QStringList ids=QStringList()<<"demo-anna"<<"demo-boris"<<"demo-katya";
        const QStringList names=QStringList()<<QString::fromUtf8("Анна")<<QString::fromUtf8("Борис")<<QString::fromUtf8("Катя");
        const QStringList states=QStringList()<<QString::fromUtf8("онлайн")<<QString::fromUtf8("недавно в сети")<<QString::fromUtf8("статус недоступен");
        for(int i=0;i<ids.size();++i){QVariantMap row;row.insert("userId",ids.at(i));row.insert("chatId",ids.at(i));row.insert("key",ids.at(i));row.insert("title",names.at(i));row.insert("username",i==0?"anna":"");row.insert("preview",states.at(i));row.insert("statusActive",i==0);row.insert("avatarKey",i==0?"demo:anna":i==1?"demo:classic":"");rows.append(row);}
    }
    m_contacts->clear();QString query=m_contactQuery;if(query.startsWith('@'))query.remove(0,1);
    foreach(const QVariant &value,rows){const QVariantMap row=value.toMap();
        if(query.isEmpty() || row.value("title").toString().contains(query,Qt::CaseInsensitive) || row.value("username").toString().contains(query,Qt::CaseInsensitive))m_contacts->append(value);}
    emit currentChatChanged();
}
bool ChatController::openContact(const QString &id,QObject *target){
    if(!target || target->objectName()!="contactsPage")return false;
    if(id!="demo-anna" && id!="demo-boris" && id!="demo-katya" && id!="demo-saved")return false;
    if(!m_history.contains(id)){
        const QString name=id=="demo-boris"?QString::fromUtf8("Борис"):QString::fromUtf8("Катя");
        addDemoChat(id,name,QString::fromUtf8("Пока нет сообщений"),m_chatRows.size());m_history[id]=QVariantList();refreshChats();
    }
    QMetaObject::invokeMethod(target,"openContactChat",Qt::QueuedConnection,Q_ARG(QVariant,QVariant(id)));return true;
}
