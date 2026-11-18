#include "TelegramStore.hpp"
#include "MessagePresentation.hpp"
#include "Presence.hpp"
#include "TelegramSession.hpp"
#include "PrivateFile.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QSize>
#include <QDir>
#include <QUrl>
#include <QRegExp>
#include <algorithm>

namespace {
QString ru(const char *s) { return QString::fromUtf8(s); }
QString idString(const QVariant &value)
{
    // int53 is exact in a JSON double, but QVariant::toString can round it.
    return QString::number(value.toLongLong());
}
QVariant number(const QString &id) { return QVariant::fromValue(id.toLongLong()); }
QVariantMap method(const char *type)
{
    QVariantMap result;
    result.insert("@type", QString::fromLatin1(type));
    return result;
}
bool chatBefore(const QVariantMap &a, const QVariantMap &b)
{
    qlonglong x = a.value("mainOrder").toLongLong(), y = b.value("mainOrder").toLongLong();
    return x == y ? a.value("chatId").toLongLong() > b.value("chatId").toLongLong() : x > y;
}
bool requestBefore(const QVariantMap &a, const QVariantMap &b)
{
    return a.value("sequence").toUInt() < b.value("sequence").toUInt();
}
QString privateMediaPath(const QString &data, const QString &path)
{
    QFileInfo candidate(path);
    if (!candidate.isFile() || candidate.isSymLink()) return QString();
    const QString canonical = candidate.canonicalFilePath();
    const QStringList folders = QStringList() << "files" << "uploads";
    foreach (const QString &folder, folders) {
        QFileInfo directory(QDir(data).filePath(folder));
        const QString boundary = directory.canonicalFilePath();
        if (!directory.isSymLink() && !boundary.isEmpty() && canonical.startsWith(boundary + "/")) return canonical;
    }
    return QString();
}
unsigned bigEndian(const QByteArray &data, int offset, int length)
{
    unsigned result = 0;
    for (int i = 0; i < length; ++i) result = (result << 8) | (unsigned char)data.at(offset + i);
    return result;
}
QSize photoDimensions(const QByteArray &header)
{
    if (header.size() >= 24 && header.startsWith("\x89PNG\r\n\x1a\n") && header.mid(12, 4) == "IHDR") {
        const unsigned width = bigEndian(header, 16, 4), height = bigEndian(header, 20, 4);
        if (width <= 10000 && height <= 10000) return QSize(width, height);
    }
    if (!header.startsWith("\xff\xd8\xff")) return QSize();
    int offset = 2;
    while (offset + 4 <= header.size()) {
        if ((unsigned char)header.at(offset++) != 255) return QSize();
        while (offset < header.size() && (unsigned char)header.at(offset) == 255) ++offset;
        if (offset + 3 > header.size()) break;
        const unsigned marker = (unsigned char)header.at(offset++);
        if (marker == 217 || marker == 218) break;
        const unsigned length = bigEndian(header, offset, 2);
        if (length < 2 || offset + int(length) > header.size()) break;
        if ((marker >= 192 && marker <= 195) || (marker >= 197 && marker <= 199) || (marker >= 201 && marker <= 203) || (marker >= 205 && marker <= 207)) {
            if (length >= 8) return QSize(bigEndian(header, offset + 5, 2), bigEndian(header, offset + 3, 2));
            break;
        }
        offset += length;
    }
    return QSize();
}
void applyFormatting(QVariantMap *result,const QVariantMap &content){
    QVariantMap &row=*result;row.remove("link");
    QString link;QVariantMap formatted=content.value(content.value("@type")=="messageText"?"text":"caption").toMap();
    const QString body=formatted.value("text").toString();
    foreach(const QVariant &value,formatted.value("entities").toList()){
        const QVariantMap entity=value.toMap(),kind=entity.value("type").toMap();const QString type=kind.value("@type").toString();
        const int offset=entity.value("offset").toInt(),length=entity.value("length").toInt();if(offset<0 || length<=0 || offset>body.size()-length)continue;
        if(type=="textEntityTypeTextUrl")link=kind.value("url").toString();
        else if(type=="textEntityTypeUrl")link=body.mid(offset,length);
        else if(type=="textEntityTypeMention")link="https://t.me/"+body.mid(offset+1,length-1);
        if(!link.isEmpty())break;
    }
    int detectedAt=-1,detectedLength=0;
    if(link.isEmpty()){QRegExp rx("(https?://[^\\s<>]+|t\\.me/[A-Za-z0-9_+/]+|@[A-Za-z][A-Za-z0-9_]{4,31})");if((detectedAt=rx.indexIn(body))>=0){link=rx.cap(1);detectedLength=link.size();while(!link.isEmpty() && QString(".,!;:)").contains(link.at(link.size()-1))){link.chop(1);--detectedLength;}}}
    if(link.startsWith("@"))link="https://t.me/"+link.mid(1);else if(link.startsWith("t.me/"))link="https://"+link;
    const QUrl target(link);if(target.isValid() && (target.scheme()=="https" || target.scheme()=="http") && !target.host().isEmpty() && target.userInfo().isEmpty()){row.insert("link",target.toString());
        if(detectedAt>=0 && detectedLength>0){QVariantMap entity,kind;kind.insert("@type","textEntityTypeTextUrl");kind.insert("url",target.toString());entity.insert("offset",detectedAt);entity.insert("length",detectedLength);entity.insert("type",kind);QVariantList entities=formatted.value("entities").toList();entities<<entity;formatted.insert("entities",entities);}
    }
    row.insert("formattedText",formatted);
    
}
QString draftValue(const QVariant &draft)
{
    return draft.toMap().value("content").toMap().value("text").toMap().value("text").toString();
}
}

TelegramStore::TelegramStore(TelegramSession *session, QObject *parent, int requestTimeoutMs)
    : QObject(parent), m_session(session), m_local(session->dataDirectory()), m_nextCleanup(0), m_requestTimeoutMs(qMax(100, requestTimeoutMs)), m_contactsLoading(false),m_openingContact(false),m_contactSection(0),m_contactSerial(0),m_nextPresenceRefresh(0), m_infoSection(0), m_memberOffset(0), m_actionSerial(0), m_infoSerial(0), m_historyCursor(0), m_generation(0),
      m_foreground(true), m_ready(false), m_loadingChats(false), m_loadingHistory(false),
      m_chatsEnd(false), m_historyEnd(false), m_online(true), m_archiveView(false), m_archiveEnd(false), m_infoLoading(false), m_infoEnd(true)
{
    m_pinnedSerial=0;m_reactionSerial=0;
    m_scheduledBusy=m_scheduledOpen=m_autoDownload=false;m_scheduledSerial=m_downloadSerial=0;
    m_autoWifiAllowed=m_autoMobileAllowed=false;
    m_completedRequests=m_timedOutRequests=m_requestLatency=0;
    m_batching = m_chatsDirty = m_messagesDirty = m_chatUpdatesPending = false;
    m_lastReadSubmitted=0;
    m_followingLatest=true;m_scrollPending=false;m_pendingIncoming=0;m_historyLocalEnd=false;
    m_historyRetryAt = 0;
    connect(session,SIGNAL(batchStarted()),this,SLOT(beginBatch()));
    connect(session,SIGNAL(batchFinished()),this,SLOT(endBatch()));
    const QString viewPath=QDir(m_session->dataDirectory()).filePath("view-state.json");
    m_viewStateValid=!QFile::exists(viewPath) || PrivateFile::read(viewPath,&m_viewState,131072);
    connect(session, SIGNAL(changed()), this, SLOT(sessionChanged()));
    connect(session, SIGNAL(eventReceived(QVariantMap)), this, SLOT(consume(QVariantMap)));
    connect(&m_expiry, SIGNAL(timeout()), this, SLOT(expireRequests()));
    connect(&m_draftTimer, SIGNAL(timeout()), this, SLOT(flushDrafts()));
    m_draftTimer.setSingleShot(true);
    m_clock.start();
    m_expiry.start(qMin(1000, m_requestTimeoutMs));
    connect(&m_peopleTimer,SIGNAL(timeout()),this,SLOT(peopleTick()));m_peopleTimer.start(1000);
    sessionChanged();
}

TelegramStore::~TelegramStore() { flushDrafts(); suspendLocalSends(); }

void TelegramStore::suspendLocalSends()
{
    foreach (const QString &chat, m_dirtyDrafts.keys()) m_local.rememberDraft(m_account, chat, m_dirtyDrafts.value(chat));
    foreach (const QVariant &value, m_local.entries(m_account)) {
        const QVariantMap item = value.toMap();
        if (item.value("state") == "submitting" || item.value("state") == "pending")
            m_local.transition(item.value("recordId").toString(), "uncertain");
    }
}

void TelegramStore::reset()
{
    closeScheduled();m_downloads.clear();m_autoSuppressed.clear();emit downloadsChanged();
    suspendLocalSends(); m_account.clear(); m_localMessages.clear();
    retireHistoryRequests();
    ++m_generation;
    m_selected.clear();m_selectionState.clear();m_storageSummary.clear();
    m_chats.clear(); m_users.clear(); m_requests.clear(); m_outbox.clear();
    m_messages.clear(); m_overlays.clear(); m_historyTouched.clear(); m_removed.clear();
    m_failedDrafts.clear(); m_recoveredTexts.clear(); m_current.clear();m_threadId.clear();m_threadRoot.clear();m_threadSourceChat.clear();m_threadSourceMessage.clear();m_pendingComment.clear(); m_error.clear();
    m_draftTimer.stop(); m_drafts.clear(); m_dirtyDrafts.clear(); m_draftInFlight.clear();
    m_files.clear(); m_downloading.clear();
    m_contactRows.clear();m_contactIds.clear();m_contactsError.clear();m_contactsLoading=m_openingContact=false;++m_contactSerial;m_pendingContactChat.clear();
    m_avatarFiles.clear();m_avatarQueue.clear();m_avatarQueued.clear();m_avatarActive.clear();m_avatarFailed.clear();m_chatActions.clear();
    emit contactsChanged();emit avatarChanged(QString());
    m_chatRows.clear(); m_messageRows.clear();
    m_chatUpdatesPending = false;
    m_failedAttachments.clear(); m_sentAttachments.clear(); m_earlyAttachmentOutcomes.clear();m_earlyForwards.clear();
    m_historyCursor = 0; m_targetMessage.clear();
    m_archiveEnd=false;m_composerContext.clear();m_forward.clear();m_info.clear();m_pinnedMessage.clear();++m_pinnedSerial;m_reactionState.clear();++m_reactionSerial;m_infoRows.clear();m_members.clear();m_shared.clear();
    m_infoError.clear();m_infoLoading=false;m_infoEnd=true;++m_infoSerial;++m_actionSerial;emit infoChanged();
    m_loadingChats = m_loadingHistory = m_chatsEnd = m_historyEnd = false;
    emit chatsChanged();
    emit recipientChatsChanged();
    emit messagesChanged();
}

void TelegramStore::sessionChanged()
{
    const QString phase = m_session->phase();
    const bool wasReady = m_ready;
    m_ready = phase == "ready";
    if (phase != m_seenPhase) {
        m_seenPhase = phase;
        if ((wasReady && !m_ready) || phase == "starting" || phase == "stopped" || phase == "failed") {
            reset();
            m_connection = m_ready ? ru("Подключение…") : ru("Движок не подключён");
        }
    }
    if (m_ready && !wasReady) {
        if (m_connection.isEmpty() || m_connection == ru("Движок не подключён"))
            m_connection = ru("Подключение…");
        rebuildChats();
        QVariantMap option = method("setOption");
        option.insert("name", "use_quick_ack");
        QVariantMap value = method("optionValueBoolean");
        value.insert("value", true);
        option.insert("value", value);
        submit(option, "option");
        m_sentNetwork.clear();
        sendNetworkType();
        submit(method("getMe"), "me");
        if (canLoadChats()) loadChats(m_archiveView, 8);
        QVariantMap online = method("setOption"), state = method("optionValueBoolean");
        state.insert("value", m_foreground); online.insert("name", "online"); online.insert("value", state);
        submit(online, "online");
    }
    emit changed();
}

