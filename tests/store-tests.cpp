#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
#include "LocalOutbox.hpp"
#include "PrivateFile.hpp"
#include "MessagePresentation.hpp"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

namespace {
const QString A = "-1001234567890", B = "9007199254740991", C = "-1001234567891";
QVariantMap object(const char *type) { QVariantMap value; value.insert("@type", type); return value; }
QVariantMap content(const QString &text) {
    QVariantMap value = object("messageText"), formatted = object("formattedText");
    formatted.insert("text", text); value.insert("text", formatted); return value;
}
QVariantMap position(const QString &order) {
    QVariantMap value = object("chatPosition"); value.insert("order", order);
    value.insert("list", object("chatListMain")); return value;
}
QVariantMap row(const TelegramStore &store, const QString &id) {
    foreach (const QVariant &value, store.messages())
        if (value.toMap().value("messageId") == id) return value.toMap();
    return QVariantMap();
}
struct Peer {
    QTemporaryDir directory;
    QString library;
    TelegramSession session;
    TelegramStore store;
    Peer(int timeout = 60000, const QString &existing = QString())
        : library((existing.isEmpty() ? directory.path() : existing) + "/libtdjson.so"),
          session("/source/tests/fake-chat-engine.py", library, QFileInfo(library).absolutePath() + "/private"),
          store(&session, 0, timeout) {
        QFile file(library); file.open(QIODevice::WriteOnly); file.write("fixture"); file.close();
    }
};
}

class StoreTests : public QObject {
    Q_OBJECT
private:
    void ready(Peer &peer) {
        peer.session.start();
        QTRY_COMPARE(peer.session.phase(), QString("parameters"));
        QVERIFY(peer.session.configure("12345", QString(32, 'a')));
        QTRY_VERIFY(peer.store.ready());
        QTRY_VERIFY(!peer.store.loadingChats());
        QTRY_COMPARE(peer.store.chats().size(), 3);
    }
    void open(Peer &peer, const QString &id = A) {
        QVERIFY(peer.store.openChat(id));
        QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!peer.store.messages().isEmpty());
    }
    QVariantMap control(Peer &peer, const QString &op, const QVariantMap &event = QVariantMap()) {
        QVariantMap command = object("fixtureControl"); command.insert("op", op);
        command.insert("event", event);
        QSignalSpy spy(&peer.session, SIGNAL(eventReceived(QVariantMap)));
        QString id = peer.session.sendReady(command);
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 3000) {
            for (int i = 0; i < spy.size(); ++i) {
                QVariantMap response = spy.at(i).at(0).toMap();
                if (response.value("@extra") == id) return response;
            }
            QTest::qWait(5);
        }
        return QVariantMap();
    }
    void update(Peer &peer, QVariantMap event, const QString &chatId = A) {
        event.insert("chat_id", QVariant::fromValue(chatId.toLongLong()));
        QVERIFY(!control(peer, "emit", event).isEmpty());
    }

    QString payload(const QString &data, const QString &name = "note.txt") {
        QDir().mkpath(data + "/uploads/copy");
        const QString path = data + "/uploads/copy/" + name;
        QFile file(path); if (!file.open(QIODevice::WriteOnly)) return QString();
        file.write("Synthetic retained input"); file.close(); return path;
    }
    QVariantMap attachment(const QString &path, const QString &caption = "caption") {
        QVariantMap value; value.insert("type", "attachment"); value.insert("path", path);
        value.insert("kind", "document"); value.insert("caption", caption); return value;
    }

