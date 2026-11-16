#ifndef BERRYGRAM_TELEGRAM_SEARCH_HPP
#define BERRYGRAM_TELEGRAM_SEARCH_HPP
#include <QObject>
#include <QVariantList>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <QStringList>
class TelegramSession;
class TelegramStore;

// Qt Core search state: independent of the open conversation and its history.
class TelegramSearch : public QObject {
    Q_OBJECT
public:
    TelegramSearch(TelegramSession *session, TelegramStore *store, QObject *parent = 0, int timeoutMs = 30000);
    QVariantList rows() const { return m_rows; }
    QString errorText() const { return m_error; }
    bool loading() const;
    bool canLoadMore() const;
    bool ready() const;
    void search(const QString &query, int mode, int filter = 0);
    void cancel();
    void loadMore();
    void retry();
signals:
    void changed();
private slots:
    void run();
    void consume(const QVariantMap &object);
    void sessionChanged();
    void expire();
    void rebuild();
private:
    bool send(QVariantMap call, const QString &kind, const QString &chatId = QString());
    void hydrate(const QString &id);
    void addChat(const QString &id);
    void messages();
    TelegramSession *m_session;
    TelegramStore *m_store;
    QTimer m_debounce, m_expiry;
    QElapsedTimer m_clock;
    QMap<QString, QVariantMap> m_requests;
    QSet<QString> m_hydrated, m_messageKeys;
    QStringList m_ids;
    QVariantList m_messages, m_rows;
    QString m_query, m_offset, m_error;
    unsigned m_epoch;
    int m_mode, m_filter, m_timeoutMs;
    bool m_active;
};
#endif
