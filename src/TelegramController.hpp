#ifndef BERRYGRAM_TELEGRAM_CONTROLLER_HPP
#define BERRYGRAM_TELEGRAM_CONTROLLER_HPP
#include "TelegramStore.hpp"
#include <bb/cascades/ArrayDataModel>
#include <bb/system/InvokeManager>
#include <bb/cascades/pickers/FilePicker>
#include <bb/multimedia/AudioRecorder>
#include <QTimer>
#include <QPointer>

class TelegramController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bb::cascades::DataModel* chats READ chats CONSTANT)
    Q_PROPERTY(bb::cascades::DataModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString currentChatTitle READ currentChatTitle NOTIFY changed)
    Q_PROPERTY(QString currentChatId READ currentChatId NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY changed)
    Q_PROPERTY(QString composerHint READ composerHint NOTIFY changed)
    Q_PROPERTY(QString failedDraft READ failedDraft NOTIFY changed)
    Q_PROPERTY(QString draftText READ draftText NOTIFY changed)
    Q_PROPERTY(int draftCursor READ draftCursor NOTIFY changed)
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
    Q_PROPERTY(QString storageSummary READ storageSummary NOTIFY changed)
    Q_PROPERTY(QString mediaError READ mediaError NOTIFY changed)
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool canAttach READ canAttach NOTIFY changed)
    Q_PROPERTY(bool canSendSelected READ canSendSelected NOTIFY changed)
    Q_PROPERTY(bool hasFailedAttachment READ hasFailedAttachment NOTIFY changed)
    Q_PROPERTY(int chatCount READ chatCount NOTIFY changed)
    Q_PROPERTY(int messageCount READ messageCount NOTIFY changed)
    Q_PROPERTY(int personalCount READ personalCount NOTIFY changed)
    Q_PROPERTY(int communityCount READ communityCount NOTIFY changed)
    Q_PROPERTY(int targetMessageIndex READ targetMessageIndex NOTIFY changed)
    Q_PROPERTY(QString targetMessageId READ targetMessageId NOTIFY changed)
    Q_PROPERTY(bool archiveView READ archiveView NOTIFY changed)
    Q_PROPERTY(QVariantMap composerContext READ composerContext NOTIFY changed)
    Q_PROPERTY(QVariantMap forwardSelection READ forwardSelection NOTIFY changed)
    Q_PROPERTY(QVariantMap chatInfo READ chatInfo NOTIFY changed)
    Q_PROPERTY(bb::cascades::DataModel* infoRows READ infoRows CONSTANT)
    Q_PROPERTY(bb::cascades::DataModel* recipients READ recipients CONSTANT)
    Q_PROPERTY(bool infoLoading READ infoLoading NOTIFY changed)
    Q_PROPERTY(bool canLoadInfo READ canLoadInfo NOTIFY changed)
    Q_PROPERTY(QString infoError READ infoError NOTIFY changed)
    Q_PROPERTY(int infoCount READ infoCount NOTIFY changed)
    Q_PROPERTY(int incomingSerial READ incomingSerial NOTIFY changed)
    Q_PROPERTY(int forwardSerial READ forwardSerial NOTIFY changed)
    Q_PROPERTY(int recipientCount READ recipientCount NOTIFY changed)
    Q_PROPERTY(bool canLoadRecipientChats READ canLoadRecipientChats NOTIFY changed)
    Q_PROPERTY(bb::cascades::DataModel* contacts READ contacts CONSTANT)
    Q_PROPERTY(int contactCount READ contactCount NOTIFY changed)
    Q_PROPERTY(bool contactsLoading READ contactsLoading NOTIFY changed)
    Q_PROPERTY(bool openingContact READ openingContact NOTIFY changed)
    Q_PROPERTY(QString contactsError READ contactsError NOTIFY changed)
    Q_PROPERTY(QString currentChatStatus READ currentChatStatus NOTIFY changed)
    Q_PROPERTY(bool currentStatusActive READ currentStatusActive NOTIFY changed)
    Q_PROPERTY(QString currentAvatarKey READ currentAvatarKey NOTIFY changed)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY changed)
    Q_PROPERTY(QVariantMap selectionState READ selectionState NOTIFY changed)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY changed)
    Q_PROPERTY(bool composerVisible READ composerVisible NOTIFY changed)
