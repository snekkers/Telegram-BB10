#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
#include "JsonCodec.hpp"
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <QDir>
#include <cstdio>
#include <unistd.h>

class DiagnosticBridge : public QObject {
    Q_OBJECT
public:
    DiagnosticBridge(const QString &engine, const QString &library, const QString &data)
        : m_session(engine, library, data), m_store(&m_session), m_stdin(0, QSocketNotifier::Read) {
        connect(&m_stdin, SIGNAL(activated(int)), this, SLOT(readInput()));
        connect(&m_session, SIGNAL(changed()), this, SLOT(schedule()));
        connect(&m_store, SIGNAL(changed()), this, SLOT(schedule()));
        connect(&m_store, SIGNAL(chatsChanged()), this, SLOT(schedule()));
        connect(&m_store, SIGNAL(messagesChanged()), this, SLOT(schedule()));
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(snapshot()));
        connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), &m_store, SLOT(flushDrafts()));
        m_timer.setSingleShot(true);
        schedule();
    }
    ~DiagnosticBridge() {
        // Child-process shutdown can emit signals while members are unwinding.
        // Disconnect before the snapshot timer and the store are destroyed.
        disconnect(&m_session, 0, this, 0);
        disconnect(&m_store, 0, this, 0);
        m_timer.stop();
        m_store.flushDrafts();
        m_session.stop();
    }
private slots:
    void schedule() { if (!m_timer.isActive()) m_timer.start(15); }
    void snapshot() {
        QVariantMap state;
        state.insert("@type", "snapshot");
        state.insert("phase", m_session.phase()); state.insert("status", m_session.status());
        state.insert("authError", m_session.errorText()); state.insert("version", m_session.version());
        state.insert("available", m_session.available()); state.insert("busy", m_session.busy());
        state.insert("hasSavedApi", m_session.hasSavedConfiguration()); state.insert("savedApiId", m_session.savedApiId());
        state.insert("ready", m_store.ready()); state.insert("chats", m_store.chats()); state.insert("messages", m_store.messages());
        state.insert("chatId", m_store.currentChatId()); state.insert("chatTitle", m_store.currentChatTitle());
        state.insert("draft", m_store.draftText()); state.insert("failedDraft", m_store.failedDraft());
        state.insert("error", m_store.errorText()); state.insert("connection", m_store.connectionStatus());
        state.insert("loadingChats", m_store.loadingChats()); state.insert("loadingHistory", m_store.loadingHistory());
        state.insert("canLoadChats", m_store.canLoadChats()); state.insert("canLoadHistory", m_store.canLoadHistory());
        state.insert("canSendDocument", m_store.canSendMedia("document"));
        state.insert("canSendPhoto", m_store.canSendMedia("photo"));
        state.insert("canSendVoice", m_store.canSendMedia("voice"));
        state.insert("failedAttachment", m_store.failedAttachment());
        state.insert("attachmentDraft", m_store.attachmentDraft());
        state.insert("recoveryWarning", m_store.recoveryWarning()); state.insert("cacheSummary", m_store.cacheSummary());
        state.insert("canSend", m_store.canSend()); state.insert("composerHint", m_store.composerHint());
        QByteArray bytes = JsonCodec::encode(state);
        if (bytes != m_previous) { output(bytes); m_previous = bytes; }
    }
    void readInput() {
        char bytes[8192];
        const ssize_t count = ::read(0, bytes, sizeof bytes);
        if (count <= 0) { m_stdin.setEnabled(false); m_store.flushDrafts(); QCoreApplication::quit(); return; }
        m_input.append(bytes, count);
        if (m_input.size() > 128 * 1024) { QCoreApplication::quit(); return; }
        int end;
        while ((end = m_input.indexOf('\n')) >= 0) {
            QByteArray line = m_input.left(end); m_input.remove(0, end + 1);
            QVariantMap command;
            if (!JsonCodec::decode(line, &command)) continue;
            dispatch(command);
        }
    }
