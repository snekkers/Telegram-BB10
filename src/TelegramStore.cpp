#include "TelegramStore.hpp"
#include "MessagePresentation.hpp"
#include "Presence.hpp"
#include "TelegramSession.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QSize>
#include <QDir>
#include <QUrl>
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
QString draftValue(const QVariant &draft)
{
    return draft.toMap().value("content").toMap().value("text").toMap().value("text").toString();
}
}

TelegramStore::TelegramStore(TelegramSession *session, QObject *parent, int requestTimeoutMs)
    : QObject(parent), m_session(session), m_local(session->dataDirectory()), m_nextCleanup(0), m_requestTimeoutMs(qMax(100, requestTimeoutMs)), m_contactsLoading(false),m_openingContact(false),m_contactSection(0),m_contactSerial(0),m_nextPresenceRefresh(0), m_infoSection(0), m_memberOffset(0), m_actionSerial(0), m_infoSerial(0), m_historyCursor(0), m_generation(0),
      m_ready(false), m_loadingChats(false), m_loadingHistory(false),
      m_chatsEnd(false), m_historyEnd(false), m_online(true), m_archiveView(false), m_archiveEnd(false), m_infoLoading(false), m_infoEnd(true)
{
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
    suspendLocalSends(); m_account.clear(); m_localMessages.clear();
    ++m_generation;
    m_chats.clear(); m_users.clear(); m_requests.clear(); m_outbox.clear();
    m_messages.clear(); m_overlays.clear(); m_historyTouched.clear(); m_removed.clear();
    m_failedDrafts.clear(); m_recoveredTexts.clear(); m_current.clear(); m_error.clear();
    m_draftTimer.stop(); m_drafts.clear(); m_dirtyDrafts.clear(); m_draftInFlight.clear();
    m_files.clear(); m_downloading.clear();
    m_contactRows.clear();m_contactIds.clear();m_contactsError.clear();m_contactsLoading=m_openingContact=false;++m_contactSerial;m_pendingContactChat.clear();
    m_avatarFiles.clear();m_avatarQueue.clear();m_avatarQueued.clear();m_avatarActive.clear();m_avatarFailed.clear();m_chatActions.clear();
    emit contactsChanged();emit avatarChanged(QString());
    m_chatRows.clear(); m_messageRows.clear();
    m_failedAttachments.clear(); m_sentAttachments.clear(); m_earlyAttachmentOutcomes.clear();m_earlyForwards.clear();
    m_historyCursor = 0; m_targetMessage.clear();
    m_archiveEnd=false;m_composerContext.clear();m_forward.clear();m_info.clear();m_infoRows.clear();m_members.clear();m_shared.clear();
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
        submit(method("getMe"), "me");
    }
    emit changed();
}