public:
    explicit TelegramController(TelegramStore *store, QObject *parent = 0);
    bb::cascades::DataModel *chats() const { return m_chats; }
    bb::cascades::DataModel *messages() const { return m_messages; }
    QString currentChatId() const{return m_store->currentChatId();}
    QString currentChatTitle() const { return m_store->currentChatTitle(); }
    QString errorText() const { return m_store->errorText(); }
    QString connectionStatus() const { return m_store->connectionStatus(); }
    QString composerHint() const { return m_store->composerHint(); }
    QString failedDraft() const { return m_store->failedDraft(); }
    QString draftText() const { return m_store->draftText(); }
    int draftCursor() const {return m_store->draftCursor();}
    Q_INVOKABLE void saveViewState(const QString &id,int cursor){m_store->saveViewState(id,cursor);}
    Q_INVOKABLE bool openReply(const QString &chat,const QString &id){return m_store->openChatAt(chat.isEmpty() || chat=="0"?m_store->currentChatId():chat,id);}
    bool ready() const { return m_store->ready(); }
    bool loadingChats() const { return m_store->loadingChats(); }
    bool loadingHistory() const { return m_store->loadingHistory(); }
    bool canLoadChats() const { return m_store->canLoadChats(); }
    bool canLoadHistory() const { return m_store->canLoadHistory(); }
    bool canSend() const { return m_store->canSend(); }
    bool sendBusy() const { return m_store->sendBusy(); }
    int scrollSerial() const { return m_scrollSerial; }
    int messageCount() const { return m_messages->size(); }
    int chatCount() const { return m_chats->size(); }
    int personalCount() const { return m_store->personalCount(); }
    int communityCount() const { return m_store->communityCount(); }
    int targetMessageIndex() const { return m_store->targetMessageIndex(); }
    QString targetMessageId() const { return m_store->targetMessageId(); }
    Q_INVOKABLE void setChatCategory(const QString &category) { m_store->setChatCategory(category); }
    Q_INVOKABLE bool openChatAt(const QString &id, const QString &messageId) { leaveAttachment(); return m_store->openChatAt(id,messageId); }
    Q_INVOKABLE bool latestMessages() { m_store->clearViewPosition(); return openChat(m_store->currentChatId()); }
    bool archiveView() const { return m_store->archiveView(); }
    Q_INVOKABLE void setArchiveView(bool value) { m_store->setArchiveView(value); }
    Q_INVOKABLE bool setPinned(const QString &id,bool value) { return m_store->setPinned(id,value); }
    Q_INVOKABLE bool setArchived(const QString &id,bool value) { return m_store->setArchived(id,value); }
    QVariantMap composerContext() const { return m_store->composerContext(); }
    QVariantMap forwardSelection() const { return m_store->forwardSelection(); }
    QVariantMap chatInfo() const { return m_store->chatInfo(); }
    bb::cascades::DataModel *infoRows() const { return m_infoRows; }
    bb::cascades::DataModel *recipients() const { return m_recipients; }
    bool infoLoading() const { return m_store->infoLoading(); }
    bool canLoadInfo() const { return m_store->canLoadInfo(); }
    QString infoError() const { return m_store->infoError(); }
    int infoCount() const { return m_infoRows->size(); }
    int incomingSerial() const { return m_incomingSerial; }
    int forwardSerial() const { return m_forwardSerial; }
    int recipientCount() const { return m_recipients->size(); }
    bool canLoadRecipientChats() const { return m_store->canLoadRecipientChats(); }
    Q_INVOKABLE void loadMoreRecipientChats() { m_store->loadMoreRecipientChats(); }
    int selectionCount() const{return m_store->selectionCount();}
    QVariantMap selectionState() const{return m_store->selectionState();}
    QString selectedText() const{return m_store->selectedText();}
    Q_INVOKABLE bool toggleSelected(const QString &id){return m_store->toggleSelected(id);}
    Q_INVOKABLE void clearSelection(){m_store->clearSelection();}
    Q_INVOKABLE bool prepareSelection(const QString &action){return m_store->prepareSelection(action);}
    Q_INVOKABLE bool deleteSelected(bool revoke){return m_store->deleteSelected(revoke);}
    Q_INVOKABLE bool beginMessageAction(const QString &id,const QString &action);
    Q_INVOKABLE void cancelComposer() { m_store->cancelComposer(); }
    Q_INVOKABLE bool forwardTo(const QString &id) { return m_store->forwardTo(id); }
    Q_INVOKABLE void loadChatInfo() { m_store->loadChatInfo(); }
    Q_INVOKABLE void setInfoSection(int section) { m_store->setInfoSection(section); }
    Q_INVOKABLE void loadMoreInfo() { m_store->loadMoreInfo(); }
    Q_INVOKABLE bool setNotifications(bool value) { return m_store->setNotifications(value); }
    Q_INVOKABLE void refreshRecipients();
    Q_INVOKABLE void setRecipientQuery(const QString &query) { const QString next=query.trimmed().left(128);if(next==m_recipientQuery)return;m_recipientQuery=next;refreshRecipients(); }
    bb::cascades::DataModel *contacts() const {return m_contacts;}
    int contactCount() const {return m_contacts->size();}
    bool contactsLoading() const {return m_store->contactsLoading();}
    bool openingContact() const {return m_store->openingContact();}
    QString contactsError() const {return m_store->contactsError();}
    QString currentChatStatus() const {return m_store->currentChatStatus();}
    bool currentStatusActive() const {return m_store->currentStatusActive();}
    QString currentAvatarKey() const {return m_store->currentAvatarKey();}
    bool composerVisible() const {return m_store->composerVisible();}
    Q_INVOKABLE void loadContacts(){m_store->loadContacts();}
    Q_INVOKABLE void setContactQuery(const QString &text){m_store->setContactQuery(text);}
    Q_INVOKABLE void setContactSection(int section){m_store->setContactSection(section);}
    Q_INVOKABLE bool openContact(const QString &id,QObject *target);
    Q_INVOKABLE void cancelContactOpen();
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
    Q_INVOKABLE bool openLink(const QString &url);
    QString attachmentName() const { return m_attachment.value("path").toString().isEmpty() ? m_attachment.value("name").toString() : m_attachment.value("path").toString().section('/', -1); }
    QString recoveryWarning() const { return m_store->recoveryWarning(); }
    QString cacheSummary() const { return m_store->cacheSummary(); }
    QString storageSummary() const{return m_store->storageSummary();}
    Q_INVOKABLE void refreshStorage(){m_store->refreshStorage();}
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
    void updateContacts();
    void contactReady(const QString &id);
    void updateInfoRows();
    void incoming();
    void forwardReady();
    void updateRecipientRows() { refreshRecipients(); }
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
    bb::cascades::ArrayDataModel *m_infoRows, *m_recipients;
    int m_scrollSerial, m_incomingSerial, m_forwardSerial;
    bb::system::InvokeManager m_invoke;
    bb::cascades::pickers::FilePicker m_picker;
    bb::multimedia::AudioRecorder m_recorder;
    QTimer m_recordLimit;
    QVariantMap m_attachment;
    bb::cascades::ArrayDataModel *m_contacts;
    QPointer<QObject> m_contactTarget;
    QString m_recipientQuery;
    QString m_attachmentChat, m_pickerChat, m_pickerKind, m_mediaError;
    bool m_recording;

};
#endif
