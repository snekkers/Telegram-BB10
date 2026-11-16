#include "LocalOutbox.hpp"
#include "PrivateFile.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QUuid>
#include <QRegExp>

namespace {
qlonglong timestamp() { return QDateTime::currentDateTime().toTime_t(); }
bool identifier(const QString &id) { return QRegExp("-?[0-9]{1,20}").exactMatch(id) && id != "0"; }
bool relativePath(const QString &path) {
    return !path.isEmpty() && !QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path &&
        !path.startsWith("../") && path != ".." && !path.contains('\\');
}
QString key(const QString &account, const QString &chat) { return account + ":" + chat; }
}

LocalOutbox::LocalOutbox(const QString &directory) : m_directory(directory), m_open(false), m_valid(false) {}

bool LocalOutbox::open()
{
    if (m_open) return m_valid;
    m_open = true;
    const QString file = QDir(m_directory).filePath("outbox.json");
    if (!QFileInfo(file).exists() && !QFileInfo(file).isSymLink()) { m_valid = true; return true; }
    m_error = QString::fromUtf8("Некорректный журнал отправки. Очистка остановлена; файлы сохранены.");
    QVariantMap value;
    if (!PrivateFile::read(file, &value, 1024 * 1024) || value.value("version").toInt() != 1 || value.value("entries").type() != QVariant::Map || value.value("drafts").type() != QVariant::Map) {
        m_error = QString::fromUtf8("Журнал отправки повреждён или недоступен. Файлы сохранены; автоматическая очистка остановлена."); return false;
    }
    m_entries = value.value("entries").toMap(); m_drafts = value.value("drafts").toMap();
    if (m_entries.size() > 128 || m_drafts.size() > 256) return false;
    const QStringList states = QStringList() << "draft" << "submitting" << "pending" << "uncertain" << "failed" << "sent";
    for (QVariantMap::const_iterator it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QVariantMap item = it.value().toMap();
        if (!QRegExp("[a-f0-9]{32}").exactMatch(it.key()) || !identifier(item.value("accountId").toString()) ||
            !identifier(item.value("chatId").toString()) || !states.contains(item.value("state").toString()) ||
            (item.value("type") != "text" && item.value("type") != "attachment" && item.value("type")!="edit" && item.value("type")!="forward") ||
            item.value("text").toString().size() > 4096 || item.value("caption").toString().size() > 1024 ||
            (item.value("type") == "attachment" && !relativePath(item.value("relativePath").toString())) ||
            ((item.value("type")=="edit" || item.value("type")=="forward") && !identifier(item.value("sourceMessageId").toString())) ||
            (item.value("type")=="forward" && !identifier(item.value("sourceChatId").toString()))) {
            m_error = QString::fromUtf8("Некорректный журнал отправки. Очистка остановлена."); return false;
        }
    }
    for (QVariantMap::const_iterator it = m_drafts.constBegin(); it != m_drafts.constEnd(); ++it)
        if (!QRegExp("[0-9]{1,20}:-?[0-9]{1,20}").exactMatch(it.key()) || it.value().toString().size() > 4096) return false;
    m_valid = true;
    QVariantMap restored = m_entries;
    for (QVariantMap::iterator it = restored.begin(); it != restored.end(); ++it) {
        QVariantMap item = it.value().toMap();
        if (item.value("state") == "pending" || item.value("state") == "submitting") {
            item.insert("state", "uncertain"); it.value() = item;
        }
    }
    if (restored != m_entries && !save(restored, m_drafts)) { m_valid = false; return false; }
    m_error.clear(); return true;
}

bool LocalOutbox::save(const QVariantMap &entries, const QVariantMap &drafts)
{
    if (!healthy()) return false;
    if (entries.size() > 128 || drafts.size() > 256) {
        m_error = QString::fromUtf8("Журнал заполнен. Проверьте незавершённые отправки и сохранённые черновики."); return false;
    }
    QVariantMap value; value.insert("version", 1); value.insert("entries", entries); value.insert("drafts", drafts);
    if (!PrivateFile::write(QDir(m_directory).filePath("outbox.json"), value, 1024 * 1024)) {
        m_error = QString::fromUtf8("Не удалось сохранить журнал отправки. Проверьте свободное место; запрос не будет повторён автоматически."); return false;
    }
    m_entries = entries; m_drafts = drafts; m_error.clear(); return true;
}

