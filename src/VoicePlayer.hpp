#ifndef TELEGRAM_VOICE_PLAYER_HPP
#define TELEGRAM_VOICE_PLAYER_HPP
#include <QObject>
#include <QVariantMap>
#include <QTimer>
#include <bb/multimedia/MediaPlayer>
#include "VoiceDecoder.hpp"
class TelegramStore;
class VoicePlayer:public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(int position READ position NOTIFY changed)
    Q_PROPERTY(int duration READ duration NOTIFY changed)
    Q_PROPERTY(double speed READ speed NOTIFY changed)
public:
    explicit VoicePlayer(TelegramStore *store,QObject *parent=0);
    ~VoicePlayer();
    Q_INVOKABLE bool open(const QString &id);
    Q_INVOKABLE bool openCheckSample();
    Q_INVOKABLE void close();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void seek(int milliseconds);
    Q_INVOKABLE void setSpeed(double speed);
    QString status() const{return m_status;}
    bool playing() const;
    bool loading() const{return m_loading || m_startPending;}
    bool available() const{return !m_source.isEmpty() && !loading();}
    int position() const{return int(m_player.position());}
    int duration() const{return int(m_player.duration());}
    double speed() const{return m_speed;}
signals:void changed();
private slots:
    void decoded();
    void stateChanged();
    void startPrepared();
    void progress();
    void failed();
    void completed();
    void validate();
    void save();
private:
    void prepare(const QString &path);
    bool beginPlayback(const QString &id,const QString &path,bool checkSample);
    QString positionFile() const;
    TelegramStore *m_store;
    VoiceDecoder m_decoder;
    bb::multimedia::MediaPlayer m_player;
    QTimer m_saveTimer;
    QString m_id,m_chat,m_account,m_key,m_status,m_source,m_temporary;
    QVariantMap m_positions;
    bool m_loading,m_startPending,m_checkSample;
    int m_resume,m_prepareAttempts,m_lastPosition;
    double m_speed;
};
#endif
