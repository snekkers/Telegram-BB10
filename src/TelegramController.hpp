#ifndef BERRYGRAM_TELEGRAM_CONTROLLER_HPP
#define BERRYGRAM_TELEGRAM_CONTROLLER_HPP
#include "TelegramStore.hpp"
#include <bb/cascades/ArrayDataModel>
#include <bb/system/InvokeManager>

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
    Q_PROPERTY(int chatCount READ chatCount NOTIFY changed)
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
    Q_INVOKABLE bool openChat(const QString &id) { return m_store->openChat(id); }
    Q_INVOKABLE void closeChat() { m_store->closeChat(); }
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
signals:
    void changed();
private slots:
    void updateChats();
    void updateMessages();
    void scroll();
private:
    void reconcile(bb::cascades::ArrayDataModel *model, const QVariantList &rows, const QString &key);
    TelegramStore *m_store;
    bb::cascades::ArrayDataModel *m_chats, *m_messages;
    int m_scrollSerial;
    bb::system::InvokeManager m_invoke;
};
#endif