QString TelegramStore::submit(QVariantMap object, const QString &kind, const QString &chatId)
{
    if (!m_ready)
        return QString();
    int active=0;
    foreach(const QVariantMap &pending,m_requests)if(!pending.value("expired").toBool())++active;
    if (active >= 64) {
        m_error = ru("Слишком много запросов без ответа. Дождитесь их завершения или перезапустите движок.");
        emit changed();
        return QString();
    }
    QString id = m_session->sendReady(object);
    if (id.isEmpty())
        return QString();
    QVariantMap pending;
    pending.insert("kind", kind);
    pending.insert("chatId", chatId);
    pending.insert("generation", m_generation);pending.insert("threadId",m_threadId);
    const int timeout = (kind == "chats" || kind == "network")
        ? qMin(15000, m_requestTimeoutMs) : m_requestTimeoutMs;
    pending.insert("deadline", m_clock.elapsed() + timeout);
    pending.insert("started",m_clock.elapsed());
    m_requests.insert(id, pending);
    return id;
}

void TelegramStore::loadMoreChats()
{
    if (!canLoadChats())
        return;
    loadChats(m_archiveView);
}

void TelegramStore::loadMoreRecipientChats()
{
    if(canLoadRecipientChats())loadChats(m_chatsEnd);
}

void TelegramStore::loadChats(bool archive, int limit)
{
    QVariantMap call = method("loadChats");
    call.insert("chat_list", method(archive ? "chatListArchive" : "chatListMain"));
    call.insert("limit", limit);
    const QString request=submit(call,"chats");
    if(request.isEmpty())return;
    m_requests[request].insert("archive",archive);
    m_requests[request].insert("limit", limit);
    m_error.clear();
    m_loadingChats = true;
    emit changed();
}

void TelegramStore::setPositions(QVariantMap *chat, const QVariantList &positions)
{
    chat->insert("mainOrder",QVariant::fromValue(qlonglong(0)));chat->insert("archiveOrder",QVariant::fromValue(qlonglong(0)));
    chat->insert("mainPinned",false);chat->insert("archivePinned",false);
    foreach(const QVariant &value,positions) {
        const QVariantMap position=value.toMap();const QString type=position.value("list").toMap().value("@type").toString();
        const QString prefix=type=="chatListMain"?"main":type=="chatListArchive"?"archive":"";
        if(prefix.isEmpty())continue;
        chat->insert(prefix+"Order",QVariant::fromValue(position.value("order").toLongLong()));
        chat->insert(prefix+"Pinned",position.value("is_pinned"));
    }
}

void TelegramStore::rebuildChats()
{
    if(m_batching){m_chatsDirty=true;return;}
    QList<QVariantMap> ordered;
    QMap<QString, QVariantMap>::const_iterator it;
    for (it = m_chats.constBegin(); it != m_chats.constEnd(); ++it) {
        const QString type = it.value().value("type").toString();
        const bool personal = type == "chatTypePrivate" || type == "chatTypeSecret";
        if (it.value().value(m_archiveView?"archiveOrder":"mainOrder").toLongLong() > 0 &&
            (m_chatCategory.isEmpty() || (m_chatCategory == "personal" ? personal : !personal)) &&
            (m_search.isEmpty() || it.value().value("title").toString().contains(m_search, Qt::CaseInsensitive)))
        {
            QVariantMap row=it.value();
            row.insert("pinned",row.value(m_archiveView?"archivePinned":"mainPinned"));
            row.insert("archived",row.value("archiveOrder").toLongLong()>0);
            row.insert("mainOrder",row.value(m_archiveView?"archiveOrder":"mainOrder"));ordered.append(row);
        }
    }
    std::sort(ordered.begin(), ordered.end(), chatBefore);
    QVariantList rows;
    if (m_ready) {
        for (int i = 0; i < ordered.size(); ++i) {
            QVariantMap row = ordered.at(i);
            row.remove("mainOrder");row.remove("archiveOrder"); // Keep int64 positions out of JavaScript.
            row.insert("order", i);
            row.insert("avatarKey","chat:"+row.value("chatId").toString());
            row.insert("typingText",chatActivity(row.value("chatId").toString()));
            row.insert("statusActive",!row.value("typingText").toString().isEmpty());
            const int unread = row.value("unreadCount").toInt();
            row.insert("unreadLabel", unread > 0 ? QString::number(unread) : QString());
            const uint lastDate = row.value("lastDate").toUInt();
            const QDateTime last = QDateTime::fromTime_t(lastDate);
            row.insert("time", lastDate == 0 ? QString() : last.toString(last.date() == QDate::currentDate() ? "HH:mm" : "dd.MM"));
            rows.append(row);
        }
    }
    if (rows != m_chatRows) {
        m_chatRows = rows;
        emit chatsChanged();
    }
}

QString TelegramStore::messageText(const QVariantMap &content) const
{
    const QString type = content.value("@type").toString();
    if (type == "messageText")
        return content.value("text").toMap().value("text").toString().left(65536);
    QString label = ru("Сообщение этого типа пока не поддерживается");
    if (type == "messagePhoto") label = ru("Фото");
    else if (type == "messageVideo") label = ru("Видео");
    else if (type == "messageDocument") label = ru("Файл");
    else if (type == "messageSticker") label = ru("Стикер");
    else if (type == "messageAudio") label = ru("Аудио");
    else if (type == "messageVoiceNote") label = ru("Голосовое сообщение");
    else if (type == "messageAnimation") label = ru("Анимация");
    const QString caption = content.value("caption").toMap().value("text").toString().left(65536);
    QString name;
    if (type == "messageDocument") name = content.value("document").toMap().value("file_name").toString().left(256);
    return "[" + label + "]" + (name.isEmpty() ? QString() : " " + name) + (caption.isEmpty() ? QString() : "\n" + caption);
}

QString TelegramStore::currentChatTitle() const
{
    return m_chats.value(m_current).value("title").toString();
}

bool TelegramStore::sendBusy() const
{
    QMap<QString, QVariantMap>::const_iterator it;
    for (it = m_requests.constBegin(); it != m_requests.constEnd(); ++it)
        if ((it.value().value("kind") == "send" || it.value().value("kind") == "edit") && it.value().value("chatId") == m_current && it.value().value("threadId").toString()==m_threadId &&
            !it.value().value("expired").toBool())
            return true;
    return false;
}

QString TelegramStore::composerHint() const
{
    const QVariantMap chat = m_chats.value(m_current);
    if (!m_ready) return ru("Telegram остановлен");
    if (chat.value("type").toString() == "chatTypeSecret") return ru("Секретные чаты пока не поддерживаются");
    if (chat.value("topics").toBool()) return ru("Отправка в темы пока не поддерживается");
    if(m_composerContext.value("mode")=="edit")return ru("Редактирование сообщения");
    if (!chat.value("canSend").toBool()) return ru("В этом чате отправка текста недоступна");
    return inComments()?ru("Комментарий"):ru("Сообщение Telegram");
}

bool TelegramStore::canSend() const
{
    const QVariantMap chat = m_chats.value(m_current);
    if(m_composerContext.value("mode")=="edit")return m_ready && !m_current.isEmpty() && !sendBusy();
    return m_ready && !m_account.isEmpty() && m_local.healthy() && !m_current.isEmpty() && !sendBusy() && chat.value("canSend").toBool() &&
        !chat.value("topics").toBool() && chat.value("type").toString() != "chatTypeSecret";
}

bool TelegramStore::canSendMedia(const QString &kind) const
{
    const QVariantMap chat = m_chats.value(m_current);
    if(m_composerContext.value("mode")=="edit")return false;
    const QString permission = kind == "photo" ? "can_send_photos" : kind == "voice" ? "can_send_voice_notes" : "can_send_documents";
    return (kind == "document" || kind == "photo" || kind == "voice") && m_ready && !m_account.isEmpty() && m_local.healthy() && !m_current.isEmpty() &&
        !sendBusy() && !chat.value("topics").toBool() && chat.value("type").toString() != "chatTypeSecret" &&
        chat.value("permissions").toMap().value(permission).toBool();
}

QString TelegramStore::uploadDirectory() const
{
    return QDir(m_session->dataDirectory()).filePath("uploads");
}

QVariantMap TelegramStore::attachmentDraft() const
{
    return m_local.latest(m_account, m_current, "attachment", QStringList() << "draft", m_threadId);
}

QVariantMap TelegramStore::failedAttachment() const
{
    const QVariantMap result = m_local.latest(m_account, m_current, "attachment", QStringList() << "failed" << "uncertain", m_threadId);
    return result.isEmpty() ? m_failedAttachments.value(draftKey()) : result;
}

QString TelegramStore::failedDraft() const
{
    const QVariantMap item = m_local.latest(m_account, m_current, "text", QStringList() << "failed" << "uncertain", m_threadId);
    return item.isEmpty() || m_recoveredTexts.contains(item.value("recordId").toString()) ? m_failedDrafts.value(draftKey()) : item.value("text").toString();
}

QVariantMap TelegramStore::uploadRecord(const QString &id) const
{
    QVariantMap item = m_local.entry(id);
    if (m_account.isEmpty() || item.value("accountId") != m_account || item.value("chatId") != m_current || item.value("type") != "attachment" || item.value("threadId").toString()!=m_threadId) return QVariantMap();
    item.insert("recordId", id); item.insert("path", m_local.path(item)); return item;
}

bool TelegramStore::stageAttachment(const QString &path, const QString &kind, const QString &caption, int duration)
{
    QFileInfo file(path);
    if (!m_ready || m_account.isEmpty() || m_current.isEmpty() || caption.size() > 1024 || duration < 0 || duration > 300 ||
        (kind != "photo" && kind != "document" && kind != "voice") || !file.isFile() || file.isSymLink() ||
        file.size() <= 0 || file.size() > 50 * 1024 * 1024) return false;
    QVariantMap item; item.insert("type", "attachment"); item.insert("path", path); item.insert("kind", kind);
    item.insert("caption", caption); item.insert("duration", duration);item.insert("threadId",m_threadId);
    const QString id = m_local.prepare(m_account, m_current, item);
    if (id.isEmpty()) { m_error = m_local.error(); emit changed(); return false; }
    setDraft(caption); emit changed(); return true;
}

bool TelegramStore::clearAttachmentDraft()
{
    const bool result = m_local.removeDraft(m_account, m_current, m_threadId);
    if (!result) m_error = m_local.error();
    emit changed(); return result;
}

bool TelegramStore::recoverAttachment()
{
    const QVariantMap item = failedAttachment();
    return !item.isEmpty() && stageAttachment(item.value("path").toString(), item.value("kind").toString(),
        item.value("caption").toString(), item.value("duration").toInt());
}

