#include "TelegramSearch.hpp"
#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
#include <QDateTime>

namespace {
QVariantMap method(const char *name) { QVariantMap call; call.insert("@type", name); return call; }
QString idOf(const QVariant &id) { return QString::number(id.toLongLong()); }
QString typeLabel(const QVariantMap &chat) {
    const QString type=chat.value("type").toString();
    return type=="chatTypePrivate" ? QString::fromUtf8("Личный чат / бот") : type=="chatTypeSecret" ? QString::fromUtf8("Секретный чат") :
        chat.value("isChannel").toBool() ? QString::fromUtf8("Канал") : QString::fromUtf8("Группа");
}
QString messageText(const QVariantMap &message) {
    const QVariantMap content=message.value("content").toMap();
    const QString type=content.value("@type").toString();
    if(type=="messageText")return content.value("text").toMap().value("text").toString();
    const QString caption=content.value("caption").toMap().value("text").toString();
    const QString label=type=="messagePhoto"?QString::fromUtf8("Фото"):type=="messageDocument"?QString::fromUtf8("Файл"):QString::fromUtf8("Вложение");
    return "["+label+"] "+caption;
}
}
TelegramSearch::TelegramSearch(TelegramSession *session, TelegramStore *store, QObject *parent, int timeoutMs)
    : QObject(parent),m_session(session),m_store(store),m_epoch(0),m_mode(0),m_filter(0),m_timeoutMs(qMax(100,timeoutMs)),m_active(false)
{
    m_debounce.setSingleShot(true);m_clock.start();
    connect(&m_debounce,SIGNAL(timeout()),this,SLOT(run()));
    connect(&m_expiry,SIGNAL(timeout()),this,SLOT(expire()));
    m_expiry.setInterval(qMin(1000,m_timeoutMs));
    connect(session,SIGNAL(eventReceived(QVariantMap)),this,SLOT(consume(QVariantMap)));
    connect(session,SIGNAL(changed()),this,SLOT(sessionChanged()));
    connect(store,SIGNAL(chatsChanged()),this,SLOT(rebuild()));
}
bool TelegramSearch::loading() const {
    if(m_debounce.isActive())return true;
    foreach(const QVariantMap &pending,m_requests) if(pending.value("epoch").toUInt()==m_epoch)return true;
    return false;
}
bool TelegramSearch::canLoadMore() const { return m_active && m_mode==2 && !m_offset.isEmpty() && !loading(); }
bool TelegramSearch::ready() const { return m_store->ready(); }
void TelegramSearch::cancel() {
    ++m_epoch;m_active=false;m_debounce.stop();m_requests.clear();m_expiry.stop();m_ids.clear();m_messages.clear();m_rows.clear();m_offset.clear();m_error.clear();
    m_hydrated.clear();m_messageKeys.clear();emit changed();
}
void TelegramSearch::search(const QString &query,int mode,int filter) {
    cancel();m_query=query.trimmed().left(256);m_mode=qBound(0,mode,2);m_filter=qBound(0,filter,3);
    if(m_query.isEmpty() && (m_mode!=2 || m_filter==0))return;
    m_active=true;
    if(!m_store->ready()) { m_error=QString::fromUtf8("Для поиска войдите в Telegram.");emit changed();return; }
    if(m_mode==0) {
        foreach(const QVariant &value,m_store->knownChats()) {
            const QVariantMap chat=value.toMap();
            if(chat.value("title").toString().contains(m_query,Qt::CaseInsensitive))addChat(chat.value("chatId").toString());
        }
    }
    m_debounce.start(350);rebuild();emit changed();
}
void TelegramSearch::retry() { search(m_query,m_mode,m_filter); }
void TelegramSearch::sessionChanged() { if(!m_store->ready()) { cancel();m_requests.clear(); } }
bool TelegramSearch::send(QVariantMap call,const QString &kind,const QString &chatId) {
    if(!m_store->ready() || m_requests.size()>=64) { m_error=QString::fromUtf8("Поиск занят. Повторите через несколько секунд.");return false; }
    const QString id=m_session->sendReady(call);
    if(id.isEmpty()) { m_error=QString::fromUtf8("Движок поиска недоступен.");return false; }
    QVariantMap pending;pending.insert("epoch",m_epoch);pending.insert("kind",kind);pending.insert("chatId",chatId);
    pending.insert("deadline",m_clock.elapsed()+m_timeoutMs);m_requests.insert(id,pending);if(!m_expiry.isActive())m_expiry.start();return true;
}
void TelegramSearch::run() {
    if(!m_active || !m_store->ready())return;
    if(m_mode==2)messages();
    else {
        QVariantMap call=method(m_mode==1?"searchPublicChats":"searchChats");
        call.insert("query",m_query);call.insert("type_filter",QVariant());
        if(m_mode==0)call.insert("limit",50);
        send(call,"chats");
        if(m_mode==0) { call.insert("@type","searchChatsOnServer");send(call,"chats"); }
    }
    emit changed();
}
void TelegramSearch::messages() {
    QVariantMap call=method("searchMessages");call.insert("chat_list",QVariant());call.insert("query",m_query);
    call.insert("offset",m_offset);call.insert("limit",30);
    const char *filters[]={"searchMessagesFilterEmpty","searchMessagesFilterPhoto","searchMessagesFilterDocument","searchMessagesFilterUrl"};
    call.insert("filter",method(filters[m_filter]));call.insert("chat_type_filter",QVariant());call.insert("min_date",0);call.insert("max_date",0);
    send(call,"messages");
}
void TelegramSearch::loadMore() { if(canLoadMore()) { m_error.clear();messages();emit changed(); } }
void TelegramSearch::hydrate(const QString &id) {
    if(id=="0" || !m_store->chat(id).isEmpty() || m_hydrated.contains(id))return;
    m_hydrated.insert(id);QVariantMap call=method("getChat");call.insert("chat_id",QVariant::fromValue(id.toLongLong()));send(call,"hydrate",id);
}
void TelegramSearch::addChat(const QString &id) { if(id!="0" && !m_ids.contains(id)) { m_ids.append(id);hydrate(id); } }
void TelegramSearch::consume(const QVariantMap &object) {
    const QString extra=object.value("@extra").toString();
    if(!m_requests.contains(extra))return;
    const QVariantMap pending=m_requests.take(extra);
    if(m_requests.isEmpty())m_expiry.stop();
    if(pending.value("epoch").toUInt()!=m_epoch || !m_active)return;
    const QString type=object.value("@type").toString(),kind=pending.value("kind").toString();
    if(type=="error") {
        m_error=QString::fromUtf8("Не удалось выполнить поиск (код %1). Можно повторить.").arg(object.value("code").toInt());
    } else if(kind=="hydrate" && type=="chat") {
        // TelegramStore consumed this same session event before the search slot.
    } else if(kind=="chats" && type=="chats") {
        foreach(const QVariant &id,object.value("chat_ids").toList())addChat(idOf(id));
    } else if(kind=="messages" && type=="foundMessages") {
        const QString next=object.value("next_offset").toString();
        m_offset=next==m_offset?QString():next;
        foreach(const QVariant &value,object.value("messages").toList()) {
            const QVariantMap message=value.toMap();
            const QString key=idOf(message.value("chat_id"))+":"+idOf(message.value("id"));
            if(message.value("id").toLongLong()<=0 || m_messageKeys.contains(key))continue;
            m_messageKeys.insert(key);m_messages.append(message);hydrate(idOf(message.value("chat_id")));
        }
    }
    rebuild();emit changed();
}
void TelegramSearch::rebuild() {
    QVariantList rows;
    if(m_active && m_store->ready()) {
        if(m_mode==2) {
            foreach(const QVariant &value,m_messages) {
                const QVariantMap message=value.toMap();const QString id=idOf(message.value("chat_id"));
                QVariantMap row=m_store->chat(id);row.insert("chatId",id);row.insert("messageId",idOf(message.value("id")));
                row.insert("key",id+":"+row.value("messageId").toString());row.insert("preview",messageText(message).left(240));
                row.insert("time",QDateTime::fromTime_t(message.value("date").toUInt()).toString("dd.MM.yy"));
                if(row.value("title").toString().isEmpty())row.insert("title",QString::fromUtf8("Чат"));
                row.insert("unreadLabel",QString());row.remove("mainOrder");row.remove("archiveOrder");rows.append(row);
            }
        } else foreach(const QString &id,m_ids) {
            QVariantMap row=m_store->chat(id);if(row.isEmpty())continue;
            row.insert("key",id);row.insert("messageId",QString());row.insert("preview",typeLabel(row));row.insert("time",QString());
            row.insert("unreadLabel",QString());row.remove("mainOrder");row.remove("archiveOrder");rows.append(row);
        }
    }
    if(rows!=m_rows) { m_rows=rows;emit changed(); }
}
void TelegramSearch::expire() {
    bool expired=false;
    QMap<QString,QVariantMap>::iterator it=m_requests.begin();
    while(it!=m_requests.end()) {
        if(it.value().value("deadline").toLongLong()>m_clock.elapsed()) { ++it;continue; }
        if(it.value().value("epoch").toUInt()==m_epoch) { expired=true;m_error=QString::fromUtf8("Поиск не ответил вовремя. Можно повторить."); }
        it=m_requests.erase(it);
    }
    if(m_requests.isEmpty())m_expiry.stop();
    if(expired)emit changed();
}
