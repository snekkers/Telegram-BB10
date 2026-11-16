#include "NotificationCenter.hpp"
#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
#include "Preferences.hpp"
#include <bb/cascades/Application>
#include <bb/platform/Notification>
#include <bps/navigator.h>
NotificationCenter::NotificationCenter(TelegramSession *session,TelegramStore *store,Preferences *preferences,QObject *parent):QObject(parent),m_session(session),m_store(store),m_preferences(preferences),m_foreground(true),m_wasReady(false),m_hidePreview(preferences->hidePreview()){
    connect(preferences,SIGNAL(changed()),this,SLOT(preferencesChanged()));
    connect(session,SIGNAL(eventReceived(QVariantMap)),this,SLOT(consume(QVariantMap)));
    connect(session,SIGNAL(changed()),this,SLOT(sessionChanged()));
    connect(bb::cascades::Application::instance(),SIGNAL(fullscreen()),this,SLOT(foreground()));
    connect(bb::cascades::Application::instance(),SIGNAL(thumbnail()),this,SLOT(background()));
    connect(bb::cascades::Application::instance(),SIGNAL(invisible()),this,SLOT(background()));
}
void NotificationCenter::preferencesChanged(){const bool hidden=m_preferences->hidePreview();if(hidden && !m_hidePreview)bb::platform::Notification::deleteAllFromInbox();m_hidePreview=hidden;}
void NotificationCenter::foreground(){m_foreground=true;m_store->setForeground(true);}
void NotificationCenter::background(){m_foreground=false;m_store->setForeground(false);}
void NotificationCenter::sessionChanged(){
    const bool ready=m_session->phase()=="ready";
    if(m_wasReady && !ready){m_policy.reset();navigator_clear_badge();bb::platform::Notification::deleteAllFromInbox();emit changed();}
    m_wasReady=ready;
}
void NotificationCenter::consume(const QVariantMap &event){
    if(!m_store->ready())return;
    const int previous=m_policy.unread;
    const QVariantList actions=m_policy.consume(event,m_foreground,m_store->currentChatId());
    foreach(const QVariant &value,actions){
        const QVariantMap action=value.toMap();const QString key="berrygram:"+action.value("key").toString();
        if(action.value("remove").toBool()){bb::platform::Notification::deleteFromInbox(key);continue;}
        bb::platform::Notification notice(key);
        notice.setTitle(m_store->chat(action.value("chatId").toString()).value("title",QString("Berrygram")).toString());
        notice.setBody(m_preferences->hidePreview()?QString::fromUtf8("Новое сообщение"):action.value("body").toString());notice.notify();
    }
    if(previous!=m_policy.unread){if(m_policy.unread>0)navigator_set_badge(NAVIGATOR_BADGE_SPLAT);else navigator_clear_badge();emit changed();}
}
