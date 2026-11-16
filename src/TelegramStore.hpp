#ifndef BERRYGRAM_TELEGRAM_STORE_HPP
#define BERRYGRAM_TELEGRAM_STORE_HPP

#include <QObject>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include "LocalOutbox.hpp"

class TelegramSession;

// Qt Core only: the same state/update logic is tested on Linux and used on BB10.
class TelegramStore : public QObject {
    Q_OBJECT
public:
    explicit TelegramStore(TelegramSession *session, QObject *parent = 0, int requestTimeoutMs = 60000);
    ~TelegramStore();
    QVariantList chats() const { return m_chatRows; }
    QVariantList messages() const { return m_messageRows; }
    QVariantList knownChats() const;
    QVariantMap chat(const QString &id) const { return m_chats.value(id); }
    void setChatCategory(const QString &category);
    int personalCount() const;
    int communityCount() const;
    QString targetMessageId() const { return m_targetMessage; }
    int targetMessageIndex() const;
    bool ready() const { return m_ready; }
    bool loadingChats() const { return m_loadingChats; }
    bool loadingHistory() const { return m_loadingHistory; }
    bool canLoadChats() const { return m_ready && !m_loadingChats && !m_chatsEnd; }
    bool canLoadHistory() const { return m_ready && !m_current.isEmpty() && !m_loadingHistory && !m_historyEnd; }
    bool canSend() const;
    bool canSendMedia(const QString &kind) const;
    QString uploadDirectory() const;
    QVariantMap failedAttachment() const;
    QVariantMap attachmentDraft() const;
    bool stageAttachment(const QString &path, const QString &kind, const QString &caption, int duration = 0);
    bool clearAttachmentDraft();
    bool recoverAttachment();
    QVariantMap uploadRecord(const QString &id) const;
    QString recoveryWarning() const;
    QString cacheSummary() const { return m_local.summary(); }
    int cleanupUploads();
    bool sendAttachment(const QString &path, const QString &kind, const QString &caption,
                        int duration = 0, int width = 0, int height = 0);
    bool sendBusy() const;
    QString currentChatTitle() const;
    QString errorText() const { return m_error; }
    QString connectionStatus() const { return m_connection; }
    QString composerHint() const;
    QString failedDraft() const;
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
    bool openChatAt(const QString &id, const QString &messageId);
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
    bool sendContent(const QVariantMap &content, const QString &text, const QVariantMap &attachment = QVariantMap());

    void suspendLocalSends();
    void restoreChatDraft(const QString &id, const QVariant &remote);
    void recordSendResult(const QVariantMap &message, const QString &recordId, const QString &outcome = QString());
    TelegramSession *m_session;
    LocalOutbox m_local;
    QString m_account;
    QMap<QString, QString> m_localMessages;
    qlonglong m_nextCleanup;
    QTimer m_expiry;
    QTimer m_draftTimer;
    QElapsedTimer m_clock;
    int m_requestTimeoutMs;
    QMap<QString, QVariantMap> m_chats, m_users, m_requests, m_outbox;
    QMap<QString, QVariantMap> m_failedAttachments, m_sentAttachments;
    QMap<QString, QString> m_failedDrafts, m_earlyAttachmentOutcomes;
    QMap<QString, QString> m_drafts, m_dirtyDrafts, m_draftInFlight;
    QMap<qlonglong, QVariantMap> m_messages, m_overlays;
    QMap<int, QVariantMap> m_files;
    QSet<int> m_downloading;
    QSet<QString> m_recoveredTexts;
    QSet<qlonglong> m_historyTouched, m_removed;
    QVariantList m_chatRows, m_messageRows;
    QString m_current, m_error, m_connection, m_seenPhase, m_search, m_chatCategory, m_targetMessage;
    qlonglong m_historyCursor;
    unsigned m_generation;
    bool m_ready, m_loadingChats, m_loadingHistory, m_chatsEnd, m_historyEnd, m_online;
    Q_DISABLE_COPY(TelegramStore)
};
#endif