QString TelegramStore::submit(QVariantMap object, const QString &kind, const QString &chatId)
{
    if (!m_ready)
        return QString();
    if (m_requests.size() >= 64) {
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
    pending.insert("generation", m_generation);
    pending.insert("deadline", m_clock.elapsed() + m_requestTimeoutMs);
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

void TelegramStore::loadChats(bool archive)
{
    QVariantMap call = method("loadChats");
    call.insert("chat_list", method(archive ? "chatListArchive" : "chatListMain"));
    call.insert("limit", 40);
    const QString request=submit(call,"chats");
    if(request.isEmpty())return;
    m_requests[request].insert("archive",archive);
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
        if ((it.value().value("kind") == "send" || it.value().value("kind") == "edit") && it.value().value("chatId") == m_current &&
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
    return ru("Сообщение Telegram");
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
    return m_local.latest(m_account, m_current, "attachment", QStringList() << "draft");
}

QVariantMap TelegramStore::failedAttachment() const
{
    const QVariantMap result = m_local.latest(m_account, m_current, "attachment", QStringList() << "failed" << "uncertain");
    return result.isEmpty() ? m_failedAttachments.value(m_current) : result;
}

QString TelegramStore::failedDraft() const
{
    const QVariantMap item = m_local.latest(m_account, m_current, "text", QStringList() << "failed" << "uncertain");
    return item.isEmpty() || m_recoveredTexts.contains(item.value("recordId").toString()) ? m_failedDrafts.value(m_current) : item.value("text").toString();
}

QVariantMap TelegramStore::uploadRecord(const QString &id) const
{
    QVariantMap item = m_local.entry(id);
    if (m_account.isEmpty() || item.value("accountId") != m_account || item.value("chatId") != m_current || item.value("type") != "attachment") return QVariantMap();
    item.insert("recordId", id); item.insert("path", m_local.path(item)); return item;
}

bool TelegramStore::stageAttachment(const QString &path, const QString &kind, const QString &caption, int duration)
{
    QFileInfo file(path);
    if (!m_ready || m_account.isEmpty() || m_current.isEmpty() || caption.size() > 1024 || duration < 0 || duration > 300 ||
        (kind != "photo" && kind != "document" && kind != "voice") || !file.isFile() || file.isSymLink() ||
        file.size() <= 0 || file.size() > 50 * 1024 * 1024) return false;
    QVariantMap item; item.insert("type", "attachment"); item.insert("path", path); item.insert("kind", kind);
    item.insert("caption", caption); item.insert("duration", duration);
    const QString id = m_local.prepare(m_account, m_current, item);
    if (id.isEmpty()) { m_error = m_local.error(); emit changed(); return false; }
    setDraft(caption); emit changed(); return true;
}

bool TelegramStore::clearAttachmentDraft()
{
    const bool result = m_local.removeDraft(m_account, m_current);
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

int TelegramStore::cleanupUploads() { const int count = m_local.cleanup(); emit changed(); return count; }

void TelegramStore::restoreChatDraft(const QString &id, const QVariant &remote)
{
    if (m_dirtyDrafts.contains(id)) return;
    const QVariant local = m_local.draft(m_account, id);
    const QVariantMap attachment = m_local.latest(m_account, id, "attachment", QStringList() << "draft");
    const QVariantMap text = m_local.latest(m_account, id, "text", QStringList() << "draft");
    if (local.isValid()) { m_drafts.insert(id, local.toString()); m_dirtyDrafts.insert(id, local.toString()); m_draftTimer.start(600); }
    else if (!attachment.isEmpty()) m_drafts.insert(id, attachment.value("caption").toString());
    else if (!text.isEmpty()) m_drafts.insert(id, text.value("text").toString());
    else m_drafts.insert(id, draftValue(remote));
}

void TelegramStore::recordSendResult(const QVariantMap &message, const QString &recordId, const QString &outcome)
{
    if (recordId.isEmpty()) return;
    const QString sending = message.value("sending_state").toMap().value("@type").toString();
    const QString state = outcome == "failed" || sending == "messageSendingStateFailed" ? "failed" :
        outcome == "succeeded" || sending.isEmpty() ? "sent" : "pending";
    if (!m_local.transition(recordId, state, idString(message.value("id")))) m_error = m_local.error();
    if(m_forward.value("recordId")==recordId) {
        m_forward.insert("state",state);m_forward.insert("status",state=="sent"?ru("Сообщение переслано."):state=="failed"?ru("Пересылка отклонена."):ru("Передано Telegram. Ожидание подтверждения."));
    }
    if (state == "pending") m_localMessages.insert(idString(message.value("chat_id")) + ":" + idString(message.value("id")), recordId);
}

bool TelegramStore::sendAttachment(const QString &path, const QString &kind, const QString &caption, int duration, int width, int height)
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
        (caption.isEmpty() ? QString() : "\n" + caption), attachment);
}

bool TelegramStore::openChat(const QString &id)
{
    return openChatAt(id, QString());
}

bool TelegramStore::openChatAt(const QString &id, const QString &messageId)
{
    if (!m_ready || !m_chats.contains(id))
        return false;
    const QVariantMap reply=m_current==id && m_composerContext.value("mode")=="reply"?m_composerContext:QVariantMap();
    closeChat();
    m_current = id;m_composerContext=reply;
    m_targetMessage = messageId.toLongLong() > 0 ? QString::number(messageId.toLongLong()) : QString();
    QVariantMap call = method("openChat");
    call.insert("chat_id", number(id));
    submit(call, "open", id);
    history(true);
    emit changed();
    return true;
}

void TelegramStore::closeChat()
{
    flushDrafts();
    if (m_ready && !m_current.isEmpty()) {
        QVariantMap call = method("closeChat");
        call.insert("chat_id", number(m_current));
        submit(call, "close", m_current);
    }
    ++m_generation;
    m_current.clear();
    m_composerContext.clear();m_forward.clear();++m_actionSerial;++m_infoSerial;
    m_infoLoading=false;m_infoEnd=true;m_infoRows.clear();m_infoError.clear();emit infoChanged();
    m_targetMessage.clear();
    m_messages.clear(); m_overlays.clear(); m_historyTouched.clear(); m_removed.clear();
    m_historyCursor = 0;
    m_loadingHistory = m_historyEnd = false;
    m_error.clear();
    rebuildMessages();
    emit changed();
}

void TelegramStore::history(bool initial)
{
    if (!canLoadHistory())
        return;
    initial = initial || m_historyCursor == 0;
    QVariantMap call = method("getChatHistory");
    call.insert("chat_id", number(m_current));
    call.insert("from_message_id", QVariant::fromValue(initial ? m_targetMessage.toLongLong() : m_historyCursor));
    call.insert("offset", initial && !m_targetMessage.isEmpty() ? -20 : 0);
    call.insert("limit", 50);
    call.insert("only_local", !m_online);
    QString id = submit(call, "history", m_current);
    if (id.isEmpty())
        return;
    m_requests[id].insert("initial", initial);
    m_historyTouched.clear();
    m_loadingHistory = true;
    m_error.clear();
    emit changed();
}

void TelegramStore::loadMoreHistory() { history(false); }

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
    row.insert("chatId", idString(message.value("chat_id")));
    row.insert("text", messageText(message.value("content").toMap()));
    row.insert("media", mediaInfo(message.value("content").toMap()));
    row.insert("outgoing", message.value("is_outgoing").toBool());
    row.insert("sendingState", message.value("sending_state").toMap().value("@type").toString());
    row.insert("senderId", idString(message.value("sender_id").toMap().value("user_id")));
    row.insert("senderChatId", idString(message.value("sender_id").toMap().value("chat_id")));
    row.insert("date", message.value("date"));
    row.insert("edited", message.value("edit_date").toInt() > 0);
    const QVariantMap content=message.value("content").toMap();
    row.insert("textMessage",content.value("@type")=="messageText");
    row.insert("formattedText",content.value(content.value("@type")=="messageText"?"text":"caption"));
    const QVariantMap reply=message.value("reply_to").toMap();
    row.insert("replyMessageId",idString(reply.value("message_id")));
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
    QVariantMap call = method("downloadFile");
    call.insert("file_id", id); call.insert("priority", 16); call.insert("offset", 0);
    call.insert("limit", 50 * 1024 * 1024); call.insert("synchronous", false);
    const QString request = submit(call, "download", m_current);
    if (request.isEmpty()) return false;
    m_requests[request].insert("fileId", id); m_downloading.insert(id);
    return true;
}

void TelegramStore::upsertMessage(const QVariantMap &message)
{
    qlonglong id = message.value("id").toLongLong();
    if (!id || idString(message.value("chat_id")) != m_current || m_removed.contains(id))
        return;
    QVariantMap row = messageRow(message);
    const QVariantMap previous = m_messages.value(id);
    if (!previous.isEmpty() && previous.value("sendingState") != "messageSendingStatePending" &&
        row.value("sendingState") == "messageSendingStatePending")
        return; // A stale send response must not undo a terminal update.
    QVariantMap overlay = m_overlays.value(id);
    if (overlay.contains("content")) {
        row.insert("text", messageText(overlay.value("content").toMap()));
        row.insert("media", mediaInfo(overlay.value("content").toMap()));
        const QVariantMap content=overlay.value("content").toMap();
        row.insert("formattedText",content.value(content.value("@type")=="messageText"?"text":"caption"));
        row.insert("textMessage",content.value("@type")=="messageText");
    }
    if (overlay.contains("edited")) row.insert("edited", overlay.value("edited"));
    if (overlay.value("acknowledged").toBool()) row.insert("acknowledged", true);
    if (m_messages.value(id).value("acknowledged").toBool()) row.insert("acknowledged", true);
    m_messages.insert(id, row);
    if (row.value("sendingState") == "messageSendingStateFailed" &&
        message.value("content").toMap().value("@type") == "messageText")
        m_failedDrafts.insert(m_current, row.value("text").toString());
}

void TelegramStore::rebuildMessages()
{
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
        row.insert("author", author);
        row.insert("delivery", delivery(row));
        row.insert("searchTarget", row.value("messageId").toString() == m_targetMessage && !m_targetMessage.isEmpty());
        rows.append(row);
    }
    QList<QVariantMap> local;
    QMap<QString, QVariantMap>::const_iterator outgoing;
    for (outgoing = m_outbox.constBegin(); outgoing != m_outbox.constEnd(); ++outgoing) {
        if (outgoing.value().value("chatId") == m_current)
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

bool TelegramStore::sendText(const QString &text)
{
    if(m_composerContext.value("mode")=="edit")return editText(text);
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
    return sendContent(content, text);
}

bool TelegramStore::sendContent(const QVariantMap &content, const QString &text, const QVariantMap &attachment)
{
    if (m_outbox.size() >= 64) return false;
    QVariantMap call = method("sendMessage"); call.insert("chat_id", number(m_current));
    QVariantMap options = method("messageSendOptions"); options.insert("paid_message_star_count", 0); call.insert("options", options);
    call.insert("input_message_content", content);
    if(m_composerContext.value("mode")=="reply")call.insert("reply_to",m_composerContext.value("replyTo"));
    QVariantMap intent = attachment;
    intent.insert("type", attachment.isEmpty() ? "text" : "attachment");
    if (attachment.isEmpty()) intent.insert("text", text);
    if(call.contains("reply_to"))intent.insert("replyTo",call.value("reply_to"));
    const QString recordId = m_local.prepare(m_account, m_current, intent);
    if (recordId.isEmpty() || !m_local.transition(recordId, "submitting")) {
        m_error = m_local.error(); emit changed(); return false;
    }
    const QString id = submit(call, "send", m_current);
    if (id.isEmpty()) { m_local.transition(recordId, "uncertain"); emit changed(); return false; }
    m_requests[id].insert("recordId", recordId);
    QVariantMap row; row.insert("recordId", recordId);
    row.insert("messageKey", "request:" + id);
    row.insert("sequence", id.section('.', -1).toUInt());
    row.insert("chatId", m_current);
    row.insert("author", ru("Вы"));
    row.insert("text", text);
    row.insert("outgoing", true);
    row.insert("date", QDateTime::currentDateTime().toTime_t());
    row.insert("time", QDateTime::currentDateTime().toString("dd.MM HH:mm"));
    row.insert("delivery", ru("Ожидание ответа движка…"));
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
    emit scrollRequested();
    return true;
}

QString TelegramStore::takeFailedDraft()
{
    const QString text = failedDraft();
    const QVariantMap item = m_local.latest(m_account, m_current, "text", QStringList() << "failed" << "uncertain");
    if (!item.isEmpty()) m_recoveredTexts.insert(item.value("recordId").toString());
    const QVariantMap reply=item.value("replyTo").toMap();
    if(!reply.isEmpty()) {m_composerContext.clear();m_composerContext.insert("mode","reply");m_composerContext.insert("messageId",idString(reply.value("message_id")));m_composerContext.insert("replyTo",reply);m_composerContext.insert("preview",reply.value("quote").toMap().value("text").toMap().value("text"));}
    m_failedDrafts.remove(m_current); setDraft(text); flushDrafts(); emit changed(); return text;
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
    if (!m_ready || m_current.isEmpty() || m_messages.isEmpty()) return false;
    qlonglong latest = 0;
    for (QMap<qlonglong, QVariantMap>::const_iterator it = m_messages.constBegin(); it != m_messages.constEnd(); ++it)
        if (it.value().value("sendingState").toString().isEmpty() && it.key() > latest) latest = it.key();
    if (!latest) return false;
    QVariantMap call = method("viewMessages"); call.insert("chat_id", number(m_current));
    call.insert("message_ids", QVariantList() << QVariant::fromValue(latest));
    call.insert("source", method("messageSourceChatHistory")); call.insert("force_read", true);
    return !submit(call, "read", m_current).isEmpty();
}

void TelegramStore::setDraft(const QString &text)
{
    if(m_composerContext.value("mode")=="edit")return;
    if (!m_ready || m_current.isEmpty() || text.size() > 4096 || m_drafts.value(m_current) == text) return;
    m_drafts.insert(m_current, text);
    m_dirtyDrafts.insert(m_current, text);
    m_draftTimer.start(600);
    emit changed();
}

void TelegramStore::flushDrafts()
{
    foreach (const QString &chat, m_dirtyDrafts.keys())
        if (!m_local.rememberDraft(m_account, chat, m_dirtyDrafts.value(chat))) m_error = m_local.error();
    if (!m_ready) return;
    const QStringList ids = m_dirtyDrafts.keys();
    foreach (const QString &chat, ids) {
        if (m_draftInFlight.contains(chat)) continue;
        QVariantMap call = method("setChatDraftMessage");
        call.insert("chat_id", number(chat));
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
            m_requests[id].insert("text", text);
            m_draftInFlight.insert(chat, id);
        }
    }
}

void TelegramStore::response(const QVariantMap &object, const QVariantMap &pending)
{
    if(peopleResponse(object,pending))return;
    if(actionResponse(object,pending))return;
    const QString type = object.value("@type").toString();
    const QString kind = pending.value("kind").toString();
    const QString chat = pending.value("chatId").toString();
    const bool current = chat == m_current && pending.value("generation").toUInt() == m_generation;
    const bool error = type == "error";
    if (kind == "me") {
        if (!error && type == "user" && object.value("id").toLongLong() > 0) {
            m_account = idString(object.value("id"));
            emit avatarChanged(QString());
            if (!m_local.open()) m_error = m_local.error();
            cleanupUploads();
            foreach (const QString &existingChat, m_chats.keys()) {
                if (m_local.draft(m_account, existingChat).isValid() ||
                    !m_local.latest(m_account, existingChat, "attachment", QStringList() << "draft").isEmpty())
                    restoreChatDraft(existingChat, QVariant());
            }
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
            loadMoreChats();
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
            if (oldest) m_historyCursor = oldest;
            rebuildMessages();
            if (pending.value("initial").toBool()) { markRead(); emit scrollRequested(); }
        }
    } else if (kind == "download") {
        if (error) {
            m_downloading.remove(pending.value("fileId").toInt());
            if (chat == m_current) m_error = ru("Вложение не загрузилось (код %1).").arg(object.value("code").toInt());
        } else if (type == "file") updateFile(object);
    } else if (kind == "draft") {
        m_draftInFlight.remove(chat);
        if (!error && m_dirtyDrafts.value(chat) == pending.value("text").toString()) {
            m_local.acknowledgeDraft(m_account, chat, pending.value("text").toString()); m_dirtyDrafts.remove(chat);
        }
        else if (error) {
            if (chat == m_current) m_error = ru("Черновик пока не сохранён. Он остаётся в памяти приложения.");
        } else m_draftTimer.start(150);
    } else if (kind == "send") {
        const QString extra = object.value("@extra").toString();
        const QString text = m_outbox.value(extra).value("text").toString();
        if (error) {
            m_local.transition(pending.value("recordId").toString(), "failed");
            if (m_outbox.contains(extra)) m_outbox[extra].insert("delivery", ru("Не отправлено"));
            const QVariantMap attachment = m_outbox.value(extra).value("attachment").toMap();
            if (attachment.isEmpty()) m_failedDrafts.insert(chat, text);
            else m_failedAttachments.insert(chat, attachment);
            if (chat == m_current) m_error = ru("Отправка отклонена (код %1). Вложение или текст можно вернуть в поле ввода.").arg(object.value("code").toInt());
        } else if (type == "message") {
            const QVariantMap attachment = m_outbox.value(extra).value("attachment").toMap();
            if (!attachment.isEmpty()) {
                const QString key = chat + ":" + idString(object.value("id"));
                const QString outcome = m_earlyAttachmentOutcomes.value(key);
                const bool failed = outcome == "failed" || (chat == m_current &&
                    m_messages.value(object.value("id").toLongLong()).value("sendingState") == "messageSendingStateFailed");
                if (failed) m_failedAttachments.insert(chat, attachment);
                else {
                    if (outcome != "succeeded") m_sentAttachments.insert(key, attachment);
                    m_failedAttachments.remove(chat);
                }
            }
            recordSendResult(object, pending.value("recordId").toString(),
                m_earlyAttachmentOutcomes.take(chat + ":" + idString(object.value("id"))));
            m_outbox.remove(extra);
            upsertMessage(object);
            bool uncertain = false;
            QMap<QString, QVariantMap>::const_iterator it;
            for (it = m_outbox.constBegin(); it != m_outbox.constEnd(); ++it)
                if (it.value().value("chatId") == m_current && it.value().value("delivery") == ru("Результат отправки неизвестен"))
                    uncertain = true;
            if (!uncertain && m_error == ru("Нет ответа об отправке. Проверьте историю перед повтором."))
                m_error.clear();
        }
        rebuildMessages();
    } else if (kind == "read" && current && error)
        m_error = ru("Не удалось отметить переписку прочитанной.");
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
        rememberAvatar("chat:"+id,source.value("photo").toMap());
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
        rebuildChats();
        emit recipientChatsChanged();
        emit changed();
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
        emit recipientChatsChanged();
        if(m_contactSection==1)rebuildContacts();
        if (chatId == m_current) rebuildMessages();
        emit changed();
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
        if (type == "updateMessageSendFailed" || type == "updateMessageSendSucceeded") {
            const QString recordId = m_localMessages.take(attachmentKey);
            recordSendResult(message, recordId, type == "updateMessageSendFailed" ? "failed" : "succeeded");
        }
        if (type == "updateMessageSendFailed" && m_sentAttachments.contains(attachmentKey))
            m_failedAttachments.insert(sendChat, m_sentAttachments.take(attachmentKey));
        if (type == "updateMessageSendSucceeded") m_sentAttachments.remove(attachmentKey);
        if (idString(message.value("chat_id")) == m_current) {
            qlonglong id = message.value("id").toLongLong();
            if (type != "updateNewMessage") {
                qlonglong old = object.value("old_message_id").toLongLong();
                m_messages.remove(old);
                if (old != id) m_removed.insert(old);
            }
            const bool incoming=type=="updateNewMessage" && !message.value("is_outgoing").toBool() && !m_messages.contains(id) && !m_removed.contains(id);
            m_historyTouched.insert(id);
            upsertMessage(message);
            if (type == "updateMessageSendFailed") {
                if (message.value("content").toMap().value("@type") == "messageText")
                    m_failedDrafts.insert(m_current, messageText(message.value("content").toMap()));
                m_error = ru("Сообщение не отправлено. Можно восстановить текст или вложение.");
            }
            rebuildMessages();
            if(incoming)emit incomingMessage();
            emit changed();
        }
    } else if (chatId == m_current && !m_current.isEmpty()) {
        qlonglong id = object.value("message_id").toLongLong();
        if (type == "updateMessageContent") {
            m_overlays[id].insert("content", object.value("new_content"));
            if (m_messages.contains(id)) {
                m_messages[id].insert("text", messageText(object.value("new_content").toMap()));
                m_messages[id].insert("media", mediaInfo(object.value("new_content").toMap()));
                const QVariantMap content=object.value("new_content").toMap();
                m_messages[id].insert("formattedText",content.value(content.value("@type")=="messageText"?"text":"caption"));
                m_messages[id].insert("textMessage",content.value("@type")=="messageText");
            }
        } else if (type == "updateMessageEdited") {
            m_overlays[id].insert("edited", object.value("edit_date").toInt() > 0);
            if (m_messages.contains(id)) m_messages[id].insert("edited", object.value("edit_date").toInt() > 0);
        } else if (type == "updateMessageSendAcknowledged") {
            m_overlays[id].insert("acknowledged", true);
            if (m_messages.contains(id)) m_messages[id].insert("acknowledged", true);
        }
        else if (type == "updateDeleteMessages") {
            QVariantList ids = object.value("message_ids").toList();
            for (int i = 0; i < ids.size(); ++i) {
                qlonglong removed = ids.at(i).toLongLong();
                m_messages.remove(removed);
                m_removed.insert(removed);
            }
        }
        rebuildMessages();
    }
}

