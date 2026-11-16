#include "ChatController.hpp"

#include <bb/cascades/ItemGrouping>
#include <QDateTime>
#include <QStringList>

namespace {

QVariantMap message(const QString &author, const QString &text, bool outgoing, int order)
{
    QVariantMap row;
    row.insert("author", author);
    row.insert("text", text);
    row.insert("outgoing", outgoing);
    row.insert("order", order);
    row.insert("time", QDateTime::currentDateTime().toString("HH:mm"));
    return row;
}

}

ChatController::ChatController(QObject *parent)
    : QObject(parent)
    , m_chats(new bb::cascades::GroupDataModel(QStringList() << "order", this))
    , m_messages(new bb::cascades::GroupDataModel(QStringList() << "order", this))
{
    m_chats->setGrouping(bb::cascades::ItemGrouping::None);
    m_messages->setGrouping(bb::cascades::ItemGrouping::None);
    addDemoChat("demo-saved", QString::fromUtf8("Избранное"),
                QString::fromUtf8("Здесь можно проверить ввод с клавиатуры Classic."), 0);
    addDemoChat("demo-bb10", "BlackBerry Classic",
                QString::fromUtf8("Проверяем список, прокрутку и кнопку «Назад»."), 1);
    addDemoChat("demo-project", QString::fromUtf8("Разработка"),
                QString::fromUtf8("Следующий этап — подключение Telegram."), 2);
    refreshChats();
}

bb::cascades::DataModel *ChatController::chats() const
{
    return m_chats;
}

bb::cascades::DataModel *ChatController::messages() const
{
    return m_messages;
}

QString ChatController::currentChatTitle() const
{
    return m_currentChatTitle;
}

void ChatController::addDemoChat(const QString &id, const QString &title,
                                const QString &text, int order)
{
    QVariantMap row;
    // Telegram IDs must also cross the C++/QML boundary as strings: JS numbers
    // cannot represent all 64-bit integers exactly.
    row.insert("chatId", id);
    row.insert("title", title);
    row.insert("preview", text);
    row.insert("order", order);
    m_chatRows.append(row);

    QVariantList history;
    history.append(message(title, text, false, 0));
    m_history.insert(id, history);
}

void ChatController::refreshChats()
{
    m_chats->clear();
    for (int i = 0; i < m_chatRows.size(); ++i) {
        m_chats->insert(m_chatRows.at(i).toMap());
    }
}

bool ChatController::openChat(const QString &chatId)
{
    if (!m_history.contains(chatId)) {
        return false;
    }

    m_currentChatId = chatId;
    for (int i = 0; i < m_chatRows.size(); ++i) {
        const QVariantMap row = m_chatRows.at(i).toMap();
        if (row.value("chatId").toString() == chatId) {
            m_currentChatTitle = row.value("title").toString();
            break;
        }
    }

    m_messages->clear();
    const QVariantList history = m_history.value(chatId);
    for (int i = 0; i < history.size(); ++i) {
        m_messages->insert(history.at(i).toMap());
    }
    emit currentChatChanged();
    return true;
}

void ChatController::closeChat()
{
    m_currentChatId.clear();
    m_currentChatTitle.clear();
    m_messages->clear();
    emit currentChatChanged();
}

bool ChatController::sendDemoMessage(const QString &text)
{
    const QString body = text.trimmed();
    if (m_currentChatId.isEmpty() || body.isEmpty()) {
        return false;
    }

    QVariantList &history = m_history[m_currentChatId];
    const QVariantMap row = message(QString::fromUtf8("Вы · локально"), body,
                                   true, history.size());
    history.append(row);
    m_messages->insert(row);

    for (int i = 0; i < m_chatRows.size(); ++i) {
        QVariantMap chat = m_chatRows.at(i).toMap();
        if (chat.value("chatId").toString() == m_currentChatId) {
            chat.insert("preview", body);
            m_chatRows[i] = chat;
            break;
        }
    }
    refreshChats();
    return true;
}
