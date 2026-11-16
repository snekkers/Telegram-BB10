#include "ChatController.hpp"
#include "MessagePresentation.hpp"

#include <bb/cascades/ItemGrouping>
#include <QDateTime>
#include <QStringList>
#include <algorithm>

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
    row.insert("textMessage",true);
    row.insert("messageId", QString::number(order + 1));
    row.insert("time", date.toString("HH:mm"));
    return row;
}

}

ChatController::ChatController(QObject *parent)
    : QObject(parent)
    , m_chats(new bb::cascades::GroupDataModel(QStringList() << "order", this))
    , m_messages(new bb::cascades::ArrayDataModel(this))
    , m_infoRows(new bb::cascades::ArrayDataModel(this))
    , m_recipients(new bb::cascades::GroupDataModel(QStringList() << "order",this))
    , m_contacts(new bb::cascades::ArrayDataModel(this)),m_contactSection(0)
    , m_archiveView(false),m_infoSection(0),m_incomingSerial(0),m_forwardSerial(0)
{
    m_typingTimer.setSingleShot(true);connect(&m_typingTimer,SIGNAL(timeout()),this,SLOT(clearDemoTyping()));
    m_chats->setGrouping(bb::cascades::ItemGrouping::None);
    m_recipients->setGrouping(bb::cascades::ItemGrouping::None);
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
    row.insert("pinned",false);row.insert("archived",false);
    row.insert("personal", id!="demo-bb10" && id!="demo-project" && id!="demo-design");
    row.insert("avatarKey",id=="demo-anna"?"demo:anna":id=="demo-design"?"demo:channel":id=="demo-project"?"demo:group":id=="demo-bb10" || id=="demo-boris"?"demo:classic":"");
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
    m_chats->clear();m_recipients->clear();
    for (int i = 0; i < m_chatRows.size(); ++i) {
        const QVariantMap row=m_chatRows.at(i).toMap();
        if(m_recipientQuery.isEmpty() || row.value("title").toString().contains(m_recipientQuery,Qt::CaseInsensitive)) {
            QVariantMap recipient=row;recipient.insert("time",QString());recipient.insert("unreadLabel",QString());m_recipients->insert(recipient);
        }
        if(row.value("archived").toBool()==m_archiveView && (m_category.isEmpty() || row.value("personal").toBool()==(m_category=="personal"))) {
            QVariantMap sorted=row;sorted.insert("order",row.value("pinned").toBool()?row.value("order").toInt()-100:row.value("order").toInt());m_chats->insert(sorted);
        }
    }
    if(m_contactSection==1)refreshContacts();
    emit currentChatChanged();
}

void ChatController::refreshMessages()
{
    QVariantList rows = MessagePresentation::decorate(m_history.value(m_currentChatId),
        m_currentChatId == "demo-project" || m_currentChatId == "demo-bb10",
        "2", m_currentChatId == "demo-anna");
    for (int i = 0; i < rows.size(); ++i) {
        QVariantMap row=rows.at(i).toMap();row.insert("selected",m_selected.contains(row.value("messageId").toString()));row.insert("searchTarget",row.value("messageId").toString()==m_targetMessage && !m_targetMessage.isEmpty());
        if(i>=m_messages->size()){
            QVariantList tail;for(int j=i;j<rows.size();++j){QVariantMap next=rows.at(j).toMap();next.insert("selected",m_selected.contains(next.value("messageId").toString()));next.insert("searchTarget",next.value("messageId").toString()==m_targetMessage && !m_targetMessage.isEmpty());tail<<next;}
            m_messages->append(tail);break;
        }
        else if(m_messages->value(i).toMap().value("messageId")!=row.value("messageId"))m_messages->insert(i,QVariant(row));
        else if(m_messages->value(i).toMap()!=row)m_messages->replace(i,QVariant(row));
    }
    while(m_messages->size()>rows.size())m_messages->removeAt(m_messages->size()-1);
}

