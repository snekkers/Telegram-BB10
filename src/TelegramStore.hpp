#ifndef BERRYGRAM_TELEGRAM_STORE_HPP
#define BERRYGRAM_TELEGRAM_STORE_HPP

#include <QObject>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>

class TelegramSession;

// Qt Core only: the same state/update logic is tested on Linux and used on BB10.
class TelegramStore : public QObject {
    Q_OBJECT
public:
    explicit TelegramStore(TelegramSession *session, QObject *parent = 0, int requestTimeoutMs = 60000);
    QVariantList chats() const { return m_chatRows; }
    QVariantList messages() const { return m_messageRows; }
    bool ready() const { return m_ready; }
    bool loadingChats() const { return m_loadingChats; }
    bool loadingHistory() const { return m_loadingHistory; }
    bool canLoadChats() const { return m_ready && !m_loadingChats && !m_chatsEnd; }
    bool canLoadHistory() const { return m_ready && !m_current.isEmpty() && !m_loadingHistory && !m_historyEnd; }
    bool canSend() const;
    bool sendBusy() const;
    QString currentChatTitle() const;
    QString errorText() const { return m_error; }
    QString connectionStatus() const { return m_connection; }
    QString composerHint() const;
    QString failedDraft() const { return m_failedDrafts.value(m_current); }
    QString draftText() const { return m_drafts.value(m_current); }
    QString currentChatId() const { return m_current; }
    QString searchQuery() const { return m_search; }
    void setDraft(const QString &text);
    void setSearchQuery(const QString &text);
    QString nextUnreadChat() const;
    bool markRead();
    bool downloadMedia(const QString &messageId);
    QString mediaPath(const QString &messageId) const;
    QString mediaMime(const QString &messageId) const;

    bool openChat(const QString &id);
    void closeChat();
    void loadMoreChats();
    void loadMoreHistory();
    bool sendText(const QString &text);
    QString takeFailedDraft();

signals:
    void chatsChanged();
    void messagesChanged();
    void changed();
    void scrollRequested();

public slots:
    void consume(const QVariantMap &object);
    void flushDrafts();

private slots:
    void sessionChanged();
    void expireRequests();

private:
    QString submit(QVariantMap object, const QString &kind, const QString &chatId = QString());
    void reset();
    void rebuildChats();
    void rebuildMessages();
    void setPositions(QVariantMap *chat, const QVariantList &positions);
    void upsertMessage(const QVariantMap &message);
    QVariantMap messageRow(const QVariantMap &message) const;
    QString messageText(const QVariantMap &content) const;
    QString delivery(const QVariantMap &row) const;
    QVariantMap mediaInfo(const QVariantMap &content) const;
    QVariantMap fileInfo(const QVariantMap &file) const;
    void updateFile(const QVariantMap &file);
    void history(bool initial);
    void response(const QVariantMap &object, const QVariantMap &request);

    TelegramSession *m_session;
    QTimer m_expiry;
    QTimer m_draftTimer;
    QElapsedTimer m_clock;
    int m_requestTimeoutMs;
    QMap<QString, QVariantMap> m_chats, m_users, m_requests, m_outbox;
    QMap<QString, QString> m_failedDrafts;
    QMap<QString, QString> m_drafts, m_dirtyDrafts, m_draftInFlight;
    QMap<qlonglong, QVariantMap> m_messages, m_overlays;
    QMap<int, QVariantMap> m_files;
    QSet<int> m_downloading;
    QSet<qlonglong> m_historyTouched, m_removed;
    QVariantList m_chatRows, m_messageRows;
    QString m_current, m_error, m_connection, m_seenPhase, m_search;
    qlonglong m_historyCursor;
    unsigned m_generation;
    bool m_ready, m_loadingChats, m_loadingHistory, m_chatsEnd, m_historyEnd, m_online;
    Q_DISABLE_COPY(TelegramStore)
};
#endif
