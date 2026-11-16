#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
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
    Peer(int timeout = 60000)
        : library(directory.path() + "/libtdjson.so"),
          session("/source/tests/fake-chat-engine.py", library, directory.path() + "/private"),
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

private slots:
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
        QVERIFY(peer.store.failedDraft().isEmpty());
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
