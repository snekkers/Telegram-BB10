#ifndef BERRYGRAM_TELEGRAM_CONTROLLER_HPP
#define BERRYGRAM_TELEGRAM_CONTROLLER_HPP
#include "TelegramStore.hpp"
#include <bb/cascades/ArrayDataModel>
#include <bb/system/InvokeManager>
#include <bb/cascades/pickers/FilePicker>
#include <bb/multimedia/AudioRecorder>
#include <QTimer>

class TelegramController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bb::cascades::DataModel* chats READ chats CONSTANT)
    Q_PROPERTY(bb::cascades::DataModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString currentChatTitle READ currentChatTitle NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY changed)
    Q_PROPERTY(QString composerHint READ composerHint NOTIFY changed)
    Q_PROPERTY(QString failedDraft READ failedDraft NOTIFY changed)
    Q_PROPERTY(QString draftText READ draftText NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool loadingChats READ loadingChats NOTIFY changed)
    Q_PROPERTY(bool loadingHistory READ loadingHistory NOTIFY changed)
    Q_PROPERTY(bool canLoadChats READ canLoadChats NOTIFY changed)
    Q_PROPERTY(bool canLoadHistory READ canLoadHistory NOTIFY changed)
    Q_PROPERTY(bool canSend READ canSend NOTIFY changed)
    Q_PROPERTY(bool sendBusy READ sendBusy NOTIFY changed)
    Q_PROPERTY(int scrollSerial READ scrollSerial NOTIFY changed)
    Q_PROPERTY(QString attachmentName READ attachmentName NOTIFY changed)
    Q_PROPERTY(QString recoveryWarning READ recoveryWarning NOTIFY changed)
    Q_PROPERTY(QString cacheSummary READ cacheSummary NOTIFY changed)
    Q_PROPERTY(QString mediaError READ mediaError NOTIFY changed)
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool canAttach READ canAttach NOTIFY changed)
    Q_PROPERTY(bool canSendSelected READ canSendSelected NOTIFY changed)
    Q_PROPERTY(bool hasFailedAttachment READ hasFailedAttachment NOTIFY changed)
    Q_PROPERTY(int chatCount READ chatCount NOTIFY changed)
    Q_PROPERTY(int personalCount READ personalCount NOTIFY changed)
    Q_PROPERTY(int communityCount READ communityCount NOTIFY changed)
    Q_PROPERTY(int targetMessageIndex READ targetMessageIndex NOTIFY changed)
    Q_PROPERTY(QString targetMessageId READ targetMessageId NOTIFY changed)
public:
    explicit TelegramController(TelegramStore *store, QObject *parent = 0);
    bb::cascades::DataModel *chats() const { return m_chats; }
    bb::cascades::DataModel *messages() const { return m_messages; }
    QString currentChatTitle() const { return m_store->currentChatTitle(); }
    QString errorText() const { return m_store->errorText(); }
    QString connectionStatus() const { return m_store->connectionStatus(); }
    QString composerHint() const { return m_store->composerHint(); }
    QString failedDraft() const { return m_store->failedDraft(); }
    QString draftText() const { return m_store->draftText(); }
    bool ready() const { return m_store->ready(); }
    bool loadingChats() const { return m_store->loadingChats(); }
    bool loadingHistory() const { return m_store->loadingHistory(); }
    bool canLoadChats() const { return m_store->canLoadChats(); }
    bool canLoadHistory() const { return m_store->canLoadHistory(); }
    bool canSend() const { return m_store->canSend(); }
    bool sendBusy() const { return m_store->sendBusy(); }
    int scrollSerial() const { return m_scrollSerial; }
    int chatCount() const { return m_chats->size(); }
    int personalCount() const { return m_store->personalCount(); }
    int communityCount() const { return m_store->communityCount(); }
    int targetMessageIndex() const { return m_store->targetMessageIndex(); }
    QString targetMessageId() const { return m_store->targetMessageId(); }
    Q_INVOKABLE void setChatCategory(const QString &category) { m_store->setChatCategory(category); }
    Q_INVOKABLE bool openChatAt(const QString &id, const QString &messageId) { leaveAttachment(); return m_store->openChatAt(id,messageId); }
    Q_INVOKABLE bool latestMessages() { return openChat(m_store->currentChatId()); }
    Q_INVOKABLE bool openChat(const QString &id) { leaveAttachment(); return m_store->openChat(id); }
    Q_INVOKABLE void closeChat() { leaveAttachment(); m_store->closeChat(); }
    Q_INVOKABLE bool sendText(const QString &text) { return m_store->sendText(text); }
    Q_INVOKABLE void loadMoreChats() { m_store->loadMoreChats(); }
    Q_INVOKABLE void loadMoreHistory() { m_store->loadMoreHistory(); }
    Q_INVOKABLE QString takeFailedDraft() { return m_store->takeFailedDraft(); }
    Q_INVOKABLE void setDraft(const QString &text) { m_store->setDraft(text); }
    Q_INVOKABLE void flushDrafts() { m_store->flushDrafts(); }
    Q_INVOKABLE void setSearchQuery(const QString &text) { m_store->setSearchQuery(text); }
    Q_INVOKABLE QString nextUnreadChat() { return m_store->nextUnreadChat(); }
    Q_INVOKABLE bool markRead() { return m_store->markRead(); }
    Q_INVOKABLE bool downloadMedia(const QString &id) { return m_store->downloadMedia(id); }
    Q_INVOKABLE bool openMedia(const QString &id);
    QString attachmentName() const { return m_attachment.value("path").toString().isEmpty() ? m_attachment.value("name").toString() : m_attachment.value("path").toString().section('/', -1); }
    QString recoveryWarning() const { return m_store->recoveryWarning(); }
    QString cacheSummary() const { return m_store->cacheSummary(); }
    Q_INVOKABLE void cleanupUploads() { m_store->cleanupUploads(); }
    QString mediaError() const { return m_mediaError; }
    bool recording() const { return m_recording; }
    bool canAttach() const { return m_store->canSendMedia("document") || m_store->canSendMedia("photo") || m_store->canSendMedia("voice"); }
    bool canSendSelected() const { return !m_recording && (m_attachment.isEmpty() ? m_store->canSend() : m_store->canSendMedia(m_attachment.value("kind").toString())); }
    bool hasFailedAttachment() const { return !m_store->failedAttachment().isEmpty(); }
    Q_INVOKABLE void chooseAttachment(const QString &kind);
    Q_INVOKABLE void cancelAttachment();
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE bool sendSelected(const QString &caption);
    Q_INVOKABLE void recoverAttachment();
    Q_INVOKABLE bool previewAttachment();
signals:
    void changed();
private slots:
    void updateChats();
    void updateMessages();
    void scroll();
    void fileSelected(const QStringList &files);
    void finishRecording();
    void recordingError();
    void checkAttachmentChat();
    void leaveAttachment();
private:
    void reconcile(bb::cascades::ArrayDataModel *model, const QVariantList &rows, const QString &key);
    QString newUploadPath(const QString &name);
    TelegramStore *m_store;
    bb::cascades::ArrayDataModel *m_chats, *m_messages;
    int m_scrollSerial;
    bb::system::InvokeManager m_invoke;
    bb::cascades::pickers::FilePicker m_picker;
    bb::multimedia::AudioRecorder m_recorder;
    QTimer m_recordLimit;
    QVariantMap m_attachment;
    QString m_attachmentChat, m_pickerChat, m_pickerKind, m_mediaError;
    bool m_recording;

};
#endif