private slots:
    void messagePresentationGroupsOnlyMatchingAuthorsAndDates() {
        const QDate today(2026,11,13);
        const QDateTime yesterday(today.addDays(-1),QTime(18,0));
        const QDateTime morning(today,QTime(9,0));
        QVariantList source;
        for(int i=0;i<5;++i) {
            QVariantMap message;
            message.insert("messageId",QString::number(i+1));
            message.insert("senderId",i==4?"200":"100");
            message.insert("author","Same display name");message.insert("outgoing",false);
            message.insert("date",(i<2?yesterday.addSecs(i*20):morning.addSecs((i-2)*20)).toTime_t());
            source.append(message);
        }
        QVariantList group=MessagePresentation::decorate(source,true,"2",true,today);
        QCOMPARE(group.at(0).toMap().value("dateLabel").toString(),QString::fromUtf8("Вчера"));
        QVERIFY(group.at(0).toMap().value("showAuthor").toBool());
        QVERIFY(group.at(1).toMap().value("joinedPrevious").toBool());
        QVERIFY(!group.at(1).toMap().value("showAuthor").toBool());
        QCOMPARE(group.at(2).toMap().value("dateLabel").toString(),QString::fromUtf8("Сегодня"));
        QVERIFY(group.at(2).toMap().value("startsDay").toBool());
        QVERIFY(group.at(2).toMap().value("startsUnread").toBool());
        QVERIFY(!group.at(3).toMap().value("startsUnread").toBool());
        QVERIFY(group.at(3).toMap().value("joinedPrevious").toBool());
        QVERIFY(!group.at(4).toMap().value("joinedPrevious").toBool());
        QVERIFY(group.at(4).toMap().value("showAuthor").toBool());
        const QVariantList personal=MessagePresentation::decorate(source,false,"2",true,today);
        for(int i=0;i<personal.size();++i) QVERIFY(!personal.at(i).toMap().value("showAuthor").toBool());
        QCOMPARE(personal.at(2).toMap().value("time").toString(),QString("09:00"));
    }

    void messagePresentationBreaksLongPausesAndUnreadBoundaries() {
        const QDateTime start(QDate(2026,11,13),QTime(12,0));
        QVariantList source;
        for(int i=0;i<4;++i) {
            QVariantMap message;message.insert("messageId",QString::number(i+1));
            message.insert("senderId","100");message.insert("outgoing",false);
            message.insert("date",start.addSecs(i==0?0:300+i).toTime_t());source.append(message);
        }
        const QVariantList rows=MessagePresentation::decorate(source,true,"2",true,start.date());
        QVERIFY(!rows.at(1).toMap().value("joinedPrevious").toBool());
        QVERIFY(rows.at(2).toMap().value("startsUnread").toBool());
        QVERIFY(!rows.at(2).toMap().value("joinedPrevious").toBool());
        QVERIFY(rows.at(3).toMap().value("joinedPrevious").toBool());
    }

    void journalSurvivesRestartAndSeparatesAccounts() {
        QTemporaryDir directory; const QString data = directory.path() + "/private";
        const QString file = payload(data); QString id;
        {
            LocalOutbox journal(data); QVERIFY(journal.open());
            id = journal.prepare("123", A, attachment(file, QString::fromUtf8("Подпись 😀"))); QVERIFY(!id.isEmpty());
            QVERIFY(journal.rememberDraft("123", A, QString::fromUtf8("Черновик 😀")));
            QVERIFY(journal.transition(id, "pending", "6001"));
        }
        LocalOutbox restored(data); QVERIFY(restored.open());
        QCOMPARE(restored.entry(id).value("state").toString(), QString("uncertain"));
        QCOMPARE(restored.draft("123", A).toString(), QString::fromUtf8("Черновик 😀"));
        QVERIFY(restored.latest("456", A, "attachment", QStringList() << "uncertain").isEmpty());
        QVERIFY(!restored.draft("456", A).isValid());
        QVERIFY(QFile::permissions(data + "/outbox.json") & QFile::ReadOwner);
        QVERIFY(!(QFile::permissions(data + "/outbox.json") & QFile::ReadOther));
    }

    void cleanupOnlyRemovesOldConfirmedUnreferencedCopies() {
        QTemporaryDir directory; const QString data = directory.path() + "/private";
        LocalOutbox journal(data); QVERIFY(journal.open());
        const QString sentFile = payload(data, "sent.txt"), pendingFile = payload(data, "pending.txt"), orphan = payload(data, "orphan.txt");
        const QString sent = journal.prepare("123", A, attachment(sentFile)); QVERIFY(journal.transition(sent, "sent"));
        const QString active = journal.prepare("123", B, attachment(pendingFile)); QVERIFY(journal.transition(active, "pending"));
        QCOMPARE(journal.cleanup(), 0); QVERIFY(QFile::exists(sentFile));
        const qlonglong tomorrow = QDateTime::currentDateTime().toTime_t() + 86401;
        QCOMPARE(journal.cleanup(tomorrow), 1); QVERIFY(!QFile::exists(sentFile));
        QVERIFY(QFile::exists(pendingFile)); QVERIFY(QFile::exists(orphan));
        QVERIFY(journal.transition(active, "uncertain")); QCOMPARE(journal.cleanup(tomorrow + 864000), 0);
        // A confirmed attempt sharing a file with an uncertain attempt must not delete it.
        const QString second = journal.prepare("123", B, attachment(pendingFile)); QVERIFY(journal.transition(second, "sent"));
        QCOMPARE(journal.cleanup(tomorrow + 864000), 0); QVERIFY(QFile::exists(pendingFile));
    }

    void malformedOrSymlinkJournalBlocksSendingAndCleanup() {
        QTemporaryDir directory; const QString data = directory.path() + "/private";
        { Peer initial(60000, directory.path()); ready(initial); }
        const QString file = payload(data);
        QFile broken(data + "/outbox.json"); QVERIFY(broken.open(QIODevice::WriteOnly)); broken.write("not JSON"); broken.close();
        LocalOutbox corrupted(data); QVERIFY(!corrupted.open()); QCOMPARE(corrupted.cleanup(9999999999LL), 0); QVERIFY(QFile::exists(file));
        QVERIFY(broken.remove());
        QVERIFY(QFile::link(file, data + "/outbox.json"));
        Peer peer(60000, directory.path()); ready(peer); open(peer);
        QVERIFY(!peer.store.canSend()); QVERIFY(!peer.store.sendText("never sent"));
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 0);
        QCOMPARE(peer.store.cleanupUploads(), 0); QVERIFY(QFile::exists(file));
    }

    void failedJournalWriteDoesNotHandMessageToEngine() {
        Peer peer; ready(peer); open(peer);
        const QString file = payload(peer.directory.path() + "/private");
        QVERIFY(QFile::link(file, peer.directory.path() + "/private/outbox.json"));
        QVERIFY(!peer.store.sendText("must be durable first"));
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 0);
        QVERIFY(!peer.store.recoveryWarning().isEmpty()); QVERIFY(QFile::exists(file));
    }

    void selectedAttachmentAndCaptionRestoreAfterProcessRecreation() {
        QTemporaryDir directory; const QString data = directory.path() + "/private";
        QString file;
        {
            Peer peer(60000, directory.path()); ready(peer); open(peer); file = payload(data);
            QVERIFY(peer.store.stageAttachment(file, "document", QString::fromUtf8("Начало 😀")));
            peer.store.setDraft(QString::fromUtf8("Сохранённая подпись 😀")); peer.store.flushDrafts();
            QVERIFY(peer.store.openChat(B)); open(peer, A);
            QCOMPARE(peer.store.attachmentDraft().value("path").toString(), file);
            QCOMPARE(control(peer, "stats").value("send_count").toInt(), 0);
        }
        Peer recreated(60000, directory.path()); ready(recreated); open(recreated);
        QCOMPARE(recreated.store.attachmentDraft().value("caption").toString(), QString::fromUtf8("Сохранённая подпись 😀"));
        QCOMPARE(recreated.store.draftText(), QString::fromUtf8("Сохранённая подпись 😀"));
        QCOMPARE(control(recreated, "stats").value("send_count").toInt(), 0);
    }

    void networkLossAndLateMediaResponseNeverRepeatSend() {
        Peer peer(300); ready(peer); open(peer);
        const QString file = payload(peer.directory.path() + "/private");
        QVariantMap state = object("updateConnectionState"); state.insert("state", object("connectionStateWaitingForNetwork"));
        QVERIFY(!control(peer, "emit", state).isEmpty());
        QVERIFY(peer.store.sendAttachment(file, "document", "timeout-media"));
        QTRY_VERIFY(!peer.store.sendBusy());
        QVERIFY(peer.store.recoveryWarning().contains(QString::fromUtf8("Проверьте историю")));
        QVERIFY(QFile::exists(file)); QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
        state.insert("state", object("connectionStateReady")); QVERIFY(!control(peer, "emit", state).isEmpty());
        QVERIFY(!control(peer, "releaseMedia").isEmpty());
        QTRY_VERIFY(peer.store.recoveryWarning().isEmpty());
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1); QVERIFY(QFile::exists(file));
    }

    void engineExitRestoresUncertainAttachmentWithoutResubmission() {
        Peer peer; ready(peer); open(peer);
        const QString file = payload(peer.directory.path() + "/private");
        QVERIFY(peer.store.sendAttachment(file, "document", "timeout-media"));
        control(peer, "crash"); QTRY_COMPARE(peer.session.phase(), QString("failed"));
        ready(peer); open(peer);
        QCOMPARE(peer.store.failedAttachment().value("state").toString(), QString("uncertain"));
        QVERIFY(peer.store.recoveryWarning().contains(QString::fromUtf8("Проверьте историю")));
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 0);
        QVERIFY(peer.store.recoverAttachment());
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 0); QVERIFY(QFile::exists(file));
    }

    void unsentTextSurvivesRestartWithoutResubmission() {
        QTemporaryDir directory;
        {
            Peer peer(250, directory.path()); ready(peer); open(peer);
            QVERIFY(peer.store.sendText("timeout")); QTRY_VERIFY(!peer.store.sendBusy());
            QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
        }
        Peer recreated(60000, directory.path()); ready(recreated); open(recreated);
        QCOMPARE(recreated.store.failedDraft(), QString("timeout"));
        QCOMPARE(recreated.store.takeFailedDraft(), QString("timeout"));
        QCOMPARE(recreated.store.draftText(), QString("timeout"));
        QCOMPARE(control(recreated, "stats").value("send_count").toInt(), 0);
    }
    void attachmentsValidatePathsFormatsPermissionsAndRecovery() {
        Peer peer; ready(peer); open(peer);
        const QString directory = peer.directory.path() + "/private/uploads";
        QVERIFY(QDir().mkpath(directory));
        QFile document(directory + "/note.txt"); QVERIFY(document.open(QIODevice::WriteOnly)); document.write("synthetic document"); document.close();
        QVERIFY(peer.store.canSendMedia("document"));
        QVERIFY(!peer.store.sendAttachment(peer.library, "document", "outside"));
        QVERIFY(!peer.store.sendAttachment(directory + "/missing", "document", "missing"));
        QVERIFY(QFile::link(document.fileName(), directory + "/link.txt"));
        QVERIFY(!peer.store.sendAttachment(directory + "/link.txt", "document", "symlink"));
        QVERIFY(!peer.store.sendAttachment(document.fileName(), "photo", "fake image", 0, 100, 100));
        QVERIFY(!peer.store.sendAttachment(document.fileName(), "voice", "fake voice", 3));
        QVERIFY(!peer.store.sendAttachment(document.fileName(), "document", QString(1025, 'x')));
        QVERIFY(peer.store.sendAttachment(document.fileName(), "document", "deny"));
        QTRY_VERIFY(!peer.store.sendBusy()); QTRY_VERIFY(!peer.store.failedAttachment().isEmpty());
        QVERIFY(peer.store.failedDraft().isEmpty());
        QCOMPARE(peer.store.failedAttachment().value("caption").toString(), QString("deny"));
        QVERIFY(QFile::exists(document.fileName()));
        QVERIFY(peer.store.sendAttachment(document.fileName(), "document", "fail-later"));
        QTRY_VERIFY(!peer.store.sendBusy());
        QTRY_COMPARE(peer.store.failedAttachment().value("caption").toString(), QString("fail-later"));
        QVERIFY(peer.store.sendAttachment(document.fileName(), "document", "fail-before-response"));
        QTRY_VERIFY(!peer.store.sendBusy());
        QTRY_COMPARE(peer.store.failedAttachment().value("caption").toString(), QString("fail-before-response"));
        QVERIFY(peer.store.sendAttachment(document.fileName(), "document", "caption"));
        QTRY_VERIFY(!peer.store.sendBusy()); QTRY_VERIFY(peer.store.failedAttachment().isEmpty());
        QVERIFY(QFile::exists(document.fileName()));
        QFile voice(directory + "/voice.m4a"); QVERIFY(voice.open(QIODevice::WriteOnly)); voice.write(QByteArray::fromHex("00000018667479704d344120000000004d34412069736f6d")); voice.close();
        QVERIFY(!peer.store.sendAttachment(voice.fileName(), "voice", "", 301));
        QVERIFY(peer.store.sendAttachment(voice.fileName(), "voice", "", 2));
        QTRY_VERIFY(!peer.store.sendBusy());
        bool hasVoice = false;
        foreach (const QVariant &value, peer.store.messages()) if (value.toMap().value("media").toMap().value("kind") == "voice") hasVoice = true;
        QVERIFY(hasVoice);
        QVariantMap event = object("updateChatPermissions"), permissions = object("chatPermissions");
        permissions.insert("can_send_basic_messages", false); permissions.insert("can_send_documents", true);
        event.insert("permissions", permissions); update(peer, event);
        QVERIFY(!peer.store.canSend()); QVERIFY(peer.store.canSendMedia("document")); QVERIFY(!peer.store.canSendMedia("photo"));
        QVERIFY(!peer.store.sendAttachment(voice.fileName(), "voice", "", 2));
        open(peer, C); QVERIFY(!peer.store.canSendMedia("document"));
    }

    void draftRestoreAndSearchLoadedChats() {
        Peer peer; ready(peer); open(peer);
        const QString draft = QString::fromUtf8("  Черновик 😀\nстрока 2  ");
        peer.store.setDraft(draft); peer.store.flushDrafts();
        QTest::qWait(100);
        QCOMPARE(peer.store.draftText(), draft);
        peer.store.closeChat(); open(peer, B); QVERIFY(peer.store.draftText().isEmpty());
        open(peer, A); QCOMPARE(peer.store.draftText(), draft);
        peer.store.setSearchQuery(QString::fromUtf8("личНОЕ"));
        QCOMPARE(peer.store.chats().size(), 1);
        QCOMPARE(peer.store.chats().first().toMap().value("chatId").toString(), B);
        QCOMPARE(peer.store.nextUnreadChat(), B);
        peer.store.setSearchQuery(QString()); QCOMPARE(peer.store.chats().size(), 3);
        QVERIFY(peer.store.sendText(draft)); QTRY_VERIFY(!peer.store.sendBusy());
        QCOMPARE(peer.store.draftText(), QString());
    }

    void cachedHistoryWhenNetworkIsUnavailable() {
        Peer peer; ready(peer); open(peer);
        QVariantMap event = object("updateConnectionState"); event.insert("state", object("connectionStateWaitingForNetwork"));
        QVERIFY(!control(peer, "emit", event).isEmpty());
        peer.store.loadMoreHistory(); QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(control(peer, "stats").value("last_only_local").toBool());
        event.insert("state", object("connectionStateReady")); QVERIFY(!control(peer, "emit", event).isEmpty());
        peer.store.loadMoreHistory(); QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!control(peer, "stats").value("last_only_local").toBool());
    }

    void unreadNavigationCyclesAndReadIsExplicit() {
        Peer peer; ready(peer); open(peer, A);
        QCOMPARE(peer.store.nextUnreadChat(), B);
        open(peer, B); QCOMPARE(peer.store.nextUnreadChat(), C);
        open(peer, C); QCOMPARE(peer.store.nextUnreadChat(), A);
        QVERIFY(peer.store.markRead());
        QTRY_COMPARE(peer.store.chats().last().toMap().value("unreadCount").toInt(), 0);
        open(peer, B); QCOMPARE(peer.store.nextUnreadChat(), A);
    }

    void mediaDownloadIsScopedToPrivateFiles() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(peer.store.downloadMedia("20"));
        QTRY_VERIFY(!peer.store.mediaPath("20").isEmpty());
        QVERIFY(peer.store.mediaPath("20").startsWith(peer.directory.path() + "/private/files/"));
        QCOMPARE(peer.store.mediaMime("20"), QString("image/jpeg"));
        QVERIFY(!peer.store.downloadMedia("999999"));
        QFile image(peer.store.mediaPath("20")); QVERIFY(image.remove());
        QVERIFY(QFile::link(peer.library, peer.directory.path() + "/private/files/photo.png"));
        QVERIFY(peer.store.mediaPath("20").isEmpty());
    }
    void listOrderIdsAndPagination() {
        Peer peer; ready(peer);
        QCOMPARE(peer.store.chats().at(0).toMap().value("chatId").toString(), A);
        QCOMPARE(peer.store.chats().at(1).toMap().value("chatId").toString(), B);
        QCOMPARE(peer.store.chats().at(2).toMap().value("chatId").toString(), C);
        QVERIFY(!peer.store.chats().at(0).toMap().contains("mainOrder"));
        peer.store.loadMoreChats();
        QTRY_VERIFY(!peer.store.loadingChats());
        QVERIFY(!peer.store.canLoadChats()); // 404 means the end, not a failure.
        QVERIFY(peer.store.errorText().isEmpty());
        QVariantMap event = object("updateChatPosition"); event.insert("position", position("0"));
        update(peer, event);
        QCOMPARE(peer.store.chats().size(), 2);
        event.insert("position", position("9223372036854775807")); update(peer, event, C);
        QCOMPARE(peer.store.chats().at(0).toMap().value("chatId").toString(), C);
        event = object("updateChatReadInbox"); event.insert("unread_count", 9); update(peer, event, B);
        QCOMPARE(peer.store.chats().at(1).toMap().value("unreadLabel").toString(), QString("9"));
        event = object("updateChatTitle"); event.insert("title", QString::fromUtf8("Название 😀")); update(peer, event, C);
        QCOMPARE(peer.store.chats().at(0).toMap().value("title").toString(), QString::fromUtf8("Название 😀"));
    }

    void historyPaginationAndMedia() {
        Peer peer; ready(peer); open(peer);
        QCOMPARE(peer.store.messages().size(), 3);
        QCOMPARE(peer.store.messages().first().toMap().value("messageId").toString(), QString("10"));
        QCOMPARE(row(peer.store, "30").value("author").toString(), QString::fromUtf8("Анна 😀"));
        QVERIFY(row(peer.store, "20").value("text").toString().contains(QString::fromUtf8("Фото")));
        QVERIFY(row(peer.store, "20").value("text").toString().contains(QString::fromUtf8("Подпись фото")));
        QVERIFY(peer.store.canLoadHistory()); // A short page does not mean the end.
        peer.store.loadMoreHistory();
        QTRY_VERIFY(!peer.store.loadingHistory());
        QCOMPARE(peer.store.messages().size(), 4); // Shared boundary message is deduplicated.
        QCOMPARE(peer.store.messages().first().toMap().value("messageId").toString(), QString("5"));
        peer.store.loadMoreHistory(); QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!peer.store.canLoadHistory());
    }

    void updatesWinOverInFlightHistory() {
        Peer peer; ready(peer);
        QVERIFY(!control(peer, "holdHistory").isEmpty());
        QVERIFY(peer.store.openChat(A));
        QVERIFY(peer.store.loadingHistory());
        QVariantMap edited = object("updateMessageContent"); edited.insert("message_id", 30);
        edited.insert("new_content", content(QString::fromUtf8("Уже исправлено 😀"))); update(peer, edited);
        edited = object("updateMessageEdited"); edited.insert("message_id", 30); edited.insert("edit_date", 1700000020); update(peer, edited);
        QVariantMap deleted = object("updateDeleteMessages"); deleted.insert("message_ids", QVariantList() << 10); update(peer, deleted);
        QVERIFY(!control(peer, "releaseHistory").isEmpty());
        QTRY_VERIFY(!peer.store.loadingHistory());
        QCOMPARE(peer.store.messages().size(), 2);
        QVERIFY(row(peer.store, "10").isEmpty());
        QCOMPARE(row(peer.store, "30").value("text").toString(), QString::fromUtf8("Уже исправлено 😀"));
        QVERIFY(row(peer.store, "30").value("edited").toBool());
    }

    void lateHistoryDoesNotMixChatsOrReopens() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(!control(peer, "holdHistory").isEmpty());
        QVERIFY(peer.store.openChat(B));
        QVERIFY(peer.store.loadingHistory());
        open(peer, A);
        QVERIFY(!control(peer, "releaseHistory").isEmpty());
        QVERIFY(row(peer.store, "90").isEmpty());
        QCOMPARE(peer.store.currentChatTitle(), QString::fromUtf8("Группа 😀"));
        QVERIFY(!control(peer, "holdHistory").isEmpty());
        QVERIFY(peer.store.openChat(A));
        peer.store.closeChat();
        open(peer, A);
        QVariantMap event = object("updateDeleteMessages"); event.insert("message_ids", QVariantList() << 10); update(peer, event);
        QVERIFY(!control(peer, "releaseHistory").isEmpty());
        QVERIFY(row(peer.store, "10").isEmpty());
        QCOMPARE(peer.store.messages().size(), 2);
    }

    void outgoingStatesAndReadReceipt() {
        Peer peer; ready(peer); open(peer, B); // Also exercises an exact int53 request.
        const QString text = QString::fromUtf8("  Привет \" \\ \n😀  ");
        QVERIFY(peer.store.sendText(text));
        QVERIFY(!peer.store.sendText("duplicate"));
        QTRY_VERIFY(!peer.store.sendBusy());
        QCOMPARE(row(peer.store, "1000").value("text").toString(), text);
        QCOMPARE(row(peer.store, "1000").value("delivery").toString(), QString::fromUtf8("Telegram получил запрос"));
        QVERIFY(!control(peer, "finishSend").isEmpty());
        QVERIFY(row(peer.store, "1000").isEmpty());
        QCOMPARE(row(peer.store, "1001").value("delivery").toString(), QString::fromUtf8("Отправлено"));
        QVariantMap read = object("updateChatReadOutbox"); read.insert("last_read_outbox_message_id", 1001); update(peer, read, B);
        QCOMPARE(row(peer.store, "1001").value("delivery").toString(), QString::fromUtf8("Прочитано"));
        QCOMPARE(peer.store.messages().size(), 2);
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
    }

    void successBeforeResponseDoesNotRestoreTemporaryId() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(peer.store.sendText("race"));
        QTRY_VERIFY(!peer.store.sendBusy());
        QVERIFY(row(peer.store, "2000").isEmpty());
        QVERIFY(!row(peer.store, "2001").isEmpty());
        QCOMPARE(peer.store.messages().size(), 4);
    }

    void sendErrorKeepsRecoverableText() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(peer.store.sendText("deny"));
        QTRY_VERIFY(!peer.store.sendBusy());
        QCOMPARE(peer.store.failedDraft(), QString("deny"));
        QCOMPARE(peer.store.messages().last().toMap().value("delivery").toString(), QString::fromUtf8("Не отправлено"));
        QVERIFY(!peer.store.errorText().contains("private"));
        QCOMPARE(peer.store.takeFailedDraft(), QString("deny"));
        QVERIFY(peer.store.failedDraft().isEmpty());
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
    }

    void acknowledgementAndFailureBeforeResponse() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(peer.store.sendText("ack-race")); QTRY_VERIFY(!peer.store.sendBusy());
        QCOMPARE(row(peer.store, "5000").value("delivery").toString(), QString::fromUtf8("Telegram получил запрос"));
        QVERIFY(peer.store.sendText("failed-race")); QTRY_VERIFY(!peer.store.sendBusy());
        QCOMPARE(row(peer.store, "4000").value("delivery").toString(), QString::fromUtf8("Не отправлено"));
        QCOMPARE(peer.store.failedDraft(), QString("failed-race"));
    }

    void incomingUpdatesAndPreviewEdits() {
        Peer peer; ready(peer); open(peer);
        QVariantMap message = object("message");
        message.insert("id", 40); message.insert("chat_id", QVariant::fromValue(A.toLongLong()));
        message.insert("date", 1700000040); message.insert("is_outgoing", false);
        QVariantMap sender = object("messageSenderUser"); sender.insert("user_id", QVariant::fromValue(qlonglong(9007199254740990LL)));
        message.insert("sender_id", sender); message.insert("content", content(QString::fromUtf8("Новое 😀")));
        QVariantMap event = object("updateNewMessage"); event.insert("message", message); update(peer, event);
        QCOMPARE(peer.store.messages().size(), 4);
        QVERIFY(row(peer.store, "40").value("delivery").toString().isEmpty());
        event = object("updateChatLastMessage"); event.insert("last_message", message);
        event.insert("positions", QVariantList() << position("9223372036854775806")); update(peer, event);
        peer.store.closeChat();
        event = object("updateMessageContent"); event.insert("message_id", 40);
        event.insert("new_content", content(QString::fromUtf8("Исправленный preview"))); update(peer, event);
        QCOMPARE(peer.store.chats().first().toMap().value("preview").toString(), QString::fromUtf8("Исправленный preview"));
    }

    void failureUpdateRetainsMessage() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(peer.store.sendText("will-fail")); QTRY_VERIFY(!peer.store.sendBusy());
        QVERIFY(!control(peer, "failSend").isEmpty());
        QCOMPARE(row(peer.store, "1000").value("delivery").toString(), QString::fromUtf8("Не отправлено"));
        QCOMPARE(peer.store.failedDraft(), QString("will-fail"));
        QVERIFY(!peer.store.errorText().contains("private"));
    }

    void uncertainSendIsNotRetriedAndLateReplyStillWorks() {
        Peer peer(500); ready(peer); open(peer);
        QVERIFY(peer.store.sendText("timeout"));
        QTRY_VERIFY(!peer.store.sendBusy());
        QCOMPARE(peer.store.messages().last().toMap().value("delivery").toString(), QString::fromUtf8("Результат отправки неизвестен"));
        QCOMPARE(peer.store.failedDraft(), QString("timeout"));
        QVERIFY(peer.store.recoveryWarning().contains(QString::fromUtf8("Проверьте историю")));
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
        QVERIFY(!control(peer, "releaseSend").isEmpty());
        QTRY_VERIFY(!row(peer.store, "3001").isEmpty());
        QVERIFY(peer.store.errorText().isEmpty());
        QCOMPARE(peer.store.messages().size(), 4);
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
    }

    void historyTimeoutAllowsRetry() {
        Peer peer(500); ready(peer);
        QVERIFY(!control(peer, "holdHistory").isEmpty());
        QVERIFY(peer.store.openChat(A));
        QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!peer.store.errorText().isEmpty());
        QVERIFY(peer.store.canLoadHistory());
        peer.store.loadMoreHistory();
        QTRY_VERIFY(!peer.store.loadingHistory());
        QCOMPARE(peer.store.messages().size(), 3);
        QVERIFY(peer.store.canLoadHistory());
        QVERIFY(!control(peer, "releaseHistory").isEmpty());
        QCOMPARE(peer.store.messages().size(), 3);
    }

    void permissionsValidationAndDisconnectClearData() {
        Peer peer; ready(peer); open(peer, C);
        QVERIFY(!peer.store.canSend());
        QVERIFY(!peer.store.sendText("forbidden"));
        open(peer, A);
        QVERIFY(peer.store.canSend());
        QVERIFY(!peer.store.sendText(" \n "));
        QVERIFY(!peer.store.sendText(QString(4097, 'a')));
        QVariantMap permissions = object("updateChatPermissions");
        QVariantMap values = object("chatPermissions"); values.insert("can_send_basic_messages", false);
        permissions.insert("permissions", values); update(peer, permissions);
        QVERIFY(!peer.store.canSend());
        QVariantMap state = object("updateConnectionState"); state.insert("state", object("connectionStateWaitingForNetwork"));
        QVERIFY(!control(peer, "emit", state).isEmpty());
        QCOMPARE(peer.store.connectionStatus(), QString::fromUtf8("Ожидание сети"));
        peer.session.stop();
        QVERIFY(!peer.store.ready());
        QVERIFY(peer.store.chats().isEmpty());
        QVERIFY(peer.store.messages().isEmpty());
        QVERIFY(peer.store.failedDraft().isEmpty());
        QTRY_COMPARE(peer.session.phase(), QString("stopped"));
    }
};

QTEST_GUILESS_MAIN(StoreTests)
#include "store-tests.moc"