QString TelegramStore::recoveryWarning() const
{
    foreach(const QVariant &value,m_local.entries(m_account)) {
        const QVariantMap item=value.toMap();
        if(item.value("state")!="uncertain")continue;
        if(item.value("type")=="edit" && item.value("chatId")==m_current)return ru("Изменение сообщения не подтверждено. Проверьте текст перед повтором.");
        if(item.value("type")=="forward" && (item.value("chatId")==m_current || item.value("sourceChatId")==m_current))return ru("Пересылка не подтверждена. Проверьте чат получателя перед повтором.");
    }

    if (m_account.isEmpty()) return QString();
    if (!m_local.healthy() || !m_local.error().isEmpty()) return m_local.error();
    const QVariantMap item = attachmentDraft();
    if (!item.isEmpty() && item.value("path").toString().isEmpty()) return ru("Копия сохранённого вложения отсутствует. Выберите исходный файл заново.");
    return QString();
}

void TelegramStore::refreshStorage(){
    if(!m_ready)return;QVariantMap call=method("getStorageStatistics");call.insert("chat_limit",0);m_storageSummary=ru("Подсчитываем файлы…");submit(call,"storage");emit changed();
}
int TelegramStore::cleanupUploads() { const int count = m_local.cleanup(); emit changed(); return count; }

void TelegramStore::restoreChatDraft(const QString &id, const QVariant &remote, bool preserveRemote)
{
    if (m_dirtyDrafts.contains(id)) return;
    const QVariant local = m_local.draft(m_account, id);
    const QVariantMap attachment = m_local.latest(m_account, id.section("/thread/",0,0), "attachment", QStringList() << "draft", id.section("/thread/",1,1));
    const QVariantMap text = m_local.latest(m_account, id.section("/thread/",0,0), "text", QStringList() << "draft", id.section("/thread/",1,1));
    if (local.isValid()) { m_drafts.insert(id, local.toString()); m_dirtyDrafts.insert(id, local.toString()); m_draftTimer.start(600); }
    else if (!attachment.isEmpty()) m_drafts.insert(id, attachment.value("caption").toString());
    else if (!text.isEmpty()) m_drafts.insert(id, text.value("text").toString());
    else if (!preserveRemote) m_drafts.insert(id, draftValue(remote));
}

void TelegramStore::recordSendResult(const QVariantMap &message, const QString &recordId, const QString &outcome)
{
    if (recordId.isEmpty()) return;
    const QString sending = message.value("sending_state").toMap().value("@type").toString();
    const QString state = outcome == "failed" || sending == "messageSendingStateFailed" ? "failed" :
        outcome == "succeeded" || sending.isEmpty() ? "sent" : "pending";
    if (!m_local.transition(recordId, state, idString(message.value("id")))) m_error = m_local.error();
    if(m_forward.value("recordIds").toStringList().contains(recordId) || m_forward.value("recordId")==recordId)updateForwardStatus();
    if (state == "pending") m_localMessages.insert(idString(message.value("chat_id")) + ":" + idString(message.value("id")), recordId);
}

bool TelegramStore::sendAttachment(const QString &path, const QString &kind, const QString &caption, int duration, int width, int height, uint scheduledAt)
{
    QFileInfo candidate(path), directory(uploadDirectory());
    const QString boundary = directory.canonicalFilePath();
    QFile source(path);
    bool valid = canSendMedia(kind) && caption.size() <= 1024 && !boundary.isEmpty() && !directory.isSymLink() &&
        candidate.isFile() && !candidate.isSymLink() && candidate.canonicalFilePath().startsWith(boundary + "/") &&
        candidate.size() > 0 && candidate.size() <= 50 * 1024 * 1024 && source.open(QIODevice::ReadOnly);
    QByteArray header = valid ? source.read(256 * 1024) : QByteArray();
    source.close();
    if (kind == "photo") {
        const QSize dimensions = photoDimensions(header); width = dimensions.width(); height = dimensions.height();
        valid = valid && candidate.size() <= 10 * 1024 * 1024 && width > 0 && height > 0 && width + height <= 10000 &&
            width <= height * 20 && height <= width * 20;
    }
    if (kind == "voice") {
        const QString suffix = candidate.suffix().toLower();
        const int opus = header.indexOf("OpusHead");
        const bool ogg = suffix == "ogg" && header.startsWith("OggS") && opus >= 0 && header.size() > opus + 9 && header.at(opus + 9) == 1;
        const bool m4a = suffix == "m4a" && header.mid(4, 4) == "ftyp";
        const bool mp3 = suffix == "mp3" && (header.startsWith("ID3") || (header.size() > 1 && (unsigned char)header[0] == 255 && ((unsigned char)header[1] & 224) == 224));
        valid = valid && duration > 0 && duration <= 300 && (ogg || m4a || mp3);
    }
    if (!valid) { m_error = ru("Вложение недоступно: проверьте формат, размер и разрешения чата."); emit changed(); return false; }
    QVariantMap attachment;
    attachment.insert("path", candidate.canonicalFilePath()); attachment.insert("kind", kind); attachment.insert("caption", caption);
    attachment.insert("duration", duration); attachment.insert("width", width); attachment.insert("height", height);
    QVariantMap content = method(kind == "photo" ? "inputMessagePhoto" : kind == "voice" ? "inputMessageVoiceNote" : "inputMessageDocument");
    QVariantMap input = method(kind == "photo" ? "inputPhoto" : kind == "voice" ? "inputVoiceNote" : "inputDocument");
    QVariantMap file = method("inputFileLocal"); file.insert("path", candidate.canonicalFilePath());
    const QString field = kind == "photo" ? "photo" : kind == "voice" ? "voice_note" : "document";
    input.insert(field, file);
    if (kind == "photo") { input.insert("width", width); input.insert("height", height); input.insert("added_sticker_file_ids", QVariantList()); }
    if (kind == "document") input.insert("disable_content_type_detection", true);
    if (kind == "voice") { input.insert("duration", duration); input.insert("waveform", ""); }
    content.insert(field, input);
    QVariantMap formatted = method("formattedText"); formatted.insert("text", caption); formatted.insert("entities", QVariantList());
    content.insert("caption", formatted);
    return sendContent(content, (kind == "photo" ? ru("Фото") : kind == "voice" ? ru("Голосовое сообщение") : candidate.fileName()) +
        (caption.isEmpty() ? QString() : "\n" + caption), attachment, scheduledAt);
}

bool TelegramStore::openChat(const QString &id)
{
    const QString position=m_viewState.value(m_account+":"+(id==m_current?draftKey():id)).toMap().value("messageId").toString();
    return openChatAt(id,position);
}
void TelegramStore::saveViewState(const QString &messageId,int cursor){
    if(m_account.isEmpty() || m_current.isEmpty())return;
    const QString path=QDir(m_session->dataDirectory()).filePath("view-state.json");if(!m_viewStateValid)return;QVariantMap state=m_viewState;
    QVariantMap item;item.insert("messageId",messageId.toLongLong()>0?QString::number(messageId.toLongLong()):QString());item.insert("cursor",qBound(0,cursor,4096));
    const QString key=m_account+":"+draftKey();state.insert(key,item);while(state.size()>256)state.erase(state.begin());
    if(state==m_viewState)return;
    if(!PrivateFile::write(path,state,131072)){m_error=ru("Не удалось сохранить положение в чате.");emit changed();}else m_viewState=state;
}
int TelegramStore::draftCursor() const{
    const QVariantMap item=m_viewState.value(m_account+":"+draftKey()).toMap();return item.contains("cursor")?qBound(0,item.value("cursor").toInt(),m_drafts.value(draftKey()).size()):m_drafts.value(draftKey()).size();
}
void TelegramStore::clearViewPosition(){saveViewState(QString(),draftCursor());}

bool TelegramStore::openChatAt(const QString &id, const QString &messageId)
{
    if (!m_ready || !m_chats.contains(id))
        return false;
    const QVariantMap reply=m_current==id && m_composerContext.value("mode")=="reply"?m_composerContext:QVariantMap();
    const QString thread=m_current==id?m_threadId:QString(),root=m_threadRoot,source=m_threadSourceChat,post=m_threadSourceMessage;
    closeChat();
    m_current = id;m_composerContext=reply;
    if(!thread.isEmpty()){m_threadId=thread;m_threadRoot=root;m_threadSourceChat=source;m_threadSourceMessage=post;}
    m_targetMessage = messageId.toLongLong() > 0 ? QString::number(messageId.toLongLong()) : QString();
    QVariantMap call = method("openChat");
    call.insert("chat_id", number(id));
    submit(call, "open", id);
    history(true);
    if(!inComments())refreshPinned();
    emit changed();
    return true;
}

void TelegramStore::closeChat()
{
    closeScheduled();
    flushDrafts();
    retireHistoryRequests();
    if (m_ready && !m_current.isEmpty()) {
        QVariantMap call = method("closeChat");
        call.insert("chat_id", number(m_current));
        submit(call, "close", m_current);
    }
    ++m_generation;
    m_current.clear();m_threadId.clear();m_threadRoot.clear();m_threadSourceChat.clear();m_threadSourceMessage.clear();m_pendingComment.clear();m_pinnedMessage.clear();++m_pinnedSerial;m_reactionState.clear();++m_reactionSerial;
    m_lastReadSubmitted=0;
    m_followingLatest=true;
    m_historyLocalEnd=false;
    m_selected.clear();m_selectionState.clear();
    m_composerContext.clear();m_forward.clear();++m_actionSerial;++m_infoSerial;
    m_infoLoading=false;m_infoEnd=true;m_infoRows.clear();m_infoError.clear();emit infoChanged();
    m_targetMessage.clear();
    m_messages.clear(); m_overlays.clear(); m_historyTouched.clear(); m_removed.clear();
    QSet<int> avatarIds;
    foreach(const QVariantMap &avatar,m_avatarFiles)avatarIds.insert(avatar.value("id").toInt());
    foreach(const int fileId,m_files.keys())if(!avatarIds.contains(fileId) && !m_avatarActive.contains(fileId) && !m_avatarQueued.contains(fileId))m_files.remove(fileId);
    m_historyCursor = 0;
    m_loadingHistory = m_historyEnd = false;
    m_error.clear();
    rebuildMessages();
    emit changed();
}

void TelegramStore::refreshPinned(){
    if(!m_ready || m_current.isEmpty())return;
    QVariantMap call=method("getChatPinnedMessage");call.insert("chat_id",number(m_current));
    const QString request=submit(call,"pinned-message",m_current);if(!request.isEmpty())m_requests[request].insert("pinnedSerial",++m_pinnedSerial);
}

void TelegramStore::history(bool initial)
{
    // Explicit opens may complete in the background; automatic paging may not.
    if (!m_ready || m_current.isEmpty() || m_loadingHistory || m_historyEnd)
        return;
    initial = initial || m_historyCursor == 0;
    QVariantMap call = method(inComments()?"getMessageThreadHistory":"getChatHistory");
    if(inComments())call.insert("message_id",number(m_threadRoot));
    call.insert("chat_id", number(m_current));
    call.insert("from_message_id", QVariant::fromValue(initial ? m_targetMessage.toLongLong() : m_historyCursor));
    call.insert("offset", initial && !m_targetMessage.isEmpty() ? -20 : 0);
    call.insert("limit", 50);
    const bool onlyLocal = !inComments() && !m_online;
    if(!inComments())call.insert("only_local", onlyLocal);
    QString id = submit(call, "history", m_current);
    if (id.isEmpty())
        return;
    m_requests[id].insert("initial", initial);
    m_requests[id].insert("onlyLocal", onlyLocal);
    m_historyRequests.insert(id);
    if (onlyLocal) {
        m_requests[id].insert("deadline", m_clock.elapsed() + qMin(15000, m_requestTimeoutMs));
    } else {
        m_historyRetryCall = call;
        m_historyRetryAt = m_clock.elapsed() + qMin(5000, m_requestTimeoutMs / 2);
    }
    m_historyTouched.clear();
    m_loadingHistory = true;
    m_error.clear();
    emit changed();
}

