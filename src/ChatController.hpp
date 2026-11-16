#ifndef CHATCONTROLLER_HPP
#define CHATCONTROLLER_HPP

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QTimer>
#include <bb/cascades/DataModel>
#include <bb/cascades/GroupDataModel>
#include <bb/cascades/ArrayDataModel>

// Offline data for the first simulator build. No Telegram session is created.
class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bb::cascades::DataModel* chats READ chats CONSTANT)
    Q_PROPERTY(bb::cascades::DataModel* messages READ messages CONSTANT)
    Q_PROPERTY(QString currentChatTitle READ currentChatTitle NOTIFY currentChatChanged)
    Q_PROPERTY(int chatCount READ chatCount NOTIFY currentChatChanged)
    Q_PROPERTY(int personalCount READ personalCount NOTIFY currentChatChanged)
    Q_PROPERTY(int communityCount READ communityCount NOTIFY currentChatChanged)
    Q_PROPERTY(int targetMessageIndex READ targetMessageIndex NOTIFY currentChatChanged)
    Q_PROPERTY(QString targetMessageId READ targetMessageId NOTIFY currentChatChanged)

    Q_PROPERTY(bool archiveView READ archiveView NOTIFY currentChatChanged)
    Q_PROPERTY(QVariantMap composerContext READ composerContext NOTIFY currentChatChanged)
    Q_PROPERTY(QVariantMap forwardSelection READ forwardSelection NOTIFY currentChatChanged)
    Q_PROPERTY(QVariantMap chatInfo READ chatInfo NOTIFY currentChatChanged)
    Q_PROPERTY(bb::cascades::DataModel* recipients READ recipients CONSTANT)
    Q_PROPERTY(bb::cascades::DataModel* infoRows READ infoRows CONSTANT)
    Q_PROPERTY(bool infoLoading READ infoLoading NOTIFY currentChatChanged)
    Q_PROPERTY(bool canLoadInfo READ canLoadInfo NOTIFY currentChatChanged)
    Q_PROPERTY(QString infoError READ infoError NOTIFY currentChatChanged)
    Q_PROPERTY(int infoCount READ infoCount NOTIFY currentChatChanged)
    Q_PROPERTY(int incomingSerial READ incomingSerial NOTIFY currentChatChanged)
    Q_PROPERTY(int forwardSerial READ forwardSerial NOTIFY currentChatChanged)
    Q_PROPERTY(int recipientCount READ recipientCount NOTIFY currentChatChanged)
    Q_PROPERTY(bool canLoadRecipientChats READ canLoadRecipientChats NOTIFY currentChatChanged)

    Q_PROPERTY(bb::cascades::DataModel* contacts READ contacts CONSTANT)
    Q_PROPERTY(int contactCount READ contactCount NOTIFY currentChatChanged)
    Q_PROPERTY(bool contactsLoading READ contactsLoading NOTIFY currentChatChanged)
    Q_PROPERTY(bool openingContact READ openingContact NOTIFY currentChatChanged)
    Q_PROPERTY(QString contactsError READ contactsError NOTIFY currentChatChanged)
    Q_PROPERTY(QString currentChatStatus READ currentChatStatus NOTIFY currentChatChanged)
    Q_PROPERTY(bool currentStatusActive READ currentStatusActive NOTIFY currentChatChanged)
    Q_PROPERTY(QString currentAvatarKey READ currentAvatarKey NOTIFY currentChatChanged)
    Q_PROPERTY(bool composerVisible READ composerVisible NOTIFY currentChatChanged)

