#ifndef BERRYGRAM_LOCAL_OUTBOX_HPP
#define BERRYGRAM_LOCAL_OUTBOX_HPP
#include <QString>
#include <QVariantMap>

// Durable intent log. Reading it never sends a message or removes an unknown file.
class LocalOutbox {
public:
    explicit LocalOutbox(const QString &directory);
    bool open();
    bool healthy() const { return m_open && m_valid; }
    QString error() const { return m_error; }
    QVariantMap entry(const QString &id) const { return m_entries.value(id).toMap(); }
    QVariantMap latest(const QString &account, const QString &chat, const QString &type, const QStringList &states) const;
    QVariantList entries(const QString &account) const;
    QString prepare(const QString &account, const QString &chat, const QVariantMap &values);
    bool transition(const QString &id, const QString &state, const QString &messageId = QString());
    bool rememberDraft(const QString &account, const QString &chat, const QString &text);
    bool acknowledgeDraft(const QString &account, const QString &chat, const QString &text);
    QVariant draft(const QString &account, const QString &chat) const;
    bool removeDraft(const QString &account, const QString &chat);
    int cleanup(qlonglong now = 0, int minimumAgeSeconds = 86400);
    QString path(const QVariantMap &entry) const;
    QString summary() const;
private:
    bool save(const QVariantMap &entries, const QVariantMap &drafts);
    bool referenced(const QString &relative) const;
    void removePayload(const QString &relative);
    QString m_directory, m_error;
    QVariantMap m_entries, m_drafts;
    bool m_open, m_valid;
};
#endif
