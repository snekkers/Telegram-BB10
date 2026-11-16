#ifndef BERRYGRAM_AVATAR_IMAGES_HPP
#define BERRYGRAM_AVATAR_IMAGES_HPP
#include <QObject>
#include <QString>
#include <QMap>
#include <QPointer>
class TelegramStore;

#include "AvatarRenderer.hpp"
class AvatarProvider : public QObject {
    Q_OBJECT
public:
    explicit AvatarProvider(TelegramStore *store,QObject *parent=0);
    QString url(const QString &key);
    void request(const QString &key);
    bool affected(const QString &key,const QString &changedKey) const;
signals:
    void avatarChanged(const QString &key);
private:
    TelegramStore *m_store;
    AvatarRenderer m_renderer;
};
class AvatarImage : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* provider READ provider WRITE setProvider)
    Q_PROPERTY(QString key READ key WRITE setKey)
    Q_PROPERTY(QString source READ source NOTIFY changed)
public:
    explicit AvatarImage(QObject *parent=0):QObject(parent){}
    QObject *provider() const {return m_provider.data();}
    void setProvider(QObject *provider);
    QString key() const {return m_key;}
    void setKey(const QString &key);
    QString source() const {return m_source;}
signals:
    void changed();
private slots:
    void refresh(const QString &changedKey=QString());
private:
    QPointer<AvatarProvider> m_provider;
    QString m_key,m_source;
};
#endif