void TelegramStore::expireRequests()
{
    const qlonglong now = m_clock.elapsed();
    if (now >= m_nextCleanup && !m_account.isEmpty()) { m_nextCleanup = now + 60000; m_local.cleanup(); }
    bool expiredAny = false;
    QMap<QString, QVariantMap>::iterator it = m_requests.begin();
    while (it != m_requests.end()) {
        QVariantMap &pending = it.value();
        if (pending.value("expired").toBool() || pending.value("deadline").toLongLong() > now) { ++it; continue; }
        expiredAny = true;
        const QString kind = pending.value("kind").toString();
        const bool current = pending.value("chatId") == m_current && pending.value("generation").toUInt() == m_generation;
        if (kind == "send" || kind=="edit" || kind=="forward") {
            pending.insert("expired", true); // Keep correlation for a late answer; never resend automatically.
            m_local.transition(pending.value("recordId").toString(), "uncertain");
            if (m_outbox.contains(it.key())) m_outbox[it.key()].insert("delivery", ru("Результат отправки неизвестен"));
            if (pending.value("chatId") == m_current) m_error = ru("Нет ответа об отправке. Проверьте историю перед повтором.");
            if(kind=="forward") {m_forward.insert("state","uncertain");m_forward.insert("status",ru("Нет ответа. Проверьте чат получателя перед повтором."));}
            if(kind=="edit")m_error=ru("Нет ответа об изменении. Проверьте сообщение перед повтором.");
            ++it;
        } else {
            if (kind == "chats") { m_loadingChats = false; m_error = ru("Загрузка чатов не ответила вовремя. Можно повторить."); }
            if (kind == "history" && current) { m_loadingHistory = false; m_error = ru("Загрузка истории не ответила вовремя. Можно повторить."); }
            if (kind == "draft") m_draftInFlight.remove(pending.value("chatId").toString());
            if (kind == "download") m_downloading.remove(pending.value("fileId").toInt());
            if(kind.startsWith("info") && pending.value("infoSerial").toUInt()==m_infoSerial) {m_infoLoading=false;m_infoError=ru("Сведения не ответили вовремя. Можно повторить.");emit infoChanged();}
            if(kind=="contacts"){m_contactsLoading=false;m_contactsError=ru("Контакты не ответили вовремя. Можно повторить.");emit contactsChanged();}
            if(kind=="contact-open" && pending.value("contactSerial").toUInt()==m_contactSerial){m_openingContact=false;m_contactsError=ru("Не удалось открыть диалог. Повторите.");emit contactsChanged();}
            if(kind=="message-action" && current)m_error=ru("Не удалось проверить действия с сообщением. Повторите.");
            it = m_requests.erase(it);
        }
    }
    if (expiredAny) {
        rebuildMessages();
        emit changed();
    }
}
