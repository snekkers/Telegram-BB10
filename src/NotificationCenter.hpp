#ifndef BERRYGRAM_NOTIFICATION_CENTER_HPP
#define BERRYGRAM_NOTIFICATION_CENTER_HPP
#include <QObject>
#include "NotificationPolicy.hpp"
class TelegramSession;class TelegramStore;class Preferences;
class NotificationCenter:public QObject {
    Q_OBJECT
    Q_PROPERTY(int unread READ unread NOTIFY changed)
public:
    NotificationCenter(TelegramSession *session,TelegramStore *store,Preferences *preferences,QObject *parent=0);
    int unread() const{return m_policy.unread;}
signals:void changed();
private slots:
    void consume(const QVariantMap &event);
    void foreground();void background();void sessionChanged();void preferencesChanged();
private:
    TelegramSession *m_session;TelegramStore *m_store;Preferences *m_preferences;NotificationPolicy m_policy;bool m_foreground,m_wasReady,m_hidePreview;
};
#endif
