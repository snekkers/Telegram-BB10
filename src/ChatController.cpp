#include "ChatController.hpp"
#include "MessagePresentation.hpp"

#include <bb/cascades/ItemGrouping>
#include <QDateTime>
#include <QStringList>

namespace {

QVariantMap message(const QString &author, const QString &text, bool outgoing, int order,
                    const QDateTime &date = QDateTime::currentDateTime())
{
    QVariantMap row;
    row.insert("author", author);
    row.insert("text", text);
    row.insert("outgoing", outgoing);
    row.insert("order", order);
    row.insert("date", date.toTime_t());
    row.insert("messageId", QString::number(order + 1));
    row.insert("time", date.toString("HH:mm"));
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
                QString::fromUtf8("Заметки, ссылки и всё важное"), 0);
    addDemoChat("demo-anna", QString::fromUtf8("Анна"),
                QString::fromUtf8("Встретимся завтра в 19:00?"), 1);
    addDemoChat("demo-bb10", "BlackBerry Classic",
                QString::fromUtf8("Пишу с настоящей клавиатуры"), 2);
    addDemoChat("demo-project", QString::fromUtf8("Команда"),
                QString::fromUtf8("Макеты готовы, можно смотреть"), 3);
    addDemoChat("demo-design", QString::fromUtf8("Дизайн"),
                QString::fromUtf8("Новый интерфейс для Classic"), 4);
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
    row.insert("time", QDateTime::currentDateTime().toString("HH:mm"));
    row.insert("unreadLabel", order == 1 ? QString("2") : QString());
    m_chatRows.append(row);

    QVariantList history;
    const QDateTime now = QDateTime::currentDateTime();
    const QString author = id == "demo-project" ? QString::fromUtf8("Анна") : title;
    history.append(message(author, QString::fromUtf8("Завтра продолжим?"), false, 0, now.addDays(-1).addSecs(-60)));
    history.append(message(QString::fromUtf8("Вы"), QString::fromUtf8("Да, договорились."), true, 1, now.addDays(-1)));
    history.append(message(author, text, false, 2, now.addSecs(-300)));
    history.append(message(author, QString::fromUtf8("Как тебе новый интерфейс?"), false, 3, now.addSecs(-280)));
    history.append(message(QString::fromUtf8("Вы"), QString::fromUtf8("Стало удобнее писать с клавиатуры."), true, 4, now.addSecs(-240)));
    history.append(message(QString::fromUtf8("Вы"), QString::fromUtf8("И больше места для сообщений."), true, 5, now.addSecs(-225)));
    history.append(message(author, QString::fromUtf8("Это демо: здесь можно проверить ввод и внешний вид без отправки в Telegram."), false, 6, now.addSecs(-180)));
    m_history.insert(id, history);
}

void ChatController::refreshChats()
{
    m_chats->clear();
    for (int i = 0; i < m_chatRows.size(); ++i) {
        m_chats->insert(m_chatRows.at(i).toMap());
    }
}

void ChatController::refreshMessages()
{
    m_messages->clear();
    const QVariantList rows = MessagePresentation::decorate(m_history.value(m_currentChatId),
        m_currentChatId == "demo-project" || m_currentChatId == "demo-bb10",
        "2", m_currentChatId == "demo-anna");
    for (int i = 0; i < rows.size(); ++i) m_messages->insert(rows.at(i).toMap());
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

    refreshMessages();
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
    refreshMessages();

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