void TelegramStore::retrySlowHistory()
{
    if (!m_foreground || !m_online || m_historyRequests.isEmpty() ||
        !m_historyRetryAt || m_clock.elapsed() < m_historyRetryAt)
        return;
    m_historyRetryAt = 0;
    const QVariantMap original = m_requests.value(*m_historyRequests.constBegin());
    if (original.value("deadline").toLongLong() <= m_clock.elapsed()) return;

    // TDLib can still answer the first read. Both requests belong to one page
    // and share its deadline and edit/delete tracking.
    const QString id = submit(m_historyRetryCall, "history", m_current);
    m_historyRetryCall.clear();
    if (id.isEmpty()) return;
    m_requests[id].insert("initial", original.value("initial"));
    m_requests[id].insert("deadline", original.value("deadline"));
    m_historyRequests.insert(id);
}

void TelegramStore::retireHistoryRequests()
{
    foreach (const QString &id, m_historyRequests) m_requests.remove(id);
    m_historyRequests.clear();
    m_historyRetryAt = 0;
    m_historyRetryCall.clear();
}

void TelegramStore::loadMoreHistory() { if (m_foreground) history(false); }

QString TelegramStore::delivery(const QVariantMap &row) const
{
    if (!row.value("outgoing").toBool()) return QString();
    QString state = row.value("sendingState").toString();
    if (state == "messageSendingStateFailed") return ru("Не отправлено");
    if (state == "messageSendingStatePending")
        return row.value("acknowledged").toBool() ? ru("Telegram получил запрос") : ru("Отправляется…");
    qlonglong read = m_chats.value(m_current).value("readOutbox").toLongLong();
    return read >= row.value("messageId").toLongLong() ? ru("Прочитано") : ru("Отправлено");
}

QVariantMap TelegramStore::messageRow(const QVariantMap &message) const
{
    QVariantMap row;
    const QString id = idString(message.value("id"));
    row.insert("messageId", id);
    row.insert("messageKey", "message:" + id);
    row.insert("threadId",idString(message.value("topic_id").toMap().value("message_thread_id")));
    row.insert("isChannelPost",message.value("is_channel_post"));
    applyReplyInfo(&row,message.value("interaction_info").toMap());
    row.insert("chatId", idString(message.value("chat_id")));
    row.insert("text", messageText(message.value("content").toMap()));
    row.insert("media", mediaInfo(message.value("content").toMap()));
    row.insert("outgoing", message.value("is_outgoing").toBool());
    row.insert("sendingState", message.value("sending_state").toMap().value("@type").toString());
    row.insert("senderId", idString(message.value("sender_id").toMap().value("user_id")));
    row.insert("senderChatId", idString(message.value("sender_id").toMap().value("chat_id")));
    row.insert("date", message.value("date"));
    row.insert("edited", message.value("edit_date").toInt() > 0);
    const QVariantList reactions=message.value("interaction_info").toMap().value("reactions").toMap().value("reactions").toList();
    row.insert("reactions",reactions);QStringList labels;
    foreach(const QVariant &value,reactions){const QVariantMap r=value.toMap(),type=r.value("type").toMap();if(type.value("@type")=="reactionTypeEmoji")labels<<type.value("emoji").toString()+" "+QString::number(r.value("total_count").toInt());}
    row.insert("reactionDisplay",labels.join("  "));
    const QVariantMap content=message.value("content").toMap();
    row.insert("textMessage",content.value("@type")=="messageText");
    applyFormatting(&row,content);
    const QVariantMap reply=message.value("reply_to").toMap();
    row.insert("replyMessageId",idString(reply.value("message_id")));
    row.insert("replyChatId",idString(reply.value("chat_id")));
    row.insert("replyQuote",reply.value("quote").toMap().value("text").toMap().value("text"));
    const QVariantMap origin=message.value("forward_info").toMap().value("origin").toMap();
    row.insert("forwarded",!origin.isEmpty());
    QString forwardAuthor=origin.value("sender_name").toString();
    if(forwardAuthor.isEmpty())forwardAuthor=m_users.value(idString(origin.value("sender_user_id"))).value("name").toString();
    if(forwardAuthor.isEmpty())forwardAuthor=m_chats.value(idString(origin.value(origin.value("@type")=="messageOriginChat"?"sender_chat_id":"chat_id"))).value("title").toString();
    row.insert("forwardAuthor",forwardAuthor);
    return row;
}

QVariantMap TelegramStore::fileInfo(const QVariantMap &file) const
{
    QVariantMap result;
    const QVariantMap local = file.value("local").toMap();
    result.insert("fileId", file.value("id"));
    result.insert("size", qMax(file.value("size").toLongLong(), file.value("expected_size").toLongLong()));
    result.insert("uploaded", file.value("remote").toMap().value("uploaded_size"));
    result.insert("uploading", file.value("remote").toMap().value("is_uploading_active").toBool());
    result.insert("downloaded", local.value("downloaded_size"));
    result.insert("downloading", local.value("is_downloading_active").toBool());
    QString path;
    if (local.value("is_downloading_completed").toBool()) {
        path = privateMediaPath(m_session->dataDirectory(), local.value("path").toString());
    }
    result.insert("path", path);
    result.insert("localUrl", path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString());
    return result;
}

QVariantMap TelegramStore::mediaInfo(const QVariantMap &content) const
{
    QVariantMap file, result;
    const QString type = content.value("@type").toString();
    if (type == "messagePhoto") {
        const QVariantList sizes = content.value("photo").toMap().value("sizes").toList();
        qlonglong best = -1;
        for (int i = 0; i < sizes.size(); ++i) {
            const QVariantMap size = sizes.at(i).toMap();
            qlonglong pixels = size.value("width").toLongLong() * size.value("height").toLongLong();
            if (size.value("width").toInt() > 0 && size.value("height").toInt() > 0 &&
                size.value("width").toInt() <= 1280 && size.value("height").toInt() <= 1280 && pixels > best) {
                best = pixels; file = size.value("photo").toMap();
            }
        }
        result.insert("kind", "photo"); result.insert("name", "photo.jpg"); result.insert("mime", "image/jpeg");
    } else if (type == "messageDocument") {
        QVariantMap document = content.value("document").toMap();
        file = document.value("document").toMap();
        result.insert("kind", "document"); result.insert("name", document.value("file_name"));
        result.insert("mime", document.value("mime_type"));
    }
    if (type == "messageVoiceNote" || type == "messageAudio") {
        const bool voice = type == "messageVoiceNote";
        const QVariantMap audio = content.value(voice ? "voice_note" : "audio").toMap();
        file = audio.value(voice ? "voice" : "audio").toMap();
        result.insert("kind", voice ? "voice" : "audio"); result.insert("duration", audio.value("duration"));
        result.insert("name", voice ? (audio.value("mime_type").toString() == "audio/ogg" ? "voice.ogg" : "voice.m4a") : audio.value("file_name").toString()); result.insert("mime", audio.value("mime_type"));
    }
    if (file.value("id").toInt() <= 0) return QVariantMap();
    if (m_files.contains(file.value("id").toInt())) file = m_files.value(file.value("id").toInt());
    const QVariantMap fields = fileInfo(file);
    for (QVariantMap::const_iterator it = fields.constBegin(); it != fields.constEnd(); ++it) result.insert(it.key(), it.value());
    return result;
}

void TelegramStore::updateFile(const QVariantMap &file)
{
    updateDownload(file);
    updateAvatarFile(file);
    const int id = file.value("id").toInt();
    if (id <= 0) return;
    bool interested = m_downloading.contains(id);
    for (QMap<qlonglong, QVariantMap>::const_iterator message = m_messages.constBegin(); message != m_messages.constEnd(); ++message)
        if (message.value().value("media").toMap().value("fileId").toInt() == id) interested = true;
    if (!interested) return;
    m_files.insert(id, file);
    const QVariantMap fields = fileInfo(file);
    if (!fields.value("path").toString().isEmpty() || !fields.value("downloading").toBool()) m_downloading.remove(id);
    for (QMap<qlonglong, QVariantMap>::iterator it = m_messages.begin(); it != m_messages.end(); ++it) {
        QVariantMap media = it.value().value("media").toMap();
        if (media.value("fileId").toInt() != id) continue;
        for (QVariantMap::const_iterator field = fields.constBegin(); field != fields.constEnd(); ++field) media.insert(field.key(), field.value());
        it.value().insert("media", media);
    }
    rebuildMessages();
    emit changed();
}

QString TelegramStore::mediaPath(const QString &messageId) const
{
    const QString path = m_messages.value(messageId.toLongLong()).value("media").toMap().value("path").toString();
    return privateMediaPath(m_session->dataDirectory(), path);
}

QString TelegramStore::mediaMime(const QString &messageId) const
{
    return m_messages.value(messageId.toLongLong()).value("media").toMap().value("mime").toString();
}

bool TelegramStore::downloadMedia(const QString &messageId)
{
    const QVariantMap media = m_messages.value(messageId.toLongLong()).value("media").toMap();
    const int id = media.value("fileId").toInt();
    if (!m_ready || id <= 0) return false;
    if (!media.value("path").toString().isEmpty()) return true;
    if (!m_online || media.value("size").toLongLong() > 50 * 1024 * 1024) {
        m_error = !m_online ? ru("Для загрузки вложения нужна сеть.") : ru("На этом этапе поддерживаются файлы до 50 МБ.");
        emit changed(); return false;
    }
    if (m_downloading.contains(id)) return true;
    if(m_downloading.size()>=4){m_error=ru("Дождитесь завершения одной из четырёх загрузок.");emit changed();return false;}
    QVariantMap call = method("downloadFile");
    call.insert("file_id", id); call.insert("priority", 16); call.insert("offset", 0);
    call.insert("limit", 50 * 1024 * 1024); call.insert("synchronous", false);
    const QString request = submit(call, "download", m_current);
    if (request.isEmpty()) return false;
    m_requests[request].insert("fileId", id); m_downloading.insert(id);
    QVariantMap item=media;item.insert("chatId",m_current);item.insert("messageId",messageId);item.insert("status",ru("Загрузка…"));item.insert("active",true);
    m_downloads.insert(id,item);m_autoSuppressed.remove(id);
    m_downloads[id].insert("serial",++m_downloadSerial);m_requests[request].insert("fileSerial",m_downloadSerial);
    while(m_downloads.size()>64){bool removed=false;foreach(const int key,m_downloads.keys())if(!m_downloads.value(key).value("active").toBool() && !m_downloads.value(key).value("cancelPending").toBool()){m_downloads.remove(key);m_autoSuppressed.remove(key);removed=true;break;}if(!removed)break;}
    emit downloadsChanged();
    return true;
}