bool ChatController::openChat(const QString &chatId)
{
    return openChatAt(chatId,QString());
}

bool ChatController::openChatAt(const QString &chatId,const QString &messageId)
{
    if (!m_history.contains(chatId)) {
        return false;
    }

    m_selected.clear();m_selectionState.clear();
    m_context.clear();m_forward.clear();m_typingTimer.stop();m_demoStatus.clear();
    m_currentChatId = chatId;
    m_targetMessage = messageId;
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
    m_context.clear();m_forward.clear();
    m_currentChatId.clear();
    m_targetMessage.clear();
    m_currentChatTitle.clear();
    m_messages->clear();
    emit currentChatChanged();
}

bool ChatController::sendDemoMessage(const QString &text)
{
    const QString body = text.trimmed();
    if (m_currentChatId.isEmpty() || !composerVisible() || body.isEmpty()) {
        return false;
    }

    QVariantList &history = m_history[m_currentChatId];
    if(m_context.value("mode")=="edit") {
        for(int i=0;i<history.size();++i) {
            QVariantMap row=history.at(i).toMap();
            if(row.value("messageId")==m_context.value("messageId")){row.insert("text",body);row.insert("edited",true);history[i]=row;break;}
        }
        if(!history.isEmpty() && history.last().toMap().value("messageId")==m_context.value("messageId"))
            for(int i=0;i<m_chatRows.size();++i){QVariantMap chat=m_chatRows.at(i).toMap();if(chat.value("chatId")==m_currentChatId){chat.insert("preview",body);m_chatRows[i]=chat;break;}}
        m_context.clear();refreshMessages();refreshChats();return true;
    }
    QVariantMap row = message(QString::fromUtf8("Вы · локально"), body,
                                   true, history.size());
    if(m_context.value("mode")=="reply"){row.insert("replyQuote",m_context.value("preview"));row.insert("replyMessageId",m_context.value("messageId"));}
    int nextId=1;foreach(const QVariant &value,history)nextId=qMax(nextId,value.toMap().value("messageId").toString().toInt()+1);row.insert("messageId",QString::number(nextId));
    m_context.clear();history.append(row);
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

void ChatController::setChatCategory(const QString &category) { m_category=category;refreshChats(); }
int ChatController::chatCount() const { return m_chats->childCount(QVariantList()); }
int ChatController::personalCount() const {
    int count=0;foreach(const QVariant &value,m_chatRows)if(value.toMap().value("personal").toBool() && value.toMap().value("archived").toBool()==m_archiveView)++count;return count;
}
int ChatController::targetMessageIndex() const {
    const QVariantList rows=m_history.value(m_currentChatId);
    for(int i=0;i<rows.size();++i)if(rows.at(i).toMap().value("messageId")==m_targetMessage)return i;
    return -1;
}
QVariantList ChatController::search(const QString &query,int mode,int filter) const {
    QVariantList result;const QString text=query.trimmed();
    if(text.isEmpty() || (mode==2 && filter!=0))return result;
    foreach(const QVariant &value,m_chatRows) {
        const QVariantMap chat=value.toMap();
        if(mode==2) {
            foreach(const QVariant &message,m_history.value(chat.value("chatId").toString())) {
                const QVariantMap source=message.toMap();
                if(!source.value("text").toString().contains(text,Qt::CaseInsensitive))continue;
                QVariantMap row=chat;row.insert("messageId",source.value("messageId"));row.insert("preview",source.value("text"));
                row.insert("key",chat.value("chatId").toString()+":"+source.value("messageId").toString());
                row.insert("unreadLabel",QString());result.append(row);
            }
        } else if(chat.value("title").toString().contains(text,Qt::CaseInsensitive)) {
            QVariantMap row=chat;row.insert("key",chat.value("chatId"));row.insert("messageId",QString());row.insert("unreadLabel",QString());
            if(mode==1)row.insert("preview",QString::fromUtf8("Пример результата · демо"));result.append(row);
        }
    }
    return result;
}
