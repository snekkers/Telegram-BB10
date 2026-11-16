#ifndef BERRYGRAM_NOTIFICATION_POLICY_HPP
#define BERRYGRAM_NOTIFICATION_POLICY_HPP
#include <QVariant>
#include <QMap>
#include <QSet>
#include <QStringList>
class NotificationPolicy {
public:
    NotificationPolicy():unread(0){}
    int unread;
    QVariantList consume(const QVariantMap &event,bool foreground,const QString &current){
        QVariantList actions;const QString type=event.value("@type").toString();
        if(type=="updateUnreadMessageCount"){
            const QString list=event.value("chat_list").toMap().value("@type").toString();
            if(list=="chatListMain" || list=="chatListArchive"){m_counts[list]=qMax(0,event.value("unread_unmuted_count").toInt());unread=m_counts.value("chatListMain")+m_counts.value("chatListArchive");}
        }
        if(type!="updateNotificationGroup")return actions;
        const QString group=QString::number(event.value("notification_group_id").toInt());
        const QString chat=QString::number(event.value("chat_id").toLongLong());
        foreach(const QVariant &id,event.value("removed_notification_ids").toList()){
            QVariantMap action;action.insert("remove",true);action.insert("key",group+":"+QString::number(id.toInt()));actions.append(action);
        }
        foreach(const QVariant &value,event.value("added_notifications").toList()){
            const QVariantMap notification=value.toMap(),content=notification.value("type").toMap();
            const QString key=group+":"+QString::number(notification.value("id").toInt());
            if(m_seen.contains(key))continue;
            m_seen.insert(key);m_order.append(key);while(m_order.size()>256)m_seen.remove(m_order.takeFirst());
            if(content.value("@type")!="notificationTypeNewMessage" || (foreground && chat==current))continue;
            const QVariantMap message=content.value("message").toMap();if(message.value("is_outgoing").toBool())continue;
            QVariantMap action;action.insert("key",key);action.insert("chatId",chat);
            QString body=QString::fromUtf8("Новое сообщение");
            if(content.value("show_preview").toBool()){
                const QVariantMap c=message.value("content").toMap();
                if(c.value("@type")=="messageText")body=c.value("text").toMap().value("text").toString().left(160);
                else if(c.value("@type")=="messagePhoto")body=QString::fromUtf8("Фотография");
            }
            action.insert("body",body);actions.append(action);
        }
        return actions;
    }
    void reset(){unread=0;m_counts.clear();m_seen.clear();m_order.clear();}
private:
    QMap<QString,int> m_counts;QSet<QString> m_seen;QStringList m_order;
};
#endif