void TelegramStore::upsertMessage(const QVariantMap &message)
{
    qlonglong id = message.value("id").toLongLong();
    if (!id || idString(message.value("chat_id")) != m_current || m_removed.contains(id) || !belongsToThread(message))
        return;
    QVariantMap row = messageRow(message);
    const QVariantMap previous = m_messages.value(id);
    if (!previous.isEmpty() && previous.value("sendingState") != "messageSendingStatePending" &&
        row.value("sendingState") == "messageSendingStatePending")
        return; // A stale send response must not undo a terminal update.
    QVariantMap overlay = m_overlays.value(id);
    if(overlay.contains("interaction_info"))applyReplyInfo(&row,overlay.value("interaction_info").toMap());
    if(overlay.contains("interaction_info")){
        const QVariantList reactions=overlay.value("interaction_info").toMap().value("reactions").toMap().value("reactions").toList();QStringList labels;
        foreach(const QVariant &value,reactions){const QVariantMap reaction=value.toMap(),kind=reaction.value("type").toMap();if(kind.value("@type")=="reactionTypeEmoji")labels<<kind.value("emoji").toString()+" "+QString::number(reaction.value("total_count").toInt());}
        row.insert("reactions",reactions);row.insert("reactionDisplay",labels.join("  "));
    }
    if (overlay.contains("content")) {
        row.insert("text", messageText(overlay.value("content").toMap()));
        row.insert("media", mediaInfo(overlay.value("content").toMap()));
        const QVariantMap content=overlay.value("content").toMap();
        applyFormatting(&row,content);
        row.insert("textMessage",content.value("@type")=="messageText");
    QString link;const QVariantMap formatted=content.value(content.value("@type")=="messageText"?"text":"caption").toMap();
    const QString body=formatted.value("text").toString();
    foreach(const QVariant &value,formatted.value("entities").toList()){
        const QVariantMap entity=value.toMap(),kind=entity.value("type").toMap();const QString type=kind.value("@type").toString();
        const int offset=entity.value("offset").toInt(),length=entity.value("length").toInt();if(offset<0 || length<=0 || offset>body.size()-length)continue;
        if(type=="textEntityTypeTextUrl")link=kind.value("url").toString();
        else if(type=="textEntityTypeUrl")link=body.mid(offset,length);
        else if(type=="textEntityTypeMention")link="https://t.me/"+body.mid(offset+1,length-1);
        if(!link.isEmpty())break;
    }
    if(link.isEmpty()){QRegExp rx("(https?://[^\\s<>]+|t\\.me/[A-Za-z0-9_+/]+|@[A-Za-z][A-Za-z0-9_]{4,31})");if(rx.indexIn(body)>=0)link=rx.cap(1);}
    if(link.startsWith("@"))link="https://t.me/"+link.mid(1);else if(link.startsWith("t.me/"))link="https://"+link;
    const QUrl target(link);if(target.isValid() && (target.scheme()=="https" || target.scheme()=="http") && !target.host().isEmpty() && target.userInfo().isEmpty())row.insert("link",target.toString());
    }
    if (overlay.contains("edited")) row.insert("edited", overlay.value("edited"));
    if (overlay.value("acknowledged").toBool()) row.insert("acknowledged", true);
    if (m_messages.value(id).value("acknowledged").toBool()) row.insert("acknowledged", true);
    m_messages.insert(id, row);
    if (row.value("sendingState") == "messageSendingStateFailed" &&
        message.value("content").toMap().value("@type") == "messageText")
        m_failedDrafts.insert(draftKey(), row.value("text").toString());
}

void TelegramStore::rebuildMessages()
{
    if(m_batching){m_messagesDirty=true;return;}
    // Keep a rolling window while following new messages. Reading older history
    // and messages involved in an action are never evicted under the reader.
    if(m_followingLatest && !m_loadingHistory && m_targetMessage.isEmpty() && m_selected.isEmpty() && m_composerContext.isEmpty() && m_messages.size()>1000){
        QMap<qlonglong,QVariantMap>::iterator old=m_messages.begin();
        while(m_messages.size()>800 && old!=m_messages.end()){
            if(!old.value().value("sendingState").toString().isEmpty()){++old;continue;}
            const qlonglong id=old.key();old=m_messages.erase(old);m_overlays.remove(id);m_historyTouched.remove(id);
        }
        if(!m_messages.isEmpty()){m_historyCursor=m_messages.begin().key();m_historyEnd=false;}
    }
    QVariantList rows;
    QMap<qlonglong, QVariantMap>::const_iterator it;
    for (it = m_messages.constBegin(); it != m_messages.constEnd(); ++it) {
        QVariantMap row = it.value();
        QString author = ru("Участник");
        if (row.value("outgoing").toBool()) author = ru("Вы");
        else if (m_users.contains(row.value("senderId").toString()))
            author = m_users.value(row.value("senderId").toString()).value("name").toString();
        else if (m_chats.contains(row.value("senderChatId").toString()))
            author = m_chats.value(row.value("senderChatId").toString()).value("title").toString();
        if(row.value("replyQuote").toString().isEmpty() && row.value("replyMessageId").toLongLong()>0)
            row.insert("replyQuote",m_messages.value(row.value("replyMessageId").toLongLong()).value("text",ru("Ответ на сообщение")).toString().left(240));
        row.insert("selected",m_selected.contains(it.key()));
        row.insert("author", author);
        row.insert("delivery", delivery(row));
        row.insert("searchTarget", row.value("messageId").toString() == m_targetMessage && !m_targetMessage.isEmpty());
        rows.append(row);
    }
    QList<QVariantMap> local;
    QMap<QString, QVariantMap>::const_iterator outgoing;
    for (outgoing = m_outbox.constBegin(); outgoing != m_outbox.constEnd(); ++outgoing) {
        if (outgoing.value().value("chatId") == m_current && outgoing.value().value("threadId").toString()==m_threadId)
            local.append(outgoing.value());
    }
    std::sort(local.begin(), local.end(), requestBefore);
    for (int i = 0; i < local.size(); ++i) rows.append(local.at(i));
    const QVariantMap chat = m_chats.value(m_current);
    const QString type = chat.value("type").toString();
    rows = MessagePresentation::decorate(rows,
        type == "chatTypeBasicGroup" || (type == "chatTypeSupergroup" && !chat.value("isChannel").toBool()),
        chat.value("readInbox").toString(), chat.value("unreadCount").toInt() > 0);
    if (rows != m_messageRows) {
        m_messageRows = rows;
        emit messagesChanged();
    }
}

void TelegramStore::beginBatch(){m_batching=true;}
void TelegramStore::endBatch(){
    m_batching=false;
    const bool chats=m_chatsDirty,messages=m_messagesDirty;
    m_chatsDirty=m_messagesDirty=false;
    if(chats)rebuildChats();
    if(messages)rebuildMessages();
    if (m_chatUpdatesPending) {
        m_chatUpdatesPending = false;
        notifyChatUpdates();
    }
    if(m_scrollPending){m_scrollPending=false;emit scrollRequested();}
    const int incoming=m_pendingIncoming;m_pendingIncoming=0;
    for(int i=0;i<incoming;++i)emit incomingMessage();
    autoDownload();
}
void TelegramStore::notifyChatUpdates()
{
    if (m_batching) {
        m_chatUpdatesPending = true;
        return;
    }
    emit recipientChatsChanged();
    emit changed();
}
void TelegramStore::requestScroll(){if(m_batching)m_scrollPending=true;else emit scrollRequested();}
void TelegramStore::notifyIncoming(){if(m_batching)++m_pendingIncoming;else emit incomingMessage();}

bool TelegramStore::sendText(const QString &text, uint scheduledAt)
{
    if(m_composerContext.value("mode")=="edit")return scheduledAt ? false : editText(text);
    if (m_outbox.size() >= 64) {
        m_error = ru("Слишком много сообщений без подтверждения. Проверьте историю и перезапустите движок.");
        emit changed();
        return false;
    }
    if (!canSend() || text.trimmed().isEmpty() || text.size() > 4096) {
        m_error = text.size() > 4096 ? ru("Сообщение длиннее 4096 символов.") : ru("Отправка сейчас недоступна.");
        emit changed();
        return false;
    }
    QVariantMap content = method("inputMessageText");
    QVariantMap formatted = method("formattedText");
    formatted.insert("text", text); // Keep the user's spaces and newlines.
    formatted.insert("entities", QVariantList());
    content.insert("text", formatted);
    content.insert("clear_draft", false);
    return sendContent(content, text, QVariantMap(), scheduledAt);
}

bool TelegramStore::sendContent(const QVariantMap &content, const QString &text, const QVariantMap &attachment, uint scheduledAt)
{
    if (m_outbox.size() >= 64) return false;
    const uint now=QDateTime::currentDateTime().toTime_t();
    if (scheduledAt && (scheduledAt <= now || qint64(scheduledAt) > qint64(now) + 366LL*86400 || scheduledAt > 2147483647U)) {
        m_error=ru("Выберите будущее время в пределах года.");emit changed();return false;
    }
    QVariantMap call = method("sendMessage"); call.insert("chat_id", number(m_current));
    QVariantMap options = method("messageSendOptions"); options.insert("paid_message_star_count", 0);
    if(scheduledAt){QVariantMap schedule=method("messageSchedulingStateSendAtDate");schedule.insert("send_date",scheduledAt);schedule.insert("repeat_period",0);options.insert("scheduling_state",schedule);}
    call.insert("options", options);
    call.insert("input_message_content", content);
    if(inComments()){QVariantMap topic=method("messageTopicThread");topic.insert("message_thread_id",number(m_threadId));call.insert("topic_id",topic);
        if(m_composerContext.value("mode")!="reply"){QVariantMap reply=method("inputMessageReplyToMessage");reply.insert("message_id",number(m_threadRoot));call.insert("reply_to",reply);}}
    if(m_composerContext.value("mode")=="reply")call.insert("reply_to",m_composerContext.value("replyTo"));
    QVariantMap intent = attachment;intent.insert("threadId",m_threadId);
    intent.insert("type", attachment.isEmpty() ? "text" : "attachment");
    if (attachment.isEmpty()) intent.insert("text", text);
    if(call.contains("reply_to"))intent.insert("replyTo",call.value("reply_to"));
    if(scheduledAt)intent.insert("scheduledAt",scheduledAt);
    const QString recordId = m_local.prepare(m_account, m_current, intent);
    if (recordId.isEmpty() || !m_local.transition(recordId, "submitting")) {
        m_error = m_local.error(); emit changed(); return false;
    }
    const QString id = submit(call, "send", m_current);
    if (id.isEmpty()) { m_local.transition(recordId, "uncertain"); emit changed(); return false; }
    m_requests[id].insert("recordId", recordId);m_requests[id].insert("threadId",m_threadId);
    if(scheduledAt)m_requests[id].insert("scheduledAt",scheduledAt);
    QVariantMap row; row.insert("recordId", recordId);
    row.insert("messageKey", "request:" + id);
    row.insert("sequence", id.section('.', -1).toUInt());
    row.insert("chatId", m_current);row.insert("threadId",m_threadId);
    row.insert("author", ru("Вы"));
    row.insert("text", text);
    row.insert("outgoing", true);
    row.insert("date", QDateTime::currentDateTime().toTime_t());
    row.insert("time", QDateTime::currentDateTime().toString("dd.MM HH:mm"));
    row.insert("delivery", scheduledAt ? ru("Планирование отправки…") : ru("Ожидание ответа движка…"));
    row.insert("edited", false);
    row.insert("attachment", attachment);
    row.insert("replyQuote",m_composerContext.value("preview"));row.insert("replyMessageId",m_composerContext.value("messageId"));
    m_composerContext.clear();
    m_outbox.insert(id, row);
    setDraft(QString());
    flushDrafts();
    m_error.clear();
    rebuildMessages();
    emit changed();
    requestScroll();
    return true;
}

QString TelegramStore::takeFailedDraft()
{
    const QString text = failedDraft();
    const QVariantMap item = m_local.latest(m_account, m_current, "text", QStringList() << "failed" << "uncertain", m_threadId);
    if (!item.isEmpty()) m_recoveredTexts.insert(item.value("recordId").toString());
    const QVariantMap reply=item.value("replyTo").toMap();
    if(!reply.isEmpty()) {m_composerContext.clear();m_composerContext.insert("mode","reply");m_composerContext.insert("messageId",idString(reply.value("message_id")));m_composerContext.insert("replyTo",reply);m_composerContext.insert("preview",reply.value("quote").toMap().value("text").toMap().value("text"));}
    m_failedDrafts.remove(draftKey()); setDraft(text); flushDrafts(); emit changed(); return text;
}

