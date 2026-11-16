#ifndef BERRYGRAM_MEDIA_CONTROLLER_HPP
#define BERRYGRAM_MEDIA_CONTROLLER_HPP
#include <QObject>
#include <QVariant>
class TelegramStore;class ChatController;
class MediaController:public QObject {
    Q_OBJECT
    Q_PROPERTY(QString photoSource READ photoSource NOTIFY changed)
    Q_PROPERTY(QString caption READ caption NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString position READ position NOTIFY changed)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY changed)
    Q_PROPERTY(bool canNext READ canNext NOTIFY changed)
    Q_PROPERTY(bool canSave READ canSave NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
public:
    MediaController(TelegramStore *store,ChatController *demo,QObject *parent=0);
    Q_INVOKABLE bool openPhoto(const QString &id,bool live);
    Q_INVOKABLE void closePhoto();
    Q_INVOKABLE void movePhoto(int delta);
    Q_INVOKABLE bool savePhoto();
    Q_INVOKABLE QString thumbnail(const QString &url);
    Q_INVOKABLE void clearCache();
    QString photoSource() const{return m_source;}QString caption() const{return m_caption;}
    QString status() const{return m_status;}QString position() const;
    bool canPrevious() const{return m_index>0;}bool canNext() const{return m_index>=0 && m_index+1<m_photos.size();}
    bool canSave() const{return m_canSave && !m_source.isEmpty();}
    bool loading() const{return !m_id.isEmpty() && m_source.isEmpty();}
signals:void changed();
private slots:void refresh();void properties(const QString &id,const QVariantMap &properties);
private:
    QString folder() const;QString original() const;void select(int index);
    TelegramStore *m_store;ChatController *m_demo;bool m_live,m_canSave;int m_index;
    QString m_id,m_source,m_caption,m_status,m_chat;QVariantList m_photos;
};
#endif