public:
    explicit ChatController(QObject *parent = 0);

    bb::cascades::DataModel *chats() const;
    bb::cascades::DataModel *messages() const;
    QString currentChatTitle() const;

    bool archiveView() const { return m_archiveView; }
    Q_INVOKABLE void setArchiveView(bool value) { m_archiveView=value;refreshChats(); }
    Q_INVOKABLE bool setPinned(const QString &id,bool value);
    Q_INVOKABLE bool setArchived(const QString &id,bool value);
    QVariantMap composerContext() const { return m_context; }
    QVariantMap forwardSelection() const { return m_forward; }
    QVariantMap chatInfo() const;
    bb::cascades::DataModel *recipients() const { return m_recipients; }
    bb::cascades::DataModel *infoRows() const { return m_infoRows; }
    bool infoLoading() const { return false; }
    bool canLoadInfo() const { return false; }
    QString infoError() const { return QString(); }
    int infoCount() const { return m_infoRows->size(); }
    int incomingSerial() const { return m_incomingSerial; }
    int forwardSerial() const { return m_forwardSerial; }
    int recipientCount() const { return m_recipients->childCount(QVariantList()); }
    bool canLoadRecipientChats() const { return false; }
    Q_INVOKABLE void loadMoreRecipientChats() {}
    Q_INVOKABLE bool beginMessageAction(const QString &id,const QString &action);
    Q_INVOKABLE void cancelComposer() { m_context.clear();emit currentChatChanged(); }
    Q_INVOKABLE bool forwardTo(const QString &id);
    Q_INVOKABLE void loadChatInfo() { setInfoSection(0); }
    Q_INVOKABLE void setInfoSection(int section);
    Q_INVOKABLE void loadMoreInfo() {}
    Q_INVOKABLE bool setNotifications(bool value) { m_muted[m_currentChatId]=!value;emit currentChatChanged();return true; }
    Q_INVOKABLE void setRecipientQuery(const QString &query) { const QString next=query.trimmed().left(128);if(next==m_recipientQuery)return;m_recipientQuery=next;refreshChats(); }
    Q_INVOKABLE void refreshRecipients() { refreshChats(); }
    bool injectIncomingDemo();
    bb::cascades::DataModel *contacts() const {return m_contacts;}
    int contactCount() const {return m_contacts->size();}
    bool contactsLoading() const {return false;}
    bool openingContact() const {return false;}
    QString contactsError() const {return QString();}
    QString currentChatStatus() const;
    bool currentStatusActive() const;
    QString currentAvatarKey() const;
    bool composerVisible() const {return m_currentChatId!="demo-design";}
    Q_INVOKABLE void loadContacts();
    Q_INVOKABLE void setContactQuery(const QString &text){const QString next=text.trimmed().left(128);if(next==m_contactQuery)return;m_contactQuery=next;refreshContacts();}
    Q_INVOKABLE void setContactSection(int section){m_contactSection=section;refreshContacts();}
    Q_INVOKABLE bool openContact(const QString &id,QObject *target);
    Q_INVOKABLE void cancelContactOpen(){}
    void demoPresence(const QString &status);
    Q_INVOKABLE bool openChat(const QString &chatId);
    Q_INVOKABLE bool openChatAt(const QString &chatId, const QString &messageId);
    Q_INVOKABLE void setChatCategory(const QString &category);
    QVariantList search(const QString &query, int mode, int filter) const;
    int chatCount() const;
    int personalCount() const;
    int communityCount() const;
    int targetMessageIndex() const;
    QString targetMessageId() const { return m_targetMessage; }
    Q_INVOKABLE void closeChat();
    Q_INVOKABLE bool sendDemoMessage(const QString &text);

signals:
    void currentChatChanged();

private slots:
    void clearDemoTyping();
private:
    void refreshContacts();
    void addDemoChat(const QString &id, const QString &title,
                     const QString &text, int order);
    void refreshChats();
    void refreshMessages();

    bb::cascades::GroupDataModel *m_chats;
    bb::cascades::ArrayDataModel *m_messages, *m_infoRows;
    bb::cascades::GroupDataModel *m_recipients;
    QVariantMap m_context,m_forward;
    QMap<QString,bool> m_muted;
    bb::cascades::ArrayDataModel *m_contacts;
    QTimer m_typingTimer;
    QString m_contactQuery,m_demoStatus;
    int m_contactSection;
    bool m_archiveView;
    int m_infoSection,m_incomingSerial,m_forwardSerial;
    QVariantList m_chatRows;
    QMap<QString, QVariantList> m_history;
    QString m_currentChatId;
    QString m_currentChatTitle;
    QString m_category, m_targetMessage,m_recipientQuery;
};

#endif