void TelegramStore::setSearchQuery(const QString &text)
{
    m_search = text.trimmed().left(256);
    rebuildChats();
    emit changed();
}

QVariantList TelegramStore::knownChats() const
{
    QVariantList result;
    foreach (const QVariantMap &chat, m_chats) result.append(chat);
    return result;
}

void TelegramStore::setChatCategory(const QString &category)
{
    const QString next = category == "personal" || category == "community" ? category : QString();
    if (next == m_chatCategory) return;
    m_chatCategory = next; rebuildChats(); emit changed();
}

int TelegramStore::personalCount() const
{
    int count = 0;
    foreach (const QVariantMap &chat, m_chats) {
        const QString type = chat.value("type").toString();
        if (chat.value(m_archiveView?"archiveOrder":"mainOrder").toLongLong() > 0 && (type == "chatTypePrivate" || type == "chatTypeSecret")) ++count;
    }
    return count;
}

int TelegramStore::communityCount() const
{
    int count = 0;
    foreach (const QVariantMap &chat, m_chats) {
        const QString type = chat.value("type").toString();
        if (chat.value(m_archiveView?"archiveOrder":"mainOrder").toLongLong() > 0 && type != "chatTypePrivate" && type != "chatTypeSecret") ++count;
    }
    return count;
}

int TelegramStore::targetMessageIndex() const
{
    if (!m_targetMessage.isEmpty())
        for (int i = 0; i < m_messageRows.size(); ++i)
            if (m_messageRows.at(i).toMap().value("messageId").toString() == m_targetMessage) return i;
    return -1;
}

QString TelegramStore::nextUnreadChat() const
{
    int current = -1;
    for (int i = 0; i < m_chatRows.size(); ++i)
        if (m_chatRows.at(i).toMap().value("chatId") == m_current) current = i;
    for (int step = 1; step <= m_chatRows.size(); ++step) {
        QVariantMap row = m_chatRows.at((current + step) % m_chatRows.size()).toMap();
        if (row.value("unreadCount").toInt() > 0 && row.value("chatId") != m_current)
            return row.value("chatId").toString();
    }
    return QString();
}

bool TelegramStore::markRead()
{
    if (!m_foreground || !m_ready || m_current.isEmpty() || m_messages.isEmpty()) return false;
    qlonglong latest = 0;
    for (QMap<qlonglong, QVariantMap>::const_iterator it = m_messages.constBegin(); it != m_messages.constEnd(); ++it)
        if (it.value().value("sendingState").toString().isEmpty() && it.key() > latest) latest = it.key();
    if(!inComments())latest=qMax(latest,m_chats.value(m_current).value("lastMessageId").toLongLong());
    if (!latest) return false;
    if(latest<=m_lastReadSubmitted)return true;
    QVariantMap call = method("viewMessages"); call.insert("chat_id", number(m_current));
    call.insert("message_ids", QVariantList() << QVariant::fromValue(latest));
    call.insert("source", method(inComments()?"messageSourceMessageThreadHistory":"messageSourceChatHistory")); call.insert("force_read", true);
    const QString request=submit(call,"read",m_current);
    if(request.isEmpty())return false;
    m_lastReadSubmitted=latest;return true;
}

void TelegramStore::setDraft(const QString &text)
{
    if(m_composerContext.value("mode")=="edit")return;
    if (!m_ready || m_current.isEmpty() || text.size() > 4096 || m_drafts.value(draftKey()) == text) return;
    m_drafts.insert(draftKey(), text);
    m_dirtyDrafts.insert(draftKey(), text);
    m_draftTimer.start(600);
    emit changed();
}

void TelegramStore::flushDrafts()
{
    if (m_account.isEmpty()) return;
    foreach (const QString &chat, m_dirtyDrafts.keys())
        if (!m_local.rememberDraft(m_account, chat, m_dirtyDrafts.value(chat))) m_error = m_local.error();
    if (!m_ready) return;
    const QStringList ids = m_dirtyDrafts.keys();
    foreach (const QString &chat, ids) {
        if (m_draftInFlight.contains(chat)) continue;
        QVariantMap call = method("setChatDraftMessage");
        call.insert("chat_id", number(chat.section("/thread/",0,0)));
        const QString thread=chat.section("/thread/",1,1);
        if(!thread.isEmpty()){QVariantMap topic=method("messageTopicThread");topic.insert("message_thread_id",number(thread));call.insert("topic_id",topic);}
        const QString text = m_dirtyDrafts.value(chat);
        if (!text.isEmpty()) {
            QVariantMap draft = method("draftMessage");
            QVariantMap content = method("draftMessageContentText"), formatted = method("formattedText");
            formatted.insert("text", text); formatted.insert("entities", QVariantList());
            content.insert("text", formatted); draft.insert("content", content);
            draft.insert("date", QDateTime::currentDateTime().toTime_t());
            call.insert("draft_message", draft);
        } else call.insert("draft_message", QVariant());
        const QString id = submit(call, "draft", chat);
        if (!id.isEmpty()) {
            m_requests[id].insert("text", text);m_requests[id].insert("draftKey",chat);
            m_draftInFlight.insert(chat, id);
        }
    }
}

void TelegramStore::response(const QVariantMap &object, const QVariantMap &pending)
{
    ++m_completedRequests;m_requestLatency+=qMax(qlonglong(0),m_clock.elapsed()-pending.value("started").toLongLong());
    if(threadResponse(object,pending))return;
    if(toolsResponse(object,pending))return;
    if(peopleResponse(object,pending))return;
    if(actionResponse(object,pending))return;
    const QString type = object.value("@type").toString();
    const QString kind = pending.value("kind").toString();
    const QString chat = pending.value("chatId").toString();
    const bool current = chat == m_current && pending.value("generation").toUInt() == m_generation;
    const bool error = type == "error";
    if (kind == "network" && error) {
        if (m_sentNetwork == pending.value("network")) m_sentNetwork.clear();
    } else if (kind == "me") {
        if (!error && type == "user" && object.value("id").toLongLong() > 0) {
            m_account = idString(object.value("id"));
            emit avatarChanged(QString());
            if (!m_local.open()) m_error = m_local.error();
            foreach (const QString &existingChat, m_chats.keys())
                restoreChatDraft(existingChat, QVariant(), true);
            if (!m_dirtyDrafts.isEmpty()) m_draftTimer.start(600);
            foreach (const QVariant &value, m_local.entries(m_account)) {
                const QVariantMap item = value.toMap();
                if (item.value("state") == "uncertain" && !item.value("messageId").toString().isEmpty()) {
                    if (m_requests.size() >= 48) break; // Reserve capacity for the chat list and interactive requests.
                    QVariantMap call = method("getMessage"); call.insert("chat_id", number(item.value("chatId").toString()));
                    call.insert("message_id", number(item.value("messageId").toString()));
                    const QString request = submit(call, "reconcile", item.value("chatId").toString());
                    if (!request.isEmpty()) m_requests[request].insert("recordId", item.value("recordId"));
                }
            }
        } else m_error = ru("Не удалось определить аккаунт для сохранённых черновиков.");
    } else if (kind == "reconcile") {
        const QVariantMap intent=m_local.entry(pending.value("recordId").toString());
        if (type == "message" && idString(object.value("chat_id")) == chat &&
            (intent.value("type")!="edit" || messageText(object.value("content").toMap())==intent.value("text").toString()))
            recordSendResult(object, pending.value("recordId").toString());
    } else if (kind == "chats") {
        m_loadingChats = false;
        if (error && object.value("code").toInt() == 404) { if(pending.value("archive").toBool())m_archiveEnd=true;else m_chatsEnd=true; }
        else if (error) m_error = ru("Не удалось загрузить чаты (код %1).").arg(object.value("code").toInt());
    } else if (kind == "history" && current) {
        m_historyRequests.remove(object.value("@extra").toString());
        if (type != "messages" && !m_historyRequests.isEmpty()) return;
        retireHistoryRequests();
        m_loadingHistory = false;
        if (error) m_error = ru("Не удалось загрузить историю (код %1).").arg(object.value("code").toInt());
        else if (type == "messages") {
            QVariantList messages = object.value("messages").toList();
            qlonglong oldest = 0;
            for (int i = 0; i < messages.size(); ++i) {
                QVariantMap message = messages.at(i).toMap();
                qlonglong id = message.value("id").toLongLong();
                if (idString(message.value("chat_id")) != m_current || !id) continue;
                if (oldest == 0 || id < oldest) oldest = id;
                if (!m_historyTouched.contains(id)) upsertMessage(message);
            }
            if (messages.isEmpty() || (!pending.value("initial").toBool() && oldest >= m_historyCursor))
                m_historyEnd = true;
            if(m_historyEnd && pending.value("onlyLocal").toBool())m_historyLocalEnd=true;
            if (oldest) m_historyCursor = oldest;
            rebuildMessages();
            if (pending.value("initial").toBool()) { markRead(); requestScroll(); }
            // A local-cache response may arrive after the network-ready update.
            if(m_historyLocalEnd && m_connection==ru("Telegram подключён")){
                m_historyLocalEnd=false;m_historyEnd=false;history(m_messages.isEmpty());
            }
        }
    } else if (kind == "pinned-message") {
        if(current && pending.value("pinnedSerial").toUInt()==m_pinnedSerial){
            if(type=="message" && idString(object.value("chat_id"))==m_current){m_pinnedMessage=messageRow(object);m_pinnedMessage.insert("preview",messageText(object.value("content").toMap()).left(160));}
            else if(error && object.value("code").toInt()==404)m_pinnedMessage.clear();
            emit changed();
        }
    } else if (kind == "storage") {
        if(error)m_storageSummary=ru("Не удалось получить сведения о файлах.");
        else if(type=="storageStatistics"){
            qlonglong photos=0,documents=0;
            foreach(const QVariant &chat,object.value("by_chat").toList())foreach(const QVariant &value,chat.toMap().value("by_file_type").toList()){
                const QVariantMap row=value.toMap();const QString type=row.value("file_type").toMap().value("@type").toString();
                if(type=="fileTypePhoto")photos+=row.value("size").toLongLong();else if(type=="fileTypeDocument")documents+=row.value("size").toLongLong();
            }
            m_storageSummary=ru("Фото: %1 МБ · документы: %2 МБ · всего файлов: %3 МБ").arg(double(photos)/(1024*1024),0,'f',1).arg(double(documents)/(1024*1024),0,'f',1).arg(double(object.value("size").toLongLong())/(1024*1024),0,'f',1);
        }
    } else if (kind == "download") {
        const int trackedId=pending.value("fileId").toInt();
        if(m_downloads.contains(trackedId) && pending.value("fileSerial").toUInt()!=m_downloads.value(trackedId).value("serial").toUInt())return;
        if (error) {
            m_downloading.remove(pending.value("fileId").toInt());
            const int fileId=pending.value("fileId").toInt();
            if(m_downloads.contains(fileId)){m_downloads[fileId].insert("active",false);m_downloads[fileId].insert("status",ru("Не удалось скачать. Можно повторить."));emit downloadsChanged();}
            if (chat == m_current) m_error = ru("Вложение не загрузилось (код %1).").arg(object.value("code").toInt());
        } else if (type == "file") updateFile(object);
    } else if (kind == "draft") {
        const QString draft=pending.value("draftKey",chat).toString();
        m_draftInFlight.remove(draft);
        if (!error && m_dirtyDrafts.value(draft) == pending.value("text").toString()) {
            m_local.acknowledgeDraft(m_account, draft, pending.value("text").toString()); m_dirtyDrafts.remove(draft);
        }
        else if (error) {
            if (chat == m_current) m_error = ru("Черновик пока не сохранён. Он остаётся в памяти приложения.");
        } else m_draftTimer.start(150);
    } else if (kind == "send") {
        const QString context=pending.value("threadId").toString().isEmpty()?chat:chat+"/thread/"+pending.value("threadId").toString();
        const QString extra = object.value("@extra").toString();
        const QString text = m_outbox.value(extra).value("text").toString();
        if (error) {
            m_local.transition(pending.value("recordId").toString(), "failed");
            if (m_outbox.contains(extra)) m_outbox[extra].insert("delivery", ru("Не отправлено"));
            const QVariantMap attachment = m_outbox.value(extra).value("attachment").toMap();
            if (attachment.isEmpty()) m_failedDrafts.insert(context, text);
            else m_failedAttachments.insert(context, attachment);
            if (chat == m_current) m_error = ru("Отправка отклонена (код %1). Вложение или текст можно вернуть в поле ввода.").arg(object.value("code").toInt());
        } else if (type == "message") {
            const QVariantMap attachment = m_outbox.value(extra).value("attachment").toMap();
            if (!attachment.isEmpty()) {
                const QString key = chat + ":" + idString(object.value("id"));
                const QString outcome = m_earlyAttachmentOutcomes.value(key);
                const bool failed = outcome == "failed" || (chat == m_current &&
                    m_messages.value(object.value("id").toLongLong()).value("sendingState") == "messageSendingStateFailed");
                if (failed) m_failedAttachments.insert(context, attachment);
                else {
                    if (outcome != "succeeded") m_sentAttachments.insert(key, attachment);
                    m_failedAttachments.remove(context);
                }
            }
            const QString earlyOutcome=m_earlyAttachmentOutcomes.take(chat + ":" + idString(object.value("id")));
            recordSendResult(object, pending.value("recordId").toString(), earlyOutcome);
            m_outbox.remove(extra);
            if(!pending.value("scheduledAt").toUInt())upsertMessage(object);
            else if(chat==m_current)m_error=(earlyOutcome=="succeeded" || object.value("sending_state").toMap().isEmpty()) && !object.value("scheduling_state").toMap().isEmpty()
                ? ru("Сообщение запланировано в Telegram.") : ru("Отправляем запрос на планирование…");
            bool uncertain = false;
            QMap<QString, QVariantMap>::const_iterator it;
            for (it = m_outbox.constBegin(); it != m_outbox.constEnd(); ++it)
                if (it.value().value("chatId") == m_current && it.value().value("delivery") == ru("Результат отправки неизвестен"))
                    uncertain = true;
            if (!uncertain && m_error == ru("Нет ответа об отправке. Проверьте историю перед повтором."))
                m_error.clear();
        }
        rebuildMessages();
    } else if (kind == "read" && current && error) {
        m_lastReadSubmitted=0;
        m_error = ru("Не удалось отметить переписку прочитанной.");
    }
    else if (kind == "open" && current && error)
        m_error = ru("Не удалось открыть чат (код %1).").arg(object.value("code").toInt());
    emit changed();
}

