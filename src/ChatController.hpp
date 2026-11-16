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

public:
    explicit ChatController(QObject *parent = 0);

    bb::cascades::DataModel *chats() const;
    bb::cascades::DataModel *messages() const;
    QString currentChatTitle() const;

    Q_INVOKABLE bool openChat(const QString &chatId);
    Q_INVOKABLE void closeChat();
    Q_INVOKABLE bool sendDemoMessage(const QString &text);

signals:
    void currentChatChanged();

private:
    void addDemoChat(const QString &id, const QString &title,
                     const QString &text, int order);
    void refreshChats();

    bb::cascades::GroupDataModel *m_chats;
    bb::cascades::GroupDataModel *m_messages;
    QVariantList m_chatRows;
    QMap<QString, QVariantList> m_history;
    QString m_currentChatId;
    QString m_currentChatTitle;
};

#endif