private:
    void output(const QByteArray &bytes) {
        std::fwrite(bytes.constData(), 1, bytes.size(), stdout); std::fputc('\n', stdout); std::fflush(stdout);
    }
    void dispatch(const QVariantMap &command) {
        const QString action = command.value("action").toString();
        bool ok = true;
        if (action == "start") m_session.start();
        else if (action == "stop") { m_store.flushDrafts(); m_session.stop(); }
        else if (action == "configure") {
            const QString id = command.value("apiId").toString(), hash = command.value("apiHash").toString();
            ok = command.value("remember").toBool() ? m_session.configureAndRemember(id, hash) : m_session.configure(id, hash);
        } else if (action == "useSaved") ok = m_session.useSavedConfiguration();
        else if (action == "forgetApi") ok = m_session.forgetConfiguration();
        else if (action == "submit") ok = m_session.submit(command.value("value").toString());
        else if (action == "openChat") ok = m_store.openChat(command.value("chatId").toString());
        else if (action == "closeChat") m_store.closeChat();
        else if (action == "sendText") ok = command.value("chatId").toString() == m_store.currentChatId() &&
            !m_store.currentChatId().isEmpty() && m_store.sendText(command.value("text").toString());
        else if (action == "stageAttachment") ok = command.value("chatId").toString() == m_store.currentChatId() &&
            m_store.stageAttachment(command.value("path").toString(), command.value("kind").toString(), command.value("caption").toString(), command.value("duration").toInt());
        else if (action == "discardUpload") ok = command.value("chatId").toString() == m_store.currentChatId() &&
            command.value("uploadId").toString() == m_store.attachmentDraft().value("recordId").toString() && m_store.clearAttachmentDraft();
        else if (action == "recoverAttachment") ok = command.value("chatId").toString() == m_store.currentChatId() && m_store.recoverAttachment();
        else if (action == "uploadRecord") ok = command.value("chatId").toString() == m_store.currentChatId() && !m_store.uploadRecord(command.value("uploadId").toString()).isEmpty();
        else if (action == "cleanupUploads") m_store.cleanupUploads();
        else if (action == "sendAttachment") ok = command.value("chatId").toString() == m_store.currentChatId() &&
            !m_store.currentChatId().isEmpty() && m_store.sendAttachment(command.value("path").toString(), command.value("kind").toString(),
                command.value("caption").toString(), command.value("duration").toInt(), command.value("width").toInt(), command.value("height").toInt());
        else if (action == "draft") {
            ok = command.value("chatId").toString() == m_store.currentChatId() && !m_store.currentChatId().isEmpty();
            if (ok) { m_store.setDraft(command.value("text").toString()); m_store.flushDrafts(); }
        }
        else if (action == "search") m_store.setSearchQuery(command.value("text").toString());
        else if (action == "nextUnread") ok = m_store.openChat(m_store.nextUnreadChat());
        else if (action == "markRead") ok = command.value("chatId").toString() == m_store.currentChatId() && m_store.markRead();
        else if (action == "download") ok = command.value("chatId").toString() == m_store.currentChatId() &&
            m_store.downloadMedia(command.value("messageId").toString());
        else if (action == "recoverDraft") m_store.setDraft(m_store.takeFailedDraft());
        else if (action == "loadChats") m_store.loadMoreChats();
        else if (action == "loadHistory") m_store.loadMoreHistory();
        else ok = false;
        QVariantMap result; result.insert("@type", "commandResult");
        result.insert("id", command.value("id")); result.insert("ok", ok);
        if (ok && (action == "stageAttachment" || action == "recoverAttachment")) result.insert("attachmentDraft", m_store.attachmentDraft());
        if (ok && action == "uploadRecord") result.insert("uploadRecord", m_store.uploadRecord(command.value("uploadId").toString()));
        output(JsonCodec::encode(result));
        schedule();
    }
    TelegramSession m_session;
    TelegramStore m_store;
    QSocketNotifier m_stdin;
    QTimer m_timer;
    QByteArray m_input, m_previous;
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList args = application.arguments();
    if (args.size() != 4) { std::fputs("Usage: diagnostic-core ENGINE LIBRARY DATA_DIRECTORY\n", stderr); return 64; }
    DiagnosticBridge bridge(args.at(1), args.at(2), args.at(3));
    return application.exec();
}

#include "core.moc"