QString LocalOutbox::path(const QVariantMap &item) const
{
    const QString relative = item.value("relativePath").toString();
    if (!relativePath(relative)) return QString();
    const QFileInfo root(QDir(m_directory).filePath("uploads"));
    const QFileInfo file(QDir(root.absoluteFilePath()).filePath(relative));
    const QString boundary = root.canonicalFilePath();
    return !root.isSymLink() && !boundary.isEmpty() && file.isFile() && !file.isSymLink() &&
        file.canonicalFilePath().startsWith(boundary + "/") ? file.canonicalFilePath() : QString();
}

QVariantMap LocalOutbox::latest(const QString &account, const QString &chat, const QString &type, const QStringList &states) const
{
    QVariantMap result;
    if (!healthy() || account.isEmpty() || chat.isEmpty()) return result;
    for (QVariantMap::const_iterator it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        QVariantMap item = it.value().toMap();
        if (item.value("accountId") != account || item.value("chatId") != chat || item.value("type") != type || !states.contains(item.value("state").toString())) continue;
        if (!result.isEmpty() && item.value("sequence").toLongLong() < result.value("sequence").toLongLong()) continue;
        item.insert("recordId", it.key());
        if (type == "attachment") { item.insert("path", path(item)); item.insert("name", QFileInfo(item.value("relativePath").toString()).fileName()); }
        result = item;
    }
    return result;
}

QVariantList LocalOutbox::entries(const QString &account) const
{
    QVariantList result;
    for (QVariantMap::const_iterator it = m_entries.constBegin(); healthy() && it != m_entries.constEnd(); ++it) {
        QVariantMap item = it.value().toMap();
        if (item.value("accountId") == account) { item.insert("recordId", it.key()); result.append(item); }
    }
    return result;
}

QString LocalOutbox::prepare(const QString &account, const QString &chat, const QVariantMap &values)
{
    if (!healthy() || !identifier(account) || !identifier(chat)) return QString();
    QVariantMap item = values;
    item.remove("path"); item.remove("recordId"); item.remove("uploadId");
    if (item.value("type") == "attachment") {
        const QString root = QDir(QDir(m_directory).filePath("uploads")).canonicalPath();
        const QString source = QFileInfo(values.value("path").toString()).canonicalFilePath();
        if (root.isEmpty() || !source.startsWith(root + "/")) return QString();
        item.insert("relativePath", QDir(root).relativeFilePath(source));
        if (path(item).isEmpty()) return QString();
    }
    const QVariantMap oldDraft = latest(account, chat, item.value("type").toString(), QStringList() << "draft");
    QString id = oldDraft.value("recordId").toString();
    if (id.isEmpty()) id = QUuid::createUuid().toString().remove('{').remove('}').remove('-');
    qlonglong sequence = 0;
    for (QVariantMap::const_iterator it = m_entries.constBegin(); it != m_entries.constEnd(); ++it)
        sequence = qMax(sequence, it.value().toMap().value("sequence").toLongLong());
    item.insert("accountId", account); item.insert("chatId", chat); item.insert("state", "draft");
    item.insert("updated", timestamp()); item.insert("sequence", sequence + 1);
    QVariantMap changed = m_entries;
    // A definite refusal is safe to replace. Uncertain attempts retain their own identity.
    foreach (const QString &oldId, changed.keys()) {
        const QVariantMap previous = changed.value(oldId).toMap();
        if (previous.value("state") == "failed" && previous.value("accountId") == account && previous.value("chatId") == chat &&
            previous.value("type") == item.value("type") &&
            (item.value("type") == "attachment" ? previous.value("relativePath") == item.value("relativePath") : previous.value("text") == item.value("text")))
            changed.remove(oldId);
    }
    changed.insert(id, item);
    if (!save(changed, m_drafts)) return QString();
    if (oldDraft.value("relativePath") != item.value("relativePath")) removePayload(oldDraft.value("relativePath").toString());
    return id;
}

