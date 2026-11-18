#ifndef BERRYGRAM_TELEGRAM_STORE_HPP
#define BERRYGRAM_TELEGRAM_STORE_HPP

#include <QObject>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include <QStringList>
#include <QList>
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
    QVariantList contacts() const { return m_contactRows; }
    void loadContacts();
    void setContactQuery(const QString &text);
    void setContactSection(int section);
    bool contactsLoading() const { return m_contactsLoading; }
    bool openingContact() const { return m_openingContact; }
    QString contactsError() const { return m_contactsError; }
    bool openContact(const QString &userId);
    void cancelContactOpen();
    QString currentChatStatus() const;
    bool currentStatusActive() const;
    bool currentChatWide() const { const QString type=m_chats.value(m_current).value("type").toString(); return type=="chatTypeBasicGroup" || type=="chatTypeSupergroup"; }
    bool composerVisible() const {const QVariantMap current=m_chats.value(m_current);return !m_current.isEmpty() && (!current.value("isChannel").toBool() || current.value("canSend").toBool());}
    QString currentAvatarKey() const { return m_current.isEmpty()?QString():"chat:"+m_current; }
    QString avatarPath(const QString &key) const;
    QString avatarCacheDirectory() const;
    bool avatarAffected(const QString &key,const QString &changedKey) const;
    void requestAvatar(const QString &key);
    bool archiveView() const { return m_archiveView; }
    void setArchiveView(bool value);
    bool setPinned(const QString &chatId, bool value);
    bool setArchived(const QString &chatId, bool value);
    QVariantMap composerContext() const { return m_composerContext; }
    QVariantMap forwardSelection() const { return m_forward; }
    bool beginMessageAction(const QString &messageId, const QString &action);
    void cancelComposer();
    bool forwardTo(const QString &chatId);
    int selectionCount() const {return m_selected.size();}
    QString selectedText() const;
    QVariantMap selectionState() const {return m_selectionState;}
    bool toggleSelected(const QString &id);
    void clearSelection();
    bool prepareSelection(const QString &action);
    bool deleteSelected(bool revoke);

    QVariantMap reactionState() const{return m_reactionState;}
    bool beginReactions(const QString &id);
    bool chooseReaction(const QString &emoji);
    bool loadReactors(bool more=false);
    void cancelReactions();
    QVariantMap pinnedMessage() const{return m_pinnedMessage;}
    void refreshPinned();
    QVariantMap chatInfo() const;
    QVariantList infoRows() const { return m_infoRows; }
    bool infoLoading() const { return m_infoLoading; }
    bool canLoadInfo() const { return !m_infoLoading && !m_infoEnd && !m_current.isEmpty(); }
    QString infoError() const { return m_infoError; }
    void loadChatInfo();
    void setInfoSection(int section);
    void loadMoreInfo();
    bool setNotifications(bool enabled);
    bool muteFor(int seconds);
    QString diagnostics() const;
    QVariantList scheduledMessages() const {return m_scheduledRows;}
    QString scheduledStatus() const {return m_scheduledStatus;}
    bool scheduledBusy() const {return m_scheduledBusy;}
    void loadScheduled();
    void closeScheduled();
    bool changeScheduled(const QString &id,const QString &text,uint date,bool changeText);
    bool cancelScheduled(const QString &id);
    QVariantList downloads() const;
    bool cancelDownload(int fileId);
    bool retryDownload(int fileId);
    void setAutoDownload(bool enabled){m_autoDownload=enabled;}
    void setAutoDownloadPreferences(bool wifi,bool mobile){m_autoWifiAllowed=wifi;m_autoMobileAllowed=mobile;}
    void autoDownload();
    QString accountKey() const {return m_account;}
    QString dataDirectory() const;
    bool ready() const { return m_ready; }
    bool loadingChats() const { return m_loadingChats; }
    bool loadingHistory() const { return m_loadingHistory; }
    bool canLoadChats() const { return m_foreground && m_ready && !m_loadingChats && !(m_archiveView ? m_archiveEnd : m_chatsEnd); }
    bool canLoadHistory() const { return m_foreground && m_ready && !m_current.isEmpty() && !m_loadingHistory && !m_historyEnd; }
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
    QString storageSummary() const{return m_storageSummary;}
    void refreshStorage();
    int cleanupUploads();
    bool sendAttachment(const QString &path, const QString &kind, const QString &caption,
                        int duration = 0, int width = 0, int height = 0, uint scheduledAt = 0);
    bool sendBusy() const;
    QString currentChatTitle() const;
    QString errorText() const { return m_error; }
    QString connectionStatus() const { return m_connection; }
    QString composerHint() const;
    QString failedDraft() const;
    QString draftText() const { return m_drafts.value(draftKey()); }
    QString composerKey() const {return draftKey();}
    bool inComments() const { return !m_threadId.isEmpty(); }
    bool commentsLoading() const { return !m_pendingComment.isEmpty(); }
    bool openComments(const QString &messageId);
    bool returnToPost();
    QString currentChatId() const { return m_current; }
    QString searchQuery() const { return m_search; }
    void setDraft(const QString &text);
    void setSearchQuery(const QString &text);
    QString nextUnreadChat() const;
    bool markRead();
    bool foreground() const { return m_foreground; }
    void setForeground(bool value);
    void setNetwork(const QString &type, const QString &identity);
    void setFollowingLatest(bool value) { m_followingLatest=value; }
    bool downloadMedia(const QString &messageId);
    bool requestMediaProperties(const QString &messageId);
    QString mediaPath(const QString &messageId) const;
    QString mediaMime(const QString &messageId) const;

    bool openChat(const QString &id);
    void saveViewState(const QString &messageId,int cursor);
    int draftCursor() const;
    void clearViewPosition();
    bool openChatAt(const QString &id, const QString &messageId);
    void closeChat();
    void loadMoreChats();
    bool canLoadRecipientChats() const { return m_foreground && m_ready && !m_loadingChats && (!m_chatsEnd || !m_archiveEnd); }
    void loadMoreRecipientChats();
    void loadMoreHistory();
    bool sendText(const QString &text, uint scheduledAt = 0);
    QString takeFailedDraft();

