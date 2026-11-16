#ifndef CHATCONTROLLER_HPP
#define CHATCONTROLLER_HPP

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>
#include <bb/cascades/DataModel>
#include <bb/cascades/GroupDataModel>

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

public:
    explicit ChatController(QObject *parent = 0);

    bb::cascades::DataModel *chats() const;
    bb::cascades::DataModel *messages() const;
    QString currentChatTitle() const;

    Q_INVOKABLE bool openChat(const QString &chatId);
    Q_INVOKABLE bool openChatAt(const QString &chatId, const QString &messageId);
    Q_INVOKABLE void setChatCategory(const QString &category);
    QVariantList search(const QString &query, int mode, int filter) const;
    int chatCount() const;
    int personalCount() const;
    int communityCount() const { return m_chatRows.size() - personalCount(); }
    int targetMessageIndex() const;
    QString targetMessageId() const { return m_targetMessage; }
    Q_INVOKABLE void closeChat();
    Q_INVOKABLE bool sendDemoMessage(const QString &text);

signals:
    void currentChatChanged();

private:
    void addDemoChat(const QString &id, const QString &title,
                     const QString &text, int order);
    void refreshChats();
    void refreshMessages();

    bb::cascades::GroupDataModel *m_chats;
    bb::cascades::GroupDataModel *m_messages;
    QVariantList m_chatRows;
    QMap<QString, QVariantList> m_history;
    QString m_currentChatId;
    QString m_currentChatTitle;
    QString m_category, m_targetMessage;
};

#endif