void TelegramStore::consume(const QVariantMap &object)
{
    const QString type = object.value("@type").toString();
    if (type == "updateConnectionState") {
        QString state = object.value("state").toMap().value("@type").toString();
        m_online = state != "connectionStateWaitingForNetwork";
        if (state == "connectionStateReady") m_connection = ru("Telegram подключён");
        else if (state == "connectionStateWaitingForNetwork") m_connection = ru("Ожидание сети");
        else if (state == "connectionStateUpdating") m_connection = ru("Обновление…");
        else { m_connection = ru("Подключение…"); }
        if(state=="connectionStateReady"){emit avatarChanged(QString());pumpAvatars();}
        if(state=="connectionStateReady" && m_historyLocalEnd){m_historyLocalEnd=false;m_historyEnd=false;if(!m_loadingHistory && !m_current.isEmpty())history(m_messages.isEmpty());}
        emit changed();
        return;
    }
    if (m_seenPhase == "stopped" || m_seenPhase == "failed" || m_seenPhase == "closing") return;
    if(peopleUpdate(object))return;
    const QString extra = object.value("@extra").toString();
    if (m_requests.contains(extra)) response(object, m_requests.take(extra));
    if (type == "updateFile") { updateFile(object.value("file").toMap()); return; }
    const QString chatId = idString(object.value("chat_id"));
    if (type == "updateMessageContent" && m_chats.contains(chatId) &&
        m_chats.value(chatId).value("lastMessageId").toLongLong() == object.value("message_id").toLongLong()) {
        m_chats[chatId].insert("preview", messageText(object.value("new_content").toMap()).left(160));
        rebuildChats();
    }
    if (type == "updateNewChat" || type == "chat") {
        const QVariantMap source = type == "chat" ? object : object.value("chat").toMap();
        const QString id = idString(source.value("id"));
        QVariantMap chat;
        chat.insert("chatId", id);
        chat.insert("title", source.value("title"));chat.insert("avatarKey","chat:"+id);
        chat.insert("type", source.value("type").toMap().value("@type"));
        chat.insert("isChannel", source.value("type").toMap().value("is_channel"));
        const QVariantMap chatType=source.value("type").toMap();
        chat.insert("peerId",idString(chatType.value(chatType.value("@type")=="chatTypePrivate"?"user_id":chatType.value("@type")=="chatTypeBasicGroup"?"basic_group_id":"supergroup_id")));
        chat.insert("notificationSettings",source.value("notification_settings"));
        chat.insert("topics", source.value("view_as_topics"));
        chat.insert("permissions", source.value("permissions"));
        chat.insert("canSend", source.value("permissions").toMap().value("can_send_basic_messages"));
        chat.insert("unreadCount", source.value("unread_count"));
        chat.insert("readInbox", idString(source.value("last_read_inbox_message_id")));
        chat.insert("readOutbox", idString(source.value("last_read_outbox_message_id")));
        chat.insert("lastMessageId", idString(source.value("last_message").toMap().value("id")));
        chat.insert("lastDate", source.value("last_message").toMap().value("date"));
        chat.insert("preview", source.value("last_message").toMap().isEmpty() ? QString() : messageText(source.value("last_message").toMap().value("content").toMap()).left(160));
        restoreChatDraft(id, source.value("draft_message"));
        setPositions(&chat, source.value("positions").toList());
        m_chats.insert(id, chat);
        rememberAvatar("chat:"+id,source.value("photo").toMap());
        rebuildChats();
        notifyChatUpdates();
        if(m_openingContact && id==m_pendingContactChat){m_openingContact=false;m_pendingContactChat.clear();emit contactChatReady(id);emit changed();}
        if(m_contactSection==1)rebuildContacts();
    } else if (type == "updateUser" || type=="user") {
        upsertUser(type=="user"?object:object.value("user").toMap());
        rebuildMessages();if(m_infoSection==1 && !m_members.isEmpty())updateInfoRows();
    } else if (type.startsWith("updateChat") && m_chats.contains(chatId)) {
        QVariantMap &chat = m_chats[chatId];
        if(type=="updateChatPhoto")rememberAvatar("chat:"+chatId,object.value("photo").toMap());
        else if (type == "updateChatTitle") chat.insert("title", object.value("title"));
        else if (type == "updateChatPermissions") { chat.insert("permissions", object.value("permissions")); chat.insert("canSend", object.value("permissions").toMap().value("can_send_basic_messages")); }
        else if (type == "updateChatReadInbox") {
            chat.insert("unreadCount", object.value("unread_count"));
            chat.insert("readInbox", idString(object.value("last_read_inbox_message_id")));
            if (chatId == m_current) rebuildMessages();
        }
        else if (type == "updateChatNotificationSettings")chat.insert("notificationSettings",object.value("notification_settings"));
        else if (type == "updateChatReadOutbox") chat.insert("readOutbox", idString(object.value("last_read_outbox_message_id")));
        else if (type == "updateChatViewAsTopics") chat.insert("topics", object.value("view_as_topics"));
        else if (type == "updateChatPosition") {
            QVariantMap position = object.value("position").toMap();
            const QString type=position.value("list").toMap().value("@type").toString();
            const QString prefix=type=="chatListMain"?"main":type=="chatListArchive"?"archive":"";
            if(!prefix.isEmpty()) {chat.insert(prefix+"Order",QVariant::fromValue(position.value("order").toLongLong()));chat.insert(prefix+"Pinned",position.value("is_pinned"));}
        } else if (type == "updateChatLastMessage") {
            chat.insert("preview", object.value("last_message").toMap().isEmpty() ? QString() : messageText(object.value("last_message").toMap().value("content").toMap()).left(160));
            chat.insert("lastMessageId", idString(object.value("last_message").toMap().value("id")));
            chat.insert("lastDate", object.value("last_message").toMap().value("date"));
            setPositions(&chat, object.value("positions").toList());
        } else if (type == "updateChatDraftMessage") {
            setPositions(&chat, object.value("positions").toList());
            restoreChatDraft(chatId, object.value("draft_message"));
        }
        rebuildChats();
        if(m_contactSection==1)rebuildContacts();
        if (chatId == m_current) rebuildMessages();
        notifyChatUpdates();
    } else if (type == "updateNewMessage" || type == "updateMessageSendSucceeded" || type == "updateMessageSendFailed") {
        QVariantMap message = object.value("message").toMap();
        const QString sendChat = idString(message.value("chat_id"));
        const QString attachmentKey = sendChat + ":" + idString(object.value("old_message_id"));
        if(type=="updateMessageSendSucceeded" || type=="updateMessageSendFailed") {
            foreach(const QVariantMap &request,m_requests)
                if(request.value("kind")=="forward" && request.value("chatId")==sendChat && m_earlyForwards.size()<64) {
                    m_earlyForwards.insert(attachmentKey,message);break;
                }
        }
        if ((type == "updateMessageSendFailed" || type == "updateMessageSendSucceeded") && !m_sentAttachments.contains(attachmentKey)) {
            for (QMap<QString, QVariantMap>::const_iterator it = m_outbox.constBegin(); it != m_outbox.constEnd(); ++it)
                if (it.value().value("chatId") == sendChat && !it.value().value("recordId").toString().isEmpty()) {
                    if (m_earlyAttachmentOutcomes.size() < 128)
                        m_earlyAttachmentOutcomes.insert(attachmentKey, type == "updateMessageSendFailed" ? "failed" : "succeeded");
                    break;
                }
        }
        const QString scheduledRecord=m_localMessages.value(attachmentKey);
        const bool scheduled=m_local.entry(scheduledRecord).value("scheduledAt").toUInt()!=0 || !message.value("scheduling_state").toMap().isEmpty();
        if (type == "updateMessageSendFailed" || type == "updateMessageSendSucceeded") {
            const QString recordId = m_localMessages.take(attachmentKey);
            recordSendResult(message, recordId, type == "updateMessageSendFailed" ? "failed" : "succeeded");
        }
        if (type == "updateMessageSendFailed" && m_sentAttachments.contains(attachmentKey))
            m_failedAttachments.insert(sendChat, m_sentAttachments.take(attachmentKey));
        if (type == "updateMessageSendSucceeded") m_sentAttachments.remove(attachmentKey);
        if (idString(message.value("chat_id")) == m_current && belongsToThread(message)) {
            qlonglong id = message.value("id").toLongLong();
            if (type != "updateNewMessage") {
                qlonglong old = object.value("old_message_id").toLongLong();
                m_messages.remove(old);
                if (old != id) m_removed.insert(old);
            }
            const bool incoming=type=="updateNewMessage" && !message.value("is_outgoing").toBool() && !m_messages.contains(id) && !m_removed.contains(id);
            m_historyTouched.insert(id);
            if(!scheduled || type=="updateMessageSendFailed")upsertMessage(message);
            if(scheduled && type=="updateMessageSendSucceeded" && !message.value("scheduling_state").toMap().isEmpty())m_error=ru("Сообщение запланировано в Telegram.");
            if (type == "updateMessageSendFailed") {
                if (message.value("content").toMap().value("@type") == "messageText")
                    m_failedDrafts.insert(draftKey(), messageText(message.value("content").toMap()));
                m_error = ru("Сообщение не отправлено. Можно восстановить текст или вложение.");
            }
            rebuildMessages();
            if(incoming)notifyIncoming();
            emit changed();
        }
    } else if (chatId == m_current && !m_current.isEmpty()) {
        qlonglong id = object.value("message_id").toLongLong();
        if (type == "updateMessageContent") {
            m_overlays[id].insert("content", object.value("new_content"));
            if(id==m_pinnedMessage.value("messageId").toLongLong()){m_pinnedMessage.insert("preview",messageText(object.value("new_content").toMap()).left(160));emit changed();}
            if (m_messages.contains(id)) {
                m_messages[id].insert("text", messageText(object.value("new_content").toMap()));
                m_messages[id].insert("media", mediaInfo(object.value("new_content").toMap()));
                const QVariantMap content=object.value("new_content").toMap();
                applyFormatting(&m_messages[id],content);
                m_messages[id].insert("textMessage",content.value("@type")=="messageText");
            }
        } else if(type=="updateMessageInteractionInfo") {
            m_overlays[id].insert("interaction_info",object.value("interaction_info"));
            if(m_messages.contains(id)){
                applyReplyInfo(&m_messages[id],object.value("interaction_info").toMap());
                const QVariantList reactions=object.value("interaction_info").toMap().value("reactions").toMap().value("reactions").toList();QStringList labels;
                foreach(const QVariant &value,reactions){const QVariantMap r=value.toMap(),kind=r.value("type").toMap();if(kind.value("@type")=="reactionTypeEmoji")labels<<kind.value("emoji").toString()+" "+QString::number(r.value("total_count").toInt());}
                m_messages[id].insert("reactions",reactions);m_messages[id].insert("reactionDisplay",labels.join("  "));
                if(m_reactionState.value("messageId").toLongLong()==id){
                    QVariantList options=m_reactionState.value("options").toList();for(int i=0;i<options.size();++i){QVariantMap option=options.at(i).toMap();bool chosen=false;foreach(const QVariant &value,reactions){const QVariantMap reaction=value.toMap();if(reaction.value("type").toMap().value("emoji")==option.value("emoji"))chosen=reaction.value("is_chosen").toBool();}option.insert("chosen",chosen);options[i]=option;}m_reactionState.insert("options",options);emit changed();
                }
            }
        } else if(type=="updateMessageIsPinned") {
            refreshPinned();
        } else if (type == "updateMessageEdited") {
            m_overlays[id].insert("edited", object.value("edit_date").toInt() > 0);
            if (m_messages.contains(id)) m_messages[id].insert("edited", object.value("edit_date").toInt() > 0);
        } else if (type == "updateMessageSendAcknowledged") {
            m_overlays[id].insert("acknowledged", true);
            if (m_messages.contains(id)) m_messages[id].insert("acknowledged", true);
        }
        else if (type == "updateDeleteMessages") {
            QVariantList ids = object.value("message_ids").toList();
            foreach(const QVariant &value,ids)if(value.toLongLong()==m_pinnedMessage.value("messageId").toLongLong()){m_pinnedMessage.clear();refreshPinned();break;}
            for (int i = 0; i < ids.size(); ++i) {
                qlonglong removed = ids.at(i).toLongLong();
                m_messages.remove(removed);
                m_selected.remove(removed);
                m_removed.insert(removed);
            }
        }
        rebuildMessages();
    }
}