signals:
    void chatsChanged();
    void messagesChanged();
    void changed();
    void scrollRequested();
    void incomingMessage();
    void forwardReady();
    void infoChanged();
    void recipientChatsChanged();
    void contactsChanged();
    void contactChatReady(const QString &chatId);
    void mediaProperties(const QString &messageId,const QVariantMap &properties);
    void avatarChanged(const QString &key);
    void scheduledChanged();
    void downloadsChanged();

public slots:
    void consume(const QVariantMap &object);
    void flushDrafts();

private slots:
    void beginBatch();
    void endBatch();
    void sessionChanged();
    void expireRequests();
    void peopleTick();

private:
    QString submit(QVariantMap object, const QString &kind, const QString &chatId = QString());
    void reset();
    void sendNetworkType(bool force = false);
    void resumeHistoryRequests();
    void rebuildChats();
    void rebuildMessages();
    void notifyChatUpdates();
    void requestScroll();
    void notifyIncoming();
    void setPositions(QVariantMap *chat, const QVariantList &positions);
    void upsertMessage(const QVariantMap &message);
    QVariantMap messageRow(const QVariantMap &message) const;
    QString messageText(const QVariantMap &content) const;
    QString delivery(const QVariantMap &row) const;
    QVariantMap mediaInfo(const QVariantMap &content) const;
    QVariantMap fileInfo(const QVariantMap &file) const;
    void updateFile(const QVariantMap &file);
    void history(bool initial);
    void retrySlowHistory();
    void retireHistoryRequests();
    void loadChats(bool archive, int limit = 40);
    void response(const QVariantMap &object, const QVariantMap &request);
    bool sendContent(const QVariantMap &content, const QString &text, const QVariantMap &attachment = QVariantMap(), uint scheduledAt = 0);
    bool editText(const QString &text);
    void applyMessageAction(const QString &messageId, const QString &action);
    void updateInfoRows();
    void updateForwardStatus();
    bool actionResponse(const QVariantMap &object, const QVariantMap &pending);
    bool reactionResponse(const QVariantMap &object,const QVariantMap &pending);
    bool toolsResponse(const QVariantMap &object,const QVariantMap &pending);
    void updateDownload(const QVariantMap &file);
    void updateReactionNames();
    bool peopleResponse(const QVariantMap &object,const QVariantMap &pending);
    bool peopleUpdate(const QVariantMap &object);
    void upsertUser(const QVariantMap &user);
    void rebuildContacts();
    void rememberAvatar(const QString &key,const QVariantMap &photo);
    void updateAvatarFile(const QVariantMap &file);
    QVariantMap avatarFile(const QString &key) const;
    void pumpAvatars();
    QString chatActivity(const QString &chatId) const;
    QString draftKey() const { return m_threadId.isEmpty()?m_current:m_current+"/thread/"+m_threadId; }
    bool threadResponse(const QVariantMap &object,const QVariantMap &pending);
    bool belongsToThread(const QVariantMap &message) const;
    void activateThread(const QVariantMap &thread,const QVariantMap &pending);
    void applyReplyInfo(QVariantMap *row,const QVariantMap &interaction) const;

    void suspendLocalSends();
    void restoreChatDraft(const QString &id, const QVariant &remote, bool preserveRemote = false);
    void recordSendResult(const QVariantMap &message, const QString &recordId, const QString &outcome = QString());
    TelegramSession *m_session;
    LocalOutbox m_local;
    QVariantMap m_viewState;
    bool m_viewStateValid;
    QString m_account;
    QMap<QString, QString> m_localMessages;
    qlonglong m_nextCleanup;
    QVariantList m_scheduledRows;
    QString m_scheduledStatus;
    bool m_scheduledBusy,m_scheduledOpen,m_autoDownload;
    bool m_autoWifiAllowed,m_autoMobileAllowed;
    unsigned m_scheduledSerial;
    QMap<int,QVariantMap> m_downloads;
    unsigned m_downloadSerial;
    QSet<int> m_autoSuppressed;
    quint64 m_completedRequests,m_timedOutRequests,m_requestLatency;
    QTimer m_expiry;
    QTimer m_draftTimer;
    QTimer m_peopleTimer;
    bool m_batching, m_chatsDirty, m_messagesDirty, m_chatUpdatesPending;
    bool m_followingLatest, m_scrollPending, m_historyLocalEnd;
    int m_pendingIncoming;
    QElapsedTimer m_clock;
    int m_requestTimeoutMs;
    QMap<QString, QVariantMap> m_chats, m_users, m_requests, m_outbox;
    QMap<QString, QVariantMap> m_failedAttachments, m_sentAttachments, m_earlyForwards;
    QMap<QString, QString> m_failedDrafts, m_earlyAttachmentOutcomes;
    QMap<QString, QString> m_drafts, m_dirtyDrafts, m_draftInFlight;
    QMap<qlonglong, QVariantMap> m_messages, m_overlays;
    QMap<int, QVariantMap> m_files;
    QSet<int> m_downloading;
    QSet<qlonglong> m_selected;
    QVariantMap m_selectionState;
    QSet<QString> m_recoveredTexts;
    QSet<qlonglong> m_historyTouched, m_removed;
    QSet<QString> m_historyRequests;
    QVariantMap m_historyRetryCall;
    qlonglong m_historyRetryAt;
    QVariantList m_chatRows, m_messageRows;
    QVariantList m_contactRows;
    QStringList m_contactIds;
    QString m_contactQuery,m_contactsError,m_pendingContactChat;
    bool m_contactsLoading,m_openingContact;
    int m_contactSection;
    unsigned m_contactSerial;
    QMap<QString,QVariantMap> m_avatarFiles,m_chatActions;
    QList<int> m_avatarQueue;
    QSet<int> m_avatarQueued;
    QMap<int,qlonglong> m_avatarActive,m_avatarFailed;
    qlonglong m_nextPresenceRefresh;
    QVariantMap m_composerContext, m_forward, m_info, m_pinnedMessage, m_reactionState;
    unsigned m_reactionSerial;
    unsigned m_pinnedSerial;
    QVariantList m_infoRows, m_members, m_shared;
    QString m_infoError, m_infoCursor;
    int m_infoSection, m_memberOffset;
    unsigned m_actionSerial, m_infoSerial;
    QString m_storageSummary;
    QString m_current, m_error, m_connection, m_seenPhase, m_search, m_chatCategory, m_targetMessage;
    QString m_threadId,m_threadRoot,m_threadSourceChat,m_threadSourceMessage,m_pendingComment;
    qlonglong m_historyCursor;
    qlonglong m_lastReadSubmitted;
    unsigned m_generation;
    bool m_foreground;
    QString m_networkType, m_networkIdentity, m_sentNetwork;
    bool m_ready, m_loadingChats, m_loadingHistory, m_chatsEnd, m_historyEnd, m_online;
    bool m_archiveView, m_archiveEnd, m_infoLoading, m_infoEnd;
    Q_DISABLE_COPY(TelegramStore)
};
#endif