bool LocalOutbox::transition(const QString &id, const QString &state, const QString &messageId)
{
    QVariantMap item = entry(id);
    if (item.isEmpty()) return false;
    item.insert("state", state); item.insert("updated", timestamp());
    if (!messageId.isEmpty()) item.insert("messageId", messageId);
    QVariantMap changed = m_entries;
    if (state == "sent" && item.value("type") == "text") changed.remove(id);
    else changed.insert(id, item);
    return save(changed, m_drafts);
}

bool LocalOutbox::rememberDraft(const QString &account, const QString &chat, const QString &text)
{
    if (!healthy() || account.isEmpty() || chat.isEmpty() || text.size() > 4096) return false;
    QVariantMap changed = m_drafts; changed.insert(key(account, chat), text);
    QVariantMap entries = m_entries;
    const QVariantMap attachment = latest(account, chat, "attachment", QStringList() << "draft");
    if (!attachment.isEmpty() && text.size() <= 1024) {
        QVariantMap item = entry(attachment.value("recordId").toString()); item.insert("caption", text);
        entries.insert(attachment.value("recordId").toString(), item);
    }
    if (changed == m_drafts && entries == m_entries) return true;
    return save(entries, changed);
}

bool LocalOutbox::acknowledgeDraft(const QString &account, const QString &chat, const QString &text)
{
    if (draft(account, chat).isValid() && draft(account, chat).toString() == text) {
        QVariantMap changed = m_drafts; changed.remove(key(account, chat)); return save(m_entries, changed);
    }
    return true;
}

QVariant LocalOutbox::draft(const QString &account, const QString &chat) const { return m_drafts.value(key(account, chat)); }

bool LocalOutbox::referenced(const QString &relative) const
{
    for (QVariantMap::const_iterator it = m_entries.constBegin(); it != m_entries.constEnd(); ++it)
        if (it.value().toMap().value("relativePath") == relative) return true;
    return false;
}

void LocalOutbox::removePayload(const QString &relative)
{
    if (!relativePath(relative) || referenced(relative)) return;
    QVariantMap item; item.insert("relativePath", relative); const QString file = path(item);
    if (!file.isEmpty() && QFile::remove(file)) QDir().rmdir(QFileInfo(file).absolutePath());
}

bool LocalOutbox::removeDraft(const QString &account, const QString &chat)
{
    const QVariantMap item = latest(account, chat, "attachment", QStringList() << "draft");
    if (item.isEmpty()) return true;
    QVariantMap changed = m_entries; changed.remove(item.value("recordId").toString());
    if (!save(changed, m_drafts)) return false;
    removePayload(item.value("relativePath").toString()); return true;
}

int LocalOutbox::cleanup(qlonglong now, int minimumAgeSeconds)
{
    if (!healthy()) return 0;
    if (!now) now = timestamp();
    int removed = 0;
    const QStringList ids = m_entries.keys();
    foreach (const QString &id, ids) {
        const QVariantMap item = entry(id);
        if (item.value("state") != "sent" || now - item.value("updated").toLongLong() < qMax(86400, minimumAgeSeconds)) continue;
        const QString relative = item.value("relativePath").toString();
        bool protectedFile = false;
        foreach (const QString &other, m_entries.keys())
            if (other != id && entry(other).value("relativePath") == relative && entry(other).value("state") != "sent") protectedFile = true;
        if (protectedFile) continue;
        // Persist removal first. A crash may leave an orphan copy, never a lost active input.
        QVariantMap changed = m_entries; changed.remove(id);
        if (!save(changed, m_drafts)) break;
        const bool exists = !path(item).isEmpty(); removePayload(relative);
        if (exists && !referenced(relative) && path(item).isEmpty()) ++removed;
    }
    return removed;
}

QString LocalOutbox::summary() const
{
    if (!healthy() || !m_error.isEmpty()) return m_error;
    int protectedCount = 0, sent = 0;
    foreach (const QVariant &value, m_entries) {
        const QVariantMap item = value.toMap();
        if (item.value("type") != "attachment") continue;
        if (item.value("state") == "sent") ++sent; else ++protectedCount;
    }
    return QString::fromUtf8("Сохранено вложений: %1 · подтверждено: %2. Подтверждённые копии очищаются через сутки.").arg(protectedCount).arg(sent);
}