void TelegramStore::expireRequests()
{
    retrySlowHistory();
    const qlonglong now = m_clock.elapsed();
    if (now >= m_nextCleanup && !m_account.isEmpty()) { m_nextCleanup = now + 60000; m_local.cleanup(); }
    bool expiredAny = false;
    QMap<QString, QVariantMap>::iterator it = m_requests.begin();
    while (it != m_requests.end()) {
        QVariantMap &pending = it.value();
        if (pending.value("expired").toBool() || pending.value("deadline").toLongLong() > now) { ++it; continue; }
        expiredAny = true;
        ++m_timedOutRequests;
        const QString kind = pending.value("kind").toString();
        if((kind=="download" || kind=="download-cancel") && m_downloads.contains(pending.value("fileId").toInt()) && pending.value("fileSerial")!=m_downloads.value(pending.value("fileId").toInt()).value("serial")){
            it=m_requests.erase(it);continue;
        }
        const bool current = pending.value("chatId") == m_current && pending.value("generation").toUInt() == m_generation;
        if (kind == "send" || kind=="edit" || kind=="forward") {
            pending.insert("expired", true); // Keep correlation for a late answer; never resend automatically.
            m_local.transition(pending.value("recordId").toString(), "uncertain");
            foreach(const QString &record,pending.value("recordIds").toStringList())m_local.transition(record,"uncertain");
            if (m_outbox.contains(it.key())) m_outbox[it.key()].insert("delivery", ru("Результат отправки неизвестен"));
            if (pending.value("chatId") == m_current) m_error = ru("Нет ответа об отправке. Проверьте историю перед повтором.");
            if(kind=="forward") {m_forward.insert("state","uncertain");m_forward.insert("status",ru("Нет ответа. Проверьте чат получателя перед повтором."));}
            if(kind=="edit")m_error=ru("Нет ответа об изменении. Проверьте сообщение перед повтором.");
            ++it;
        } else {
            if (kind == "chats") { m_loadingChats = false; m_error = ru("Загрузка чатов не ответила вовремя. Можно повторить."); }
            if (kind == "history" && current) {
                m_historyRequests.remove(it.key());
                if (m_historyRequests.isEmpty()) {
                    m_historyRetryAt = 0;
                    m_historyRetryCall.clear();
                    m_loadingHistory = false;
                    m_error = ru("Загрузка истории не ответила вовремя. Можно повторить.");
                }
            }
            if (kind == "read" && current) m_lastReadSubmitted=0;
            if (kind == "draft") m_draftInFlight.remove(pending.value("draftKey",pending.value("chatId")).toString());
            if(kind.startsWith("comments-") && current){m_pendingComment.clear();m_error=ru("Комментарии не ответили вовремя. Повторите.");}
            if(kind=="storage")m_storageSummary=ru("Подсчёт файлов не ответил. Повторите.");
            if (kind == "download") m_downloading.remove(pending.value("fileId").toInt());
            if(kind=="download"){const int id=pending.value("fileId").toInt();m_downloads[id].insert("active",false);m_downloads[id].insert("status",ru("Нет ответа. Можно повторить."));emit downloadsChanged();}
            if(kind=="download-cancel"){const int id=pending.value("fileId").toInt();if(m_downloads.contains(id)){m_downloads[id].insert("cancelPending",false);m_downloads[id].insert("status",ru("Нет ответа об отмене. Проверьте файл."));emit downloadsChanged();}}
            if(kind.startsWith("scheduled-") && pending.value("scheduledSerial").toUInt()==m_scheduledSerial){m_scheduledBusy=false;m_scheduledStatus=ru("Нет ответа. Обновите список, чтобы проверить результат.");emit scheduledChanged();}
            if(kind.startsWith("info") && pending.value("infoSerial").toUInt()==m_infoSerial) {m_infoLoading=false;m_infoError=ru("Сведения не ответили вовремя. Можно повторить.");emit infoChanged();}
            if(kind=="contacts"){m_contactsLoading=false;m_contactsError=ru("Контакты не ответили вовремя. Можно повторить.");emit contactsChanged();}
            if(kind=="contact-open" && pending.value("contactSerial").toUInt()==m_contactSerial){m_openingContact=false;m_contactsError=ru("Не удалось открыть диалог. Повторите.");emit contactsChanged();}
            if(kind.startsWith("reaction-") && current && pending.value("reactionSerial").toUInt()==m_reactionSerial){m_reactionState.insert("state","error");m_reactionState.insert("peopleLoading",false);m_reactionState.insert("status",ru("Реакции не ответили вовремя. Откройте их снова."));}
            if((kind=="selection-check" || kind=="delete") && current){++m_actionSerial;m_selectionState.clear();m_error=ru("Нет ответа о действии. Проверьте чат перед повтором.");}
            if(kind=="message-action" && current)m_error=ru("Не удалось проверить действия с сообщением. Повторите.");
            it = m_requests.erase(it);
        }
    }
    if (expiredAny) {
        rebuildMessages();
        emit changed();
    }
}

void TelegramStore::setNetwork(const QString &type, const QString &identity)
{
    if (type != "networkTypeNone" && type != "networkTypeWiFi" &&
        type != "networkTypeMobile" && type != "networkTypeOther") return;
    m_networkType = type;
    m_networkIdentity = identity;
    if (type == "networkTypeNone") m_online = false;
    sendNetworkType();
}

void TelegramStore::sendNetworkType(bool force)
{
    if (!m_ready || m_networkType.isEmpty()) return;
    const QString signature = m_networkType + ":" + m_networkIdentity;
    if (!force && m_sentNetwork == signature) return;
    QVariantMap call = method("setNetworkType");
    call.insert("type", method(m_networkType.toLatin1().constData()));
    const QString request = submit(call, "network");
    if (!request.isEmpty()) { m_sentNetwork = signature; m_requests[request].insert("network", signature); }
}

void TelegramStore::resumeHistoryRequests()
{
    const bool retryHistory = m_loadingHistory && !m_current.isEmpty();
    retireHistoryRequests();
    bool retryChats = m_loadingChats, archive = m_archiveView;
    int chatLimit = 40;
    QMap<QString, QVariantMap>::iterator it = m_requests.begin();
    while (it != m_requests.end()) {
        const QVariantMap pending = it.value();
        const QString kind = pending.value("kind").toString();
        if (kind == "chats") {
            retryChats = true;
            archive = pending.value("archive").toBool();
            chatLimit = pending.value("limit", 40).toInt();
            // Retire only read requests. Late replies must not complete a new load.
            it = m_requests.erase(it);
        } else ++it;
    }
    if (retryHistory) { m_loadingHistory = false; history(m_messages.isEmpty()); }
    if (retryChats) { m_loadingChats = false; loadChats(archive, chatLimit); }
}

void TelegramStore::setForeground(bool value)
{
    if (m_foreground == value) return;
    m_foreground = value;
    if (value) m_peopleTimer.start(1000);
    else m_peopleTimer.stop();
    m_expiry.setInterval(value ? qMin(1000, m_requestTimeoutMs) : qMin(5000, m_requestTimeoutMs));
    if (m_ready) {
        QVariantMap call = method("setOption"), option = method("optionValueBoolean");
        option.insert("value", value); call.insert("name", "online"); call.insert("value", option);
        submit(call, "online");
        if (value) {
            sendNetworkType(true);
            resumeHistoryRequests();
        } else flushDrafts();
    }
    if (value) { expireRequests(); peopleTick(); markRead(); }
    emit changed();
}
