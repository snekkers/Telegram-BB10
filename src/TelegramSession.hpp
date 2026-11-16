#ifndef BERRYGRAM_TELEGRAM_SESSION_HPP
#define BERRYGRAM_TELEGRAM_SESSION_HPP

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

// Qt 4/C++98 boundary. TDLib and its GNU C++ runtime live only in the child.
class TelegramSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasSavedConfiguration READ hasSavedConfiguration NOTIFY changed)
    Q_PROPERTY(QString savedApiId READ savedApiId NOTIFY changed)
    Q_PROPERTY(bool testConfigurationAvailable READ testConfigurationAvailable CONSTANT)

public:
    explicit TelegramSession(QObject *parent = 0);
    TelegramSession(const QString &engine, const QString &library,
                    const QString &dataDirectory, QObject *parent = 0);
    ~TelegramSession();
    bool available() const;
    QString phase() const { return m_phase; }
    QString status() const { return m_status; }
    QString errorText() const { return m_error; }
    QString version() const { return m_version; }
    bool busy() const { return m_busy; }
    bool hasSavedConfiguration() const;
    QString savedApiId() const;
    bool testConfigurationAvailable() const;
    QString dataDirectory() const { return m_dataDirectory; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool configure(const QString &apiId, const QString &apiHash);
    Q_INVOKABLE bool configureAndRemember(const QString &apiId, const QString &apiHash);
    Q_INVOKABLE bool useSavedConfiguration();
    Q_INVOKABLE bool useTestConfiguration();
    Q_INVOKABLE bool forgetConfiguration();
    Q_INVOKABLE bool submit(const QString &value);
    // Application API; deliberately not invokable from QML.
    QString sendReady(QVariantMap object);

signals:
    void batchStarted();
    void batchFinished();
    void changed();
    void eventReceived(const QVariantMap &object);

private slots:
    void started();
    void readOutput();
    void discardErrorOutput();
    void processError(QProcess::ProcessError error);
    void finished(int code, QProcess::ExitStatus exitStatus);
    void timedOut();

private:
    void initialize();
    void setPhase(const QString &phase, const QString &status);
    void fail(const QString &message);
    void handle(const QVariantMap &object);
    void authorization(const QVariantMap &state);
    QString request(QVariantMap object);
    bool authenticate(QVariantMap object);
    bool loadDatabaseKey(QByteArray *key);

    QProcess m_process;
    QTimer m_timer;
    QString m_engine, m_library, m_dataDirectory;
    QString m_phase, m_status, m_error, m_version, m_pending, m_versionRequest, m_authorizationType;
    QByteArray m_input;
    unsigned m_sequence;
    bool m_busy, m_stopping, m_failed, m_readQueued;
    Q_DISABLE_COPY(TelegramSession)
};
#endif
