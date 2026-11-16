#include "TelegramSession.hpp"
#include "TelegramStore.hpp"
#include "TelegramSearch.hpp"
#include "AutoPager.hpp"
#include "Presence.hpp"
#include "NotificationPolicy.hpp"
#include "FormattedText.hpp"
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

    void addOutgoing(Peer &peer){QVariantMap event=object("updateNewMessage"),message=object("message");message.insert("chat_id",A.toLongLong());message.insert("id",50);message.insert("is_outgoing",true);message.insert("content",content("original"));event.insert("message",message);update(peer,event);}
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
    void groupAndChannelMessagesUseWideLayoutWhilePrivateChatsDoNot() {
        Peer peer; ready(peer); open(peer,A); QVERIFY(peer.store.currentChatWide());
        open(peer,B); QVERIFY(!peer.store.currentChatWide());
        open(peer,C); QVERIFY(peer.store.currentChatWide());
        peer.store.closeChat(); QVERIFY(!peer.store.currentChatWide());
    }
    void communityAvatarsLoadChangeAndDisappear(){
        Peer peer;ready(peer);
        foreach(const QString &id,QStringList()<<A<<C){
            QVariantMap photo,file=object("file");file.insert("id",id==A?77:78);file.insert("size",68);photo.insert("small",file);
            QVariantMap event=object("updateChatPhoto");event.insert("photo",photo);update(peer,event,id);
            peer.store.requestAvatar("chat:"+id);QTRY_VERIFY(!peer.store.avatarPath("chat:"+id).isEmpty());
        }
        QVariantMap event=object("updateChatPhoto"),photo,file=object("file");file.insert("id",78);file.insert("size",68);photo.insert("small",file);event.insert("photo",photo);update(peer,event,A);
        QCOMPARE(peer.store.avatarPath("chat:"+A),peer.store.avatarPath("chat:"+C));
        event.insert("photo",QVariant());update(peer,event,A);QVERIFY(peer.store.avatarPath("chat:"+A).isEmpty());QVERIFY(!peer.store.avatarPath("chat:"+C).isEmpty());
    }
    void commentsUseDiscussionHistoryTopicAndSeparateDrafts(){
        Peer peer;ready(peer);open(peer);peer.store.setDraft("group draft");peer.store.flushDrafts();
        QVERIFY(peer.store.openComments("30"));QTRY_VERIFY(peer.store.inComments());QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!row(peer.store,"320").isEmpty());QVERIFY(row(peer.store,"30").isEmpty());QCOMPARE(peer.store.draftText(),QString());
        peer.store.setDraft("thread draft");peer.store.flushDrafts();
        QVariantMap event=object("updateNewMessage"),message=object("message");message.insert("id",999);message.insert("chat_id",A.toLongLong());message.insert("content",content("unrelated"));event.insert("message",message);update(peer,event);
        QVERIFY(row(peer.store,"999").isEmpty());
        QVERIFY(peer.store.sendText("comment"));QTRY_VERIFY(!peer.store.sendBusy());QTRY_VERIFY(!row(peer.store,"340").isEmpty());
        QVariantList actions=control(peer,"stats").value("actions").toList();bool sent=false,read=false;
        foreach(const QVariant &value,actions){const QVariantMap call=value.toMap();if(call.value("@type")=="sendMessage"){sent=true;QCOMPARE(call.value("topic_id").toMap().value("message_thread_id").toInt(),300);QCOMPARE(call.value("reply_to").toMap().value("message_id").toInt(),300);}if(call.value("@type")=="viewMessages" && call.value("source").toMap().value("@type")=="messageSourceMessageThreadHistory"){read=true;QVERIFY(call.value("message_ids").toList().first().toLongLong()<=330);}}
        QVERIFY(sent);QVERIFY(read);
        peer.store.setDraft("unfinished comment");peer.store.flushDrafts();QVERIFY(peer.store.returnToPost());QTRY_VERIFY(!peer.store.loadingHistory());QVERIFY(!peer.store.inComments());QCOMPARE(peer.store.draftText(),QString("group draft"));
        QVERIFY(peer.store.openComments("30"));QTRY_VERIFY(peer.store.inComments());QCOMPARE(peer.store.draftText(),QString("unfinished comment"));
    }
    void commentsHonorReadOnlyPermissionAndDiscardLateThread(){
        Peer peer;ready(peer);open(peer);control(peer,"readOnlyThread");QVERIFY(peer.store.openComments("30"));QTRY_VERIFY(peer.store.inComments());QVERIFY(!peer.store.canSend());QVERIFY(!peer.store.sendText("blocked"));
        QCOMPARE(control(peer,"stats").value("send_count").toInt(),0);QVERIFY(peer.store.returnToPost());QTRY_VERIFY(!peer.store.loadingHistory());
        control(peer,"holdThread");QVERIFY(peer.store.openComments("30"));QTRY_VERIFY(control(peer,"stats").value("actions").toList().last().toMap().value("@type")=="getMessageThread");
        QVERIFY(peer.store.openChat(B));control(peer,"releaseThread");QVERIFY(!peer.store.inComments());QCOMPARE(peer.store.currentChatId(),B);QVERIFY(!peer.store.commentsLoading());
    }
    void channelCommentsHydrateDiscussionAndUpdateCount(){
        Peer peer;ready(peer);QVariantMap channel=object("chat"),type=object("chatTypeSupergroup");type.insert("is_channel",true);channel.insert("id",C.toLongLong());channel.insert("type",type);channel.insert("title","Channel");peer.store.consume(channel);open(peer,C);QVERIFY(!peer.store.composerVisible());
        QVariantMap info,reply,event=object("updateMessageInteractionInfo");reply.insert("reply_count",0);info.insert("reply_info",reply);event.insert("interaction_info",info);event.insert("message_id",90);update(peer,event,C);QVERIFY(row(peer.store,"90").value("hasComments").toBool());
        reply.insert("reply_count",5);info.insert("reply_info",reply);event.insert("interaction_info",info);update(peer,event,C);QCOMPARE(row(peer.store,"90").value("commentCount").toInt(),5);
        control(peer,"unknownThread");QVERIFY(peer.store.openComments("90"));QTRY_VERIFY(peer.store.inComments());QCOMPARE(peer.store.currentChatId(),QString("-1001234567800"));QVERIFY(peer.store.composerVisible());QVERIFY(peer.store.canSend());
        QVERIFY(peer.store.returnToPost());QCOMPARE(peer.store.currentChatId(),C);QCOMPARE(peer.store.targetMessageId(),QString("90"));QVERIFY(!peer.store.composerVisible());
    }
    void deniedAndTimedOutCommentsAreRetryable(){
        Peer peer(200);ready(peer);open(peer);QVariantMap event=object("updateNewMessage"),message=object("message");message.insert("id",999);message.insert("chat_id",A.toLongLong());message.insert("content",content("no thread"));event.insert("message",message);update(peer,event);
        QVERIFY(peer.store.openComments("999"));QTRY_VERIFY(!peer.store.commentsLoading());QVERIFY(!peer.store.inComments());QVERIFY(!peer.store.errorText().isEmpty());
        control(peer,"holdThread");QVERIFY(peer.store.openComments("30"));QTRY_VERIFY(!peer.store.commentsLoading());QVERIFY(!peer.store.inComments());control(peer,"releaseThread");QVERIFY(!peer.store.inComments());QVERIFY(peer.store.openComments("30"));QTRY_VERIFY(peer.store.inComments());
    }
    void threadDraftJournalSurvivesRestartWithoutReplacingMainDraft(){
        QTemporaryDir dir;LocalOutbox first(dir.path());QVERIFY(first.open());QVERIFY(first.rememberDraft("42",A,"main"));QVERIFY(first.rememberDraft("42",A+"/thread/300","comment"));
        QVariantMap intent;intent.insert("type","text");intent.insert("text","comment");intent.insert("threadId","300");QVERIFY(!first.prepare("42",A,intent).isEmpty());
        LocalOutbox restarted(dir.path());QVERIFY(restarted.open());QCOMPARE(restarted.draft("42",A).toString(),QString("main"));QCOMPARE(restarted.draft("42",A+"/thread/300").toString(),QString("comment"));
        QVERIFY(restarted.latest("42",A,"text",QStringList()<<"draft").isEmpty());QVERIFY(!restarted.latest("42",A,"text",QStringList()<<"draft","300").isEmpty());
        QVariantMap attachment;attachment.insert("type","attachment");attachment.insert("kind","document");attachment.insert("path",payload(dir.path(),"normal.txt"));attachment.insert("caption","main file");
        const QString mainFile=restarted.prepare("42",A,attachment);QVERIFY(!mainFile.isEmpty());
        attachment.insert("threadId","300");attachment.insert("path",payload(dir.path(),"thread.txt"));attachment.insert("caption","thread file");const QString threadFile=restarted.prepare("42",A,attachment);QVERIFY(!threadFile.isEmpty());QVERIFY(mainFile!=threadFile);
        QVERIFY(restarted.rememberDraft("42",A+"/thread/300","new thread caption"));QCOMPARE(restarted.entry(mainFile).value("caption").toString(),QString("main file"));QCOMPARE(restarted.entry(threadFile).value("caption").toString(),QString("new thread caption"));
        QVERIFY(restarted.removeDraft("42",A,"300"));QVERIFY(restarted.entry(threadFile).isEmpty());QVERIFY(!restarted.entry(mainFile).isEmpty());
    }
    void cancelledDownloadsIgnoreOldRepliesAndCanBeRetried(){
        Peer peer;ready(peer);open(peer);QVERIFY(!control(peer,"holdDownload").isEmpty());QVERIFY(peer.store.downloadMedia("20"));
        QCOMPARE(peer.store.downloads().size(),1);QVERIFY(peer.store.downloads().first().toMap().value("active").toBool());
        QVERIFY(peer.store.cancelDownload(7));QVERIFY(!peer.store.retryDownload(7));
        QTRY_VERIFY(!peer.store.downloads().first().toMap().value("cancelPending").toBool());
        QVERIFY(!control(peer,"releaseDownload").isEmpty());
        QVERIFY(!peer.store.downloads().first().toMap().value("active").toBool());QVERIFY(peer.store.retryDownload(7));
        QTRY_VERIFY(!peer.store.downloads().first().toMap().value("path").toString().isEmpty());
        QCOMPARE(control(peer,"stats").value("send_count").toInt(),0);
    }
    void reconnectRestoresRuntimeConfigurationWithoutSending(){
        Peer peer;ready(peer);open(peer);peer.store.setDraft("Keep across reconnect");peer.store.flushDrafts();
        peer.session.reconnect();QTRY_COMPARE(peer.session.phase(),QString("ready"));QTRY_VERIFY(!peer.store.chats().isEmpty());open(peer);
        QCOMPARE(peer.store.draftText(),QString("Keep across reconnect"));QCOMPARE(control(peer,"stats").value("send_count").toInt(),0);
    }
    void scheduledMessagesCanBeEditedRescheduledAndCancelled(){
        Peer peer;ready(peer);open(peer);peer.store.loadScheduled();QTRY_VERIFY(!peer.store.scheduledBusy());QCOMPARE(peer.store.scheduledMessages().size(),1);
        QVERIFY(!peer.store.changeScheduled("999","wrong",0,true));
        QVERIFY(!peer.store.changeScheduled("7100",QString(),QDateTime::currentDateTime().toTime_t()-1,false));
        QVERIFY(peer.store.changeScheduled("7100","Changed later",0,true));QTRY_VERIFY(!peer.store.scheduledBusy());QCOMPARE(peer.store.scheduledMessages().first().toMap().value("text").toString(),QString("Changed later"));
        const uint date=QDateTime::currentDateTime().toTime_t()+7200;
        QVERIFY(peer.store.changeScheduled("7100",QString(),date,false));QTRY_VERIFY(!peer.store.scheduledBusy());QCOMPARE(peer.store.scheduledMessages().first().toMap().value("sendDate").toUInt(),date);
        QVERIFY(peer.store.cancelScheduled("7100"));QTRY_VERIFY(!peer.store.scheduledBusy());QVERIFY(peer.store.scheduledMessages().isEmpty());
        QCOMPARE(control(peer,"stats").value("send_count").toInt(),0);
        open(peer,B);QVERIFY(peer.store.scheduledMessages().isEmpty());QVERIFY(!peer.store.cancelScheduled("7100"));
    }
    void timedMutePreservesOtherNotificationSettings(){
        Peer peer;ready(peer);open(peer);QVERIFY(!peer.store.muteFor(-1));QVERIFY(peer.store.muteFor(3600));
        QTRY_COMPARE(peer.store.chatInfo().value("notificationsOn").toBool(),false);
        QVERIFY(peer.store.muteFor(0));QTRY_COMPARE(peer.store.chatInfo().value("notificationsOn").toBool(),true);
    }
    void diagnosticsContainsCountersWithoutMessageText(){
        Peer peer;ready(peer);open(peer);peer.store.setDraft("PRIVATE-DRAFT-NEVER-REPORT");
        const QString report=peer.store.diagnostics()+peer.session.diagnostics();
        QVERIFY(!report.contains("PRIVATE-DRAFT"));QVERIFY(!report.contains(A));QVERIFY(!report.contains(QString(32,'a')));QVERIFY(!report.isEmpty());
    }
    void burstUpdatesAreBatchedAndAllMessagesArrive(){
        Peer peer;ready(peer);open(peer);
        QSignalSpy changed(&peer.store,SIGNAL(messagesChanged()));
        QVERIFY(!control(peer,"messageBurst").isEmpty());
        QTRY_VERIFY(!row(peer.store,"10599").isEmpty());
        for(int i=0;i<600;++i)QVERIFY(!row(peer.store,QString::number(10000+i)).isEmpty());
        QVERIFY2(changed.size()<150,"A burst must not rebuild the message list for every update");
        open(peer,B);open(peer,A);QVERIFY(row(peer.store,"10599").isEmpty());
    }
    void expiredSendsDoNotBlockReadingHistory(){
        Peer peer(100);ready(peer);open(peer);
        for(int i=0;i<64;++i){QVERIFY(peer.store.sendText("timeout"));QTRY_VERIFY(!peer.store.sendBusy());}
        open(peer,B);QVERIFY(!peer.store.loadingHistory());
        QCOMPARE(control(peer,"stats").value("send_count").toInt(),64);
    }
    void longSessionKeepsRecentWindowAndPreservesOlderReading(){
        Peer peer;ready(peer);open(peer);
        for(int i=0;i<12;++i){QVariantMap fields;fields.insert("base",i*600);QVERIFY(!control(peer,"messageBurst",fields).isEmpty());}
        QVERIFY(peer.store.messages().size()<=1000);
        QVERIFY(!row(peer.store,"17199").isEmpty());
        QVERIFY(peer.store.canLoadHistory());
        peer.store.setFollowingLatest(false);
        for(int i=12;i<15;++i){QVariantMap fields;fields.insert("base",i*600);QVERIFY(!control(peer,"messageBurst",fields).isEmpty());}
        QVERIFY(peer.store.messages().size()>1000);
        QVERIFY(!row(peer.store,"17199").isEmpty());
    }
    void reconnectResumesHistoryAfterLocalCacheEnds(){
        Peer peer;ready(peer);open(peer);
        QVariantMap event=object("updateConnectionState");event.insert("state",object("connectionStateWaitingForNetwork"));
        QVERIFY(!control(peer,"emit",event).isEmpty());
        peer.store.loadMoreHistory();QTRY_VERIFY(!peer.store.loadingHistory());
        peer.store.loadMoreHistory();QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!peer.store.canLoadHistory());
        event.insert("state",object("connectionStateReady"));QVERIFY(!control(peer,"emit",event).isEmpty());
        QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!control(peer,"stats").value("last_only_local").toBool());
    }
    void lateLocalHistoryAfterReconnectStillRequestsNetwork(){
        Peer peer;ready(peer);open(peer);
        peer.store.loadMoreHistory();QTRY_VERIFY(!peer.store.loadingHistory());
        QVariantMap event=object("updateConnectionState");event.insert("state",object("connectionStateWaitingForNetwork"));
        QVERIFY(!control(peer,"emit",event).isEmpty());
        QVERIFY(!control(peer,"holdHistory").isEmpty());peer.store.loadMoreHistory();
        QVERIFY(peer.store.loadingHistory());
        event.insert("state",object("connectionStateReady"));QVERIFY(!control(peer,"emit",event).isEmpty());
        QVERIFY(!control(peer,"releaseHistory").isEmpty());QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!control(peer,"stats").value("last_only_local").toBool());
    }
    void scheduledSendStaysOutOfHistoryAndRetainsRejectedDraft(){
        Peer peer;ready(peer);open(peer);
        const uint now=QDateTime::currentDateTime().toTime_t();
        QVERIFY(!peer.store.sendText("invalid",now-1));
        QVERIFY(!peer.store.sendText("too far",now+367U*86400U));
        QVERIFY(peer.store.sendText("scheduled fixture",now+3600));
        QTRY_VERIFY(!peer.store.sendBusy());
        QTRY_VERIFY(peer.store.errorText().contains(QString::fromUtf8("запланировано")));
        QVERIFY(row(peer.store,"7000").isEmpty());QVERIFY(row(peer.store,"7001").isEmpty());
        QVERIFY(peer.store.sendText("scheduled-deny",now+3600));
        QTRY_VERIFY(!peer.store.sendBusy());
        QTRY_COMPARE(peer.store.failedDraft(),QString("scheduled-deny"));
    }
    void reactionsRespectAvailabilityAndShowWhoReacted(){
        Peer peer;ready(peer);peer.store.loadContacts();QTRY_VERIFY(!peer.store.contactsLoading());open(peer);
        QVERIFY(peer.store.beginReactions("30"));QTRY_COMPARE(peer.store.reactionState().value("state").toString(),QString("ready"));
        QCOMPARE(peer.store.reactionState().value("options").toList().size(),2);QVERIFY(!peer.store.chooseReaction(QString::fromUtf8("❤️")));QVERIFY(!peer.store.chooseReaction(QString::fromUtf8("🔥")));
        QVERIFY(peer.store.chooseReaction(QString::fromUtf8("👍")));QTRY_COMPARE(peer.store.reactionState().value("state").toString(),QString("ready"));
        QTRY_VERIFY(row(peer.store,"30").value("reactionDisplay").toString().contains(QString::fromUtf8("👍 1")));
        QVERIFY(peer.store.reactionState().value("options").toList().first().toMap().value("chosen").toBool());
        QVERIFY(peer.store.loadReactors(false));QTRY_VERIFY(!peer.store.reactionState().value("peopleLoading").toBool());QVERIFY(peer.store.reactionState().value("names").toString().contains(QString::fromUtf8("Анна")));
        QVERIFY(peer.store.chooseReaction(QString::fromUtf8("👍")));QTRY_VERIFY(row(peer.store,"30").value("reactionDisplay").toString().isEmpty());
        peer.store.cancelReactions();QVERIFY(peer.store.reactionState().isEmpty());
    }
    void pinnedMessagesTrackUpdatesAndChatSwitches(){
        Peer peer;ready(peer);QVERIFY(!control(peer,"pinFixture").isEmpty());open(peer,A);
        QTRY_COMPARE(peer.store.pinnedMessage().value("messageId").toString(),QString("30"));
        QCOMPARE(peer.store.pinnedMessage().value("preview").toString(),QString("Pinned fixture"));
        open(peer,B);QTRY_VERIFY(peer.store.pinnedMessage().isEmpty());
        open(peer,A);QTRY_COMPARE(peer.store.pinnedMessage().value("messageId").toString(),QString("30"));
        QVERIFY(!control(peer,"unpinFixture").isEmpty());QVariantMap event=object("updateMessageIsPinned");event.insert("chat_id",A.toLongLong());event.insert("message_id",30);event.insert("is_pinned",false);update(peer,event);QTRY_VERIFY(peer.store.pinnedMessage().isEmpty());
    }
    void burstOfMessagesKeepsHistoryConsistent(){
        Peer peer;ready(peer);open(peer);QElapsedTimer timer;timer.start();
        for(int id=10000;id<10300;++id){QVariantMap event=object("updateNewMessage"),message=object("message");message.insert("chat_id",A.toLongLong());message.insert("id",id);message.insert("date",1700000000+id);message.insert("content",content(QString::fromUtf8("Проверка 😀")));event.insert("message",message);peer.store.consume(event);}
        QCOMPARE(peer.store.messages().size(),303);QVERIFY(!row(peer.store,"10299").isEmpty());QVERIFY(timer.elapsed()<30000);
        QVariantMap removal=object("updateDeleteMessages");removal.insert("chat_id",A.toLongLong());QVariantList ids;for(int id=10000;id<10050;++id)ids<<id;removal.insert("message_ids",ids);peer.store.consume(removal);QCOMPARE(peer.store.messages().size(),253);
    }
    void storageBreakdownUsesTelegramStatistics(){
        Peer peer;ready(peer);peer.store.refreshStorage();QTRY_VERIFY(peer.store.storageSummary().contains(QString::fromUtf8("2,0")) || peer.store.storageSummary().contains("2.0"));
        QVERIFY(peer.store.storageSummary().contains(QString::fromUtf8("документы")));QVERIFY(peer.store.storageSummary().contains("3.0") || peer.store.storageSummary().contains("3,0"));
    }
    void savedViewRestoresForTheSameAccountAndChat(){
        QTemporaryDir directory;
        {Peer peer(60000,directory.path());ready(peer);open(peer);peer.store.setDraft(QString::fromUtf8("abc😀def"));peer.store.flushDrafts();peer.store.saveViewState("20",3);peer.store.closeChat();}
        Peer restored(60000,directory.path());ready(restored);QVERIFY(restored.store.openChat(A));QTRY_VERIFY(!restored.store.loadingHistory());QCOMPARE(restored.store.targetMessageId(),QString("20"));QCOMPARE(restored.store.draftCursor(),3);QVERIFY(restored.store.targetMessageIndex()>=0);
        open(restored,B);QVERIFY(restored.store.targetMessageId().isEmpty());open(restored,A);QCOMPARE(restored.store.targetMessageId(),QString("20"));restored.store.clearViewPosition();open(restored,A);QVERIFY(restored.store.targetMessageId().isEmpty());
    }
    void formattedTextEscapesMarkupAndKeepsEmoji(){
        const QString text=QString::fromUtf8("A <b> 😀 Б");QVariantMap formatted;formatted.insert("text",text);
        QVariantMap bold=object("textEntity"),type=object("textEntityTypeBold");bold.insert("offset",2);bold.insert("length",3);bold.insert("type",type);
        QVariantMap broken=object("textEntity");broken.insert("offset",7);broken.insert("length",1);broken.insert("type",type);
        formatted.insert("entities",QVariantList()<<bold<<broken);const QString html=FormattedText::html(text,0,formatted);
        QVERIFY(html.contains("&lt;b&gt;"));QVERIFY(!html.contains("<b><b>"));QVERIFY(html.contains(QString::fromUtf8("😀")));
    }
    void messageLinksAreExtractedAndUnsafeSchemesRejected(){
        Peer peer;ready(peer);open(peer);QVariantMap event=object("updateNewMessage"),message=object("message");message.insert("chat_id",A.toLongLong());message.insert("id",81);message.insert("content",content("hello https://example.org/test"));event.insert("message",message);update(peer,event);
        QCOMPARE(row(peer.store,"81").value("link").toString(),QString("https://example.org/test"));
        message.insert("id",82);QVariantMap text=content("click"),formatted=text.value("text").toMap(),entity=object("textEntity"),kind=object("textEntityTypeTextUrl");kind.insert("url","javascript:alert(1)");entity.insert("offset",0);entity.insert("length",5);entity.insert("type",kind);formatted.insert("entities",QVariantList()<<entity);text.insert("text",formatted);message.insert("content",text);event.insert("message",message);update(peer,event);QVERIFY(row(peer.store,"82").value("link").toString().isEmpty());
    }
    void readOnlyChannelsHideComposerAndPublishersKeepIt() {
        Peer peer;ready(peer);
        QVariantMap source=object("chat");source.insert("id",C.toLongLong());source.insert("title","Channel");
        QVariantMap type=object("chatTypeSupergroup");type.insert("is_channel",true);type.insert("supergroup_id",1234);source.insert("type",type);
        QVariantMap permissions=object("chatPermissions");permissions.insert("can_send_basic_messages",false);source.insert("permissions",permissions);
        QVariantMap updateChat=object("updateNewChat");updateChat.insert("chat",source);peer.store.consume(updateChat);
        QVERIFY(peer.store.openChat(C));QVERIFY(!peer.store.composerVisible());
        permissions.insert("can_send_basic_messages",true);QVariantMap changed=object("updateChatPermissions");changed.insert("permissions",permissions);update(peer,changed,C);
        QVERIFY(peer.store.composerVisible());
    }
    void contactsUseTelegramUsersSearchAndOpenWithoutSending() {
        Peer peer;ready(peer);peer.store.loadContacts();QTRY_VERIFY(!peer.store.contactsLoading());QTRY_COMPARE(peer.store.contacts().size(),3);
        peer.store.setContactQuery("@ANNA");QCOMPARE(peer.store.contacts().size(),1);QCOMPARE(peer.store.contacts().first().toMap().value("userId").toString(),QString("42"));
        QSignalSpy readyChat(&peer.store,SIGNAL(contactChatReady(QString)));
        QVERIFY(peer.store.openContact("42"));QVERIFY(!peer.store.openContact("42"));QTRY_COMPARE(readyChat.size(),1);
        const QString id=readyChat.first().first().toString();QCOMPARE(id,QString("420042"));QVERIFY(!peer.store.chat(id).isEmpty());
        QCOMPARE(peer.store.chat(id).value("peerId").toString(),QString("42"));
        QVERIFY(peer.store.openChat(id));QTRY_VERIFY(!peer.store.loadingHistory());QCOMPARE(peer.store.currentChatStatus(),QString::fromUtf8("онлайн"));
        QCOMPARE(control(peer,"stats").value("send_count").toInt(),0);
        peer.store.setContactQuery(QString());peer.store.setContactSection(1);QCOMPARE(peer.store.contacts().size(),1); // Only known main-list private chats.
    }
    void cancelledContactRequestCannotOpenAChatLater() {
        Peer peer;ready(peer);peer.store.loadContacts();QTRY_VERIFY(!peer.store.contactsLoading());
        QVERIFY(!control(peer,"holdContact").isEmpty());QSignalSpy chats(&peer.store,SIGNAL(contactChatReady(QString)));
        QVERIFY(peer.store.openContact("42"));peer.store.cancelContactOpen();QVERIFY(!control(peer,"releaseContact").isEmpty());QTest::qWait(50);QCOMPARE(chats.size(),0);QVERIFY(!peer.store.openingContact());
    }
    void presenceRespectsPrivacyAndOnlineExpiry() {
        QVariantMap user;QVariantMap status=object("userStatusOnline");status.insert("expires",1001);user.insert("status",status);
        QCOMPARE(Presence::text(user,1000),QString::fromUtf8("онлайн"));QVERIFY(Presence::active(status,1000));QVERIFY(!Presence::active(status,1001));
        QCOMPARE(Presence::text(user,1001),QString::fromUtf8("недавно в сети"));
        user.insert("status",object("userStatusEmpty"));QCOMPARE(Presence::text(user),QString::fromUtf8("статус недоступен"));
        user.insert("status",object("userStatusLastMonth"));QVERIFY(Presence::text(user).contains(QString::fromUtf8("месяце")));
        user.insert("bot",true);QCOMPARE(Presence::text(user),QString::fromUtf8("бот"));
    }
    void typingUsesSenderUpdatesCancelAndExpires() {
        Peer peer;ready(peer);open(peer);QVariantMap typing=object("updateChatAction"),sender=object("messageSenderUser");sender.insert("user_id",42);
        typing.insert("sender_id",sender);typing.insert("action",object("chatActionTyping"));update(peer,typing);
        QVERIFY(peer.store.currentChatStatus().contains(QString::fromUtf8("печатает")));QVERIFY(peer.store.currentStatusActive());
        QVERIFY(peer.store.chats().first().toMap().value("typingText").toString().contains(QString::fromUtf8("печатает")));
        typing.insert("action",object("chatActionCancel"));update(peer,typing);QCOMPARE(peer.store.currentChatStatus(),QString::fromUtf8("группа"));
        typing.insert("action",object("chatActionTyping"));update(peer,typing);QTest::qWait(6200);QTRY_COMPARE(peer.store.currentChatStatus(),QString::fromUtf8("группа"));
    }
    void avatarsAreLazyBoundedCachedAndInvalidatedWhenRemoved() {
        Peer peer;ready(peer);peer.store.loadContacts();QTRY_VERIFY(!peer.store.contactsLoading());
        QCOMPARE(control(peer,"stats").value("people_calls").toList().size(),2); // Contacts + the missing user, no eager image downloads.
        peer.store.requestAvatar("user:42");peer.store.requestAvatar("user:42");
        QTRY_VERIFY(!peer.store.avatarPath("user:42").isEmpty());QVERIFY(peer.store.avatarPath("user:42").startsWith(peer.session.dataDirectory()+"/files/"));
        const QString cachedPath=peer.store.avatarPath("user:42");
        QVariantMap stale=object("updateFile"),staleFile=object("file");staleFile.insert("id",77);QVariantMap local;local.insert("is_downloading_completed",false);staleFile.insert("local",local);stale.insert("file",staleFile);peer.store.consume(stale);
        QCOMPARE(peer.store.avatarPath("user:42"),cachedPath);
        const int calls=control(peer,"stats").value("people_calls").toList().size();
        peer.store.requestAvatar("user:42");QTest::qWait(80);QCOMPARE(control(peer,"stats").value("people_calls").toList().size(),calls);
        QVariantMap user=object("user");user.insert("id",42);user.insert("first_name","Anna");user.insert("is_contact",true);user.insert("profile_photo",QVariant());QVariantMap event=object("updateUser");event.insert("user",user);update(peer,event);
        QVERIFY(peer.store.avatarPath("user:42").isEmpty());
        QVariantMap file=object("file");file.insert("id",90);file.insert("size",2*1024*1024);QVariantMap photo;photo.insert("small",file);user.insert("profile_photo",photo);event.insert("user",user);update(peer,event);
        peer.store.requestAvatar("user:42");QTest::qWait(50);QCOMPARE(control(peer,"stats").value("people_calls").toList().size(),calls);
    }
    void autoPagerWaitsForLayoutAndDoesNotSpinOrLoadHiddenPages() {
        AutoPager pager(0,40);QSignalSpy requests(&pager,SIGNAL(requestMore()));
        pager.setEnabled(true);pager.setAtEdge(true);pager.setContentCount(1);
        pager.setAtEdge(false);QTest::qWait(60);QCOMPARE(requests.size(),0);
        pager.setAtEdge(true);QTRY_COMPARE(requests.size(),1);
        QTest::qWait(100);QCOMPARE(requests.size(),1);
        pager.setContentCount(2);pager.setEnabled(false);QTest::qWait(60);QCOMPARE(requests.size(),1);
        pager.setEnabled(true);QTRY_COMPARE(requests.size(),2);
        pager.setEnabled(false);pager.setContentCount(3);QTest::qWait(60);QCOMPARE(requests.size(),2);
    }
    void replyQuoteAndEditRequireServerPermissionAndKeepDraft() {
        Peer peer;ready(peer);open(peer);
        peer.store.setDraft("ordinary draft");
        QVERIFY(peer.store.beginMessageAction("30","reply"));
        QTRY_COMPARE(peer.store.composerContext().value("mode").toString(),QString("reply"));
        QVERIFY(!peer.store.composerContext().value("replyTo").toMap().value("quote").toMap().isEmpty());
        QVERIFY(peer.store.sendText("quoted answer"));QTRY_VERIFY(!peer.store.sendBusy());
        QVERIFY(peer.store.composerContext().isEmpty());
        QVariantMap incoming=object("updateNewMessage"),message=object("message");
        message.insert("chat_id",A.toLongLong());message.insert("id",50);message.insert("is_outgoing",true);message.insert("content",content("original"));incoming.insert("message",message);update(peer,incoming);
        peer.store.setDraft("keep me");
        QVERIFY(peer.store.beginMessageAction("50","edit"));QTRY_COMPARE(peer.store.composerContext().value("mode").toString(),QString("edit"));
        QVERIFY(!peer.store.canSendMedia("document"));
        peer.store.setDraft("editing must not replace ordinary draft");QCOMPARE(peer.store.draftText(),QString("keep me"));
        QVERIFY(peer.store.sendText("edited text"));QTRY_VERIFY(!peer.store.sendBusy());
        QCOMPARE(row(peer.store,"50").value("text").toString(),QString("edited text"));QVERIFY(row(peer.store,"50").value("edited").toBool());
        QCOMPARE(peer.store.draftText(),QString("keep me"));QVERIFY(peer.store.composerContext().isEmpty());
        QVERIFY(peer.store.beginMessageAction("30","edit"));QTest::qWait(80);QVERIFY(peer.store.composerContext().isEmpty());QVERIFY(!peer.store.errorText().isEmpty());
    }
    void selectionRequiresConfirmationAndPreservesOtherMessages(){
        Peer peer;ready(peer);open(peer);addOutgoing(peer);QVERIFY(peer.store.toggleSelected("30"));QVERIFY(peer.store.toggleSelected("50"));QCOMPARE(peer.store.selectionCount(),2);
        QVERIFY(!peer.store.deleteSelected(false));QVERIFY(peer.store.prepareSelection("delete"));
        QTRY_COMPARE(peer.store.selectionState().value("state").toString(),QString("ready"));
        QVERIFY(!peer.store.selectionState().value("all").toBool());QVERIFY(!peer.store.deleteSelected(true));
        QVariantList actions=control(peer,"stats").value("actions").toList();foreach(const QVariant &value,actions)QVERIFY(value.toMap().value("@type")!="deleteMessages");
        QVERIFY(peer.store.deleteSelected(false));QTRY_COMPARE(peer.store.selectionCount(),0);
        QVERIFY(row(peer.store,"30").isEmpty());QVERIFY(row(peer.store,"50").isEmpty());QVERIFY(!row(peer.store,"20").isEmpty());
    }
    void deletionForEveryoneRequiresExplicitChoiceWhenLocalDeleteIsForbidden(){
        Peer peer;ready(peer);open(peer);addOutgoing(peer);
        QVariantMap fixture=object("fixtureOnlyAllDelete");QVERIFY(!peer.session.sendReady(fixture).isEmpty());
        QVERIFY(peer.store.toggleSelected("50"));QVERIFY(peer.store.prepareSelection("delete"));QTRY_COMPARE(peer.store.selectionState().value("state").toString(),QString("ready"));
        QVERIFY(!peer.store.selectionState().value("self").toBool());QVERIFY(peer.store.selectionState().value("all").toBool());QVERIFY(!peer.store.deleteSelected(false));
        QVERIFY(peer.store.deleteSelected(true));QTRY_COMPARE(peer.store.selectionCount(),0);
    }
    void selectionCancelsStaleChecksAndClearsOnChatChange(){
        Peer peer;ready(peer);open(peer);addOutgoing(peer);QVERIFY(peer.store.toggleSelected("50"));QVERIFY(peer.store.prepareSelection("delete"));peer.store.clearSelection();QTest::qWait(70);
        QVERIFY(peer.store.selectionState().isEmpty());QVERIFY(!peer.store.deleteSelected(true));
        QVERIFY(peer.store.toggleSelected("30"));open(peer,B);QCOMPARE(peer.store.selectionCount(),0);
    }
    void selectedForwardHasOneJournalRecordPerMessage(){
        Peer peer;ready(peer);open(peer);addOutgoing(peer);QVERIFY(peer.store.toggleSelected("50"));QVERIFY(peer.store.toggleSelected("30"));QVERIFY(peer.store.prepareSelection("forward"));
        QTRY_COMPARE(peer.store.forwardSelection().value("state").toString(),QString("ready"));
        QCOMPARE(peer.store.forwardSelection().value("messageIds").toStringList(),QStringList()<<"30"<<"50");
        QVERIFY(peer.store.forwardTo(B));QTRY_COMPARE(peer.store.forwardSelection().value("state").toString(),QString("sent"));
        LocalOutbox journal(peer.session.dataDirectory());QVERIFY(journal.open());int sent=0;foreach(const QVariant &value,journal.entries("9007199254740990"))if(value.toMap().value("type")=="forward" && value.toMap().value("state")=="sent")++sent;QCOMPARE(sent,2);
    }
    void backgroundChatDoesNotAcknowledgeUnreadMessages(){
        Peer peer;ready(peer);peer.store.setForeground(false);open(peer);QCOMPARE(peer.store.chat(A).value("unreadCount").toInt(),3);QVERIFY(!peer.store.markRead());peer.store.setForeground(true);QTRY_COMPARE(peer.store.chat(A).value("unreadCount").toInt(),0);
    }
    void notificationPolicyDeduplicatesHonorsPreviewAndBadge(){
        NotificationPolicy policy;QVariantMap count=object("updateUnreadMessageCount");count.insert("chat_list",object("chatListMain"));count.insert("unread_unmuted_count",3);policy.consume(count,true,A);QCOMPARE(policy.unread,3);
        count.insert("chat_list",object("chatListArchive"));count.insert("unread_unmuted_count",2);policy.consume(count,true,A);QCOMPARE(policy.unread,5);
        QVariantMap type=object("notificationTypeNewMessage"),message=object("message");message.insert("content",content("private text"));type.insert("message",message);type.insert("show_preview",false);
        QVariantMap notice;notice.insert("id",1);notice.insert("type",type);QVariantMap event=object("updateNotificationGroup");event.insert("notification_group_id",7);event.insert("chat_id",A.toLongLong());event.insert("added_notifications",QVariantList()<<notice);
        QVariantList actions=policy.consume(event,false,A);QCOMPARE(actions.size(),1);QVERIFY(!actions.first().toMap().value("body").toString().contains("private text"));QVERIFY(policy.consume(event,false,A).isEmpty());
        policy.reset();QVERIFY(policy.consume(event,true,A).isEmpty());policy.reset();type.insert("show_preview",true);notice.insert("type",type);event.insert("added_notifications",QVariantList()<<notice);QCOMPARE(policy.consume(event,true,B).first().toMap().value("body").toString(),QString("private text"));
        event.remove("added_notifications");event.insert("removed_notification_ids",QVariantList()<<1);QVERIFY(policy.consume(event,false,A).first().toMap().value("remove").toBool());
    }
    void forwardIsExplicitJournaledAndKeepsOriginalReference() {
        Peer peer;ready(peer);open(peer);
        QVERIFY(peer.store.beginMessageAction("30","forward"));QTRY_COMPARE(peer.store.forwardSelection().value("state").toString(),QString("ready"));
        QCOMPARE(control(peer,"stats").value("actions").toList().size(),1);
        QVERIFY(peer.store.forwardTo(B));QVERIFY(!peer.store.forwardTo(B));
        QTRY_COMPARE(peer.store.forwardSelection().value("state").toString(),QString("sent"));
        LocalOutbox journal(peer.session.dataDirectory());QVERIFY(journal.open());
        const QVariantMap sent=journal.latest("9007199254740990",B,"forward",QStringList()<<"sent");QVERIFY(!sent.isEmpty());
        QCOMPARE(sent.value("sourceChatId").toString(),A);QCOMPARE(sent.value("sourceMessageId").toString(),QString("30"));
    }
    void forwardingSuccessBeforeResponseNeverBecomesPendingAgain() {
        Peer peer;ready(peer);open(peer);QVERIFY(!control(peer,"forwardRace").isEmpty());
        QVERIFY(peer.store.beginMessageAction("30","forward"));QTRY_COMPARE(peer.store.forwardSelection().value("state").toString(),QString("ready"));
        QVERIFY(peer.store.forwardTo(B));QTRY_COMPARE(peer.store.forwardSelection().value("state").toString(),QString("sent"));
        LocalOutbox journal(peer.session.dataDirectory());QVERIFY(journal.open());
        QCOMPARE(journal.latest("9007199254740990",B,"forward",QStringList()<<"sent").value("messageId").toString(),QString("8801"));
        QVERIFY(!peer.store.forwardTo(B));
    }
    void quoteUsesUpdatedTextAndClipsFormattingAtItsBoundary() {
        Peer peer;ready(peer);open(peer);
        QVariantMap changed=object("updateMessageContent");changed.insert("message_id",30);
        QVariantMap next=content(QString(140,'x')),formatted=next.value("text").toMap();
        QVariantMap bold=object("textEntity");bold.insert("offset",0);bold.insert("length",140);bold.insert("type",object("textEntityTypeBold"));
        QVariantMap date=object("textEntity");date.insert("offset",120);date.insert("length",20);date.insert("type",object("textEntityTypeDateTime"));
        formatted.insert("entities",QVariantList()<<bold<<date);next.insert("text",formatted);changed.insert("new_content",next);update(peer,changed);
        QVERIFY(peer.store.beginMessageAction("30","reply"));QTRY_COMPARE(peer.store.composerContext().value("mode").toString(),QString("reply"));
        const QVariantMap quote=peer.store.composerContext().value("replyTo").toMap().value("quote").toMap().value("text").toMap();
        QCOMPARE(quote.value("text").toString(),QString(120,'x'));QCOMPARE(quote.value("entities").toList().first().toMap().value("length").toInt(),120);
        QVERIFY(peer.store.openChat(A));QVERIFY(!peer.store.composerContext().isEmpty());peer.store.cancelComposer();QVERIFY(peer.store.composerContext().isEmpty());
    }
    void chatInfoMembersMediaNotificationsAndArchiveUseExactApi() {
        Peer peer;ready(peer);open(peer);peer.store.loadChatInfo();
        QTRY_COMPARE(peer.store.chatInfo().value("description").toString(),QString("Synthetic group description"));
        QTRY_VERIFY(peer.store.chatInfo().value("notificationsKnown").toBool());
        QVERIFY(peer.store.setNotifications(false));QTRY_VERIFY(!peer.store.chatInfo().value("notificationsOn").toBool());
        peer.store.setInfoSection(1);QTRY_VERIFY(!peer.store.infoLoading());QCOMPARE(peer.store.infoRows().size(),2);
        peer.store.setInfoSection(2);QTRY_VERIFY(!peer.store.infoLoading());QCOMPARE(peer.store.infoRows().size(),1);QVERIFY(!peer.store.canLoadInfo());
        QCOMPARE(peer.store.infoRows().first().toMap().value("messageId").toString(),QString("20"));
        peer.store.setInfoSection(3);QTRY_VERIFY(!peer.store.infoLoading());QCOMPARE(peer.store.infoRows().size(),1);
        QVERIFY(peer.store.setPinned(A,true));QTRY_VERIFY(peer.store.chatInfo().value("pinned").toBool());
        QVERIFY(peer.store.setArchived(A,true));QTRY_VERIFY(peer.store.chatInfo().value("archived").toBool());
        QCOMPARE(peer.store.chats().size(),2);peer.store.setArchiveView(true);QCOMPARE(peer.store.chats().size(),1);
        QVERIFY(peer.store.setArchived(A,false));QTRY_VERIFY(!peer.store.chatInfo().value("archived").toBool());
        QCOMPARE(peer.store.chats().size(),0);peer.store.setArchiveView(false);QCOMPARE(peer.store.chats().size(),3);
    }
    void recipientPagingIncludesArchiveWithoutSwitchingTheVisibleList() {
        Peer peer;ready(peer);peer.store.setArchiveView(true);QVERIFY(peer.store.archiveView());
        QVERIFY(peer.store.canLoadRecipientChats());peer.store.loadMoreRecipientChats();QTRY_VERIFY(!peer.store.loadingChats());
        QVERIFY(peer.store.archiveView());peer.store.loadMoreRecipientChats();QTRY_VERIFY(!peer.store.loadingChats());
        QCOMPARE(peer.store.chats().size(),1);QCOMPARE(peer.store.chats().first().toMap().value("chatId").toString(),QString("44"));
        peer.store.loadMoreRecipientChats();QTRY_VERIFY(!peer.store.loadingChats());QVERIFY(!peer.store.canLoadRecipientChats());QVERIFY(peer.store.archiveView());
    }
    void incomingEventsAreDeduplicatedAndNeverRequestHistoryJump() {
        Peer peer;ready(peer);open(peer);QSignalSpy incoming(&peer.store,SIGNAL(incomingMessage())),scroll(&peer.store,SIGNAL(scrollRequested()));
        QVariantMap event=object("updateNewMessage"),message=object("message");message.insert("id",777);message.insert("chat_id",A.toLongLong());message.insert("is_outgoing",false);message.insert("content",content("new incoming"));event.insert("message",message);
        update(peer,event);QCOMPARE(incoming.size(),1);QCOMPARE(scroll.size(),0);
        update(peer,event);QCOMPARE(incoming.size(),1);QCOMPARE(scroll.size(),0);
        peer.store.closeChat();update(peer,event);QCOMPARE(incoming.size(),1);
    }
    void chatTabsSeparatePrivateAndCommunityWithoutLosingRows() {
        Peer peer;ready(peer);
        QCOMPARE(peer.store.personalCount(),1);QCOMPARE(peer.store.communityCount(),2);
        peer.store.setChatCategory("personal");QCOMPARE(peer.store.chats().size(),1);
        QCOMPARE(peer.store.chats().first().toMap().value("chatId").toString(),B);
        peer.store.setChatCategory("community");QCOMPARE(peer.store.chats().size(),2);
        QCOMPARE(peer.store.knownChats().size(),4);
        peer.store.setChatCategory("personal");
        QVariantMap event=object("updateChatTitle");event.insert("title","Renamed group");update(peer,event,A);
        QCOMPARE(peer.store.chats().size(),1);
        peer.store.setChatCategory("community");QCOMPARE(peer.store.chats().first().toMap().value("title").toString(),QString("Renamed group"));
    }
    void searchMergesServerChatsAndIgnoresLatePreviousQuery() {
        Peer peer;ready(peer);peer.store.setChatCategory("personal");
        TelegramSearch search(&peer.session,&peer.store);
        search.search("slow",1);QTest::qWait(450);QVERIFY(search.loading());
        search.search("needle",0);QTRY_VERIFY(!search.loading());QCOMPARE(search.rows().size(),2);
        QVERIFY(!control(peer,"releaseSearch").isEmpty());QTest::qWait(50);
        QCOMPARE(search.rows().size(),2);
        QCOMPARE(peer.store.chats().size(),1); // Search doesn't change the selected main tab.
        search.cancel();QVERIFY(search.rows().isEmpty());QVERIFY(!search.loading());
    }
    void publicSearchHydratesUnknownChannelAndOpensWithoutJoining() {
        Peer peer;ready(peer);TelegramSearch search(&peer.session,&peer.store);
        search.search("public",1);QTRY_VERIFY(!search.loading());QCOMPARE(search.rows().size(),1);
        const QVariantMap hit=search.rows().first().toMap();
        QCOMPARE(hit.value("chatId").toString(),QString("-1001234567899"));
        QCOMPARE(hit.value("title").toString(),QString::fromUtf8("Публичный канал"));
        QVERIFY(peer.store.openChat(hit.value("chatId").toString()));QTRY_VERIFY(!peer.store.loadingHistory());
        QCOMPARE(peer.store.chats().size(),3); // Merely viewing a public chat doesn't subscribe to it.
        QVERIFY(!peer.store.canSend());
    }
    void messageSearchPagesDeduplicatesAndJumpsToExactMessage() {
        Peer peer;ready(peer);TelegramSearch search(&peer.session,&peer.store);
        search.search("needle",2);QTRY_VERIFY(!search.loading());QCOMPARE(search.rows().size(),2);
        QVERIFY(search.canLoadMore());const QVariantMap hit=search.rows().first().toMap();
        QCOMPARE(hit.value("messageId").toString(),QString("555"));
        search.loadMore();QTRY_VERIFY(!search.loading());QCOMPARE(search.rows().size(),3);QVERIFY(!search.canLoadMore());
        QVERIFY(peer.store.openChatAt(hit.value("chatId").toString(),hit.value("messageId").toString()));
        QTRY_VERIFY(!peer.store.loadingHistory());QCOMPARE(peer.store.targetMessageIndex(),1);
        QVERIFY(row(peer.store,"555").value("searchTarget").toBool());
        QCOMPARE(peer.store.targetMessageId(),QString("555"));
        peer.store.closeChat();QVERIFY(peer.store.targetMessageId().isEmpty());
    }
    void messageSearchFiltersAndErrorsAreBoundedAndRetryable() {
        Peer peer;ready(peer);TelegramSearch search(&peer.session,&peer.store,0,500);
        search.search("",2,1);QTRY_VERIFY(!search.loading());QCOMPARE(search.rows().size(),2);
        const QVariantList calls=control(peer,"stats").value("search_calls").toList();
        QCOMPARE(calls.last().toMap().value("filter").toMap().value("@type").toString(),QString("searchMessagesFilterPhoto"));
        search.search("deny",1);QTRY_VERIFY(!search.loading());QVERIFY(search.errorText().contains("429"));QVERIFY(!search.errorText().contains("private"));
        search.search("slow",1);QTRY_VERIFY(!search.loading());QVERIFY(!search.errorText().isEmpty());
        search.search("public",1);QTRY_VERIFY(!search.loading());QCOMPARE(search.rows().size(),1);QVERIFY(search.errorText().isEmpty());
        peer.session.stop();QTRY_VERIFY(!peer.store.ready());QVERIFY(search.rows().isEmpty());
    }
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
        QVERIFY(peer.store.recoveryWarning().isEmpty());
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
        QVERIFY(peer.store.recoveryWarning().isEmpty());
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
        QCOMPARE(peer.store.nextUnreadChat(), QString());
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

    void openingChatsMarksMessagesRead() {
        Peer peer; ready(peer); open(peer, A);
        QTRY_COMPARE(peer.store.chat(A).value("unreadCount").toInt(), 0);
        QCOMPARE(peer.store.nextUnreadChat(), B);
        open(peer, B);
        QTRY_COMPARE(peer.store.chat(B).value("unreadCount").toInt(), 0);
        QCOMPARE(peer.store.nextUnreadChat(), C);
        open(peer, C);
        QTRY_COMPARE(peer.store.chat(C).value("unreadCount").toInt(), 0);
        QCOMPARE(peer.store.nextUnreadChat(), QString());
        peer.store.closeChat();open(peer,A);
        QTRY_COMPARE(peer.store.chat(A).value("unreadCount").toInt(), 0);
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
        QVERIFY(peer.store.recoveryWarning().isEmpty());
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
        QVERIFY(!control(peer, "releaseSend").isEmpty());
        QTRY_VERIFY(!row(peer.store, "3001").isEmpty());
        QVERIFY(peer.store.errorText().isEmpty());
        QCOMPARE(peer.store.messages().size(), 4);
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
    }

    void backgroundPausesAutomaticHistoryAndResumesOnce() {
        Peer peer; ready(peer); open(peer);
        peer.store.setDraft("keep this draft");
        peer.store.setForeground(false);
        QVERIFY(!peer.store.canLoadHistory());
        QVERIFY(!peer.store.canLoadChats());
        QVERIFY(!peer.store.canLoadRecipientChats());
        const int before = control(peer, "stats").value("history_calls").toList().size();
        peer.store.loadMoreHistory(); peer.store.loadMoreChats();
        QCOMPARE(control(peer, "stats").value("history_calls").toList().size(), before);
        peer.store.setForeground(true);
        QVERIFY(peer.store.canLoadHistory());
        QCOMPARE(peer.store.draftText(), QString("keep this draft"));
        QCOMPARE(control(peer, "stats").value("history_calls").toList().size(), before);
    }

    void foregroundRecoversLostHistoryWithoutRestartOrDuplicateSend() {
        Peer peer; ready(peer);
        QVERIFY(!control(peer, "holdHistory").isEmpty());
        QVERIFY(peer.store.openChat(A)); QVERIFY(peer.store.loadingHistory());
        peer.store.setForeground(false);
        peer.store.setForeground(true);
        QTRY_VERIFY(!peer.store.loadingHistory());
        QCOMPARE(peer.store.messages().size(), 3);
        QCOMPARE(control(peer, "stats").value("history_calls").toList().size(), 2);
        QVERIFY(!control(peer, "releaseHistory").isEmpty());
        QCOMPARE(peer.store.messages().size(), 3);
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 0);
        peer.store.setForeground(true);
        QCOMPARE(control(peer, "stats").value("history_calls").toList().size(), 2);
    }

    void lateRetiredHistoryCannotFinishTheNewRequest() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(!control(peer, "holdHistory").isEmpty()); peer.store.loadMoreHistory();
        peer.store.setForeground(false);
        QVERIFY(!control(peer, "holdHistory").isEmpty());
        peer.store.setForeground(true); QVERIFY(peer.store.loadingHistory());
        const QVariantList calls = control(peer, "stats").value("history_calls").toList();
        QVariantMap stale = object("messages");
        stale.insert("@extra", calls.at(calls.size()-2).toMap().value("@extra"));
        stale.insert("messages", QVariantList());
        QVERIFY(!control(peer, "emit", stale).isEmpty());
        QVERIFY(peer.store.loadingHistory());
        QVERIFY(!control(peer, "releaseHistory").isEmpty());
        QTRY_VERIFY(!peer.store.loadingHistory());
        QVERIFY(!row(peer.store, "5").isEmpty());
    }

    void networkChangesAndResumeNotifyTdlibWithoutPollingReconnects() {
        Peer peer; peer.store.setNetwork("networkTypeWiFi", "network-a"); ready(peer); open(peer);
        QCOMPARE(control(peer, "stats").value("network_calls").toList().size(), 1);
        for (int i=0; i<20; ++i) peer.store.setNetwork("networkTypeWiFi", "network-a");
        QCOMPARE(control(peer, "stats").value("network_calls").toList().size(), 1);
        peer.store.setNetwork("networkTypeWiFi", "network-b");
        peer.store.setNetwork("networkTypeNone", "");
        peer.store.setNetwork("networkTypeMobile", "mobile");
        QCOMPARE(control(peer, "stats").value("network_calls").toList().size(), 4);
        peer.store.setForeground(false); peer.store.setForeground(true);
        QCOMPARE(control(peer, "stats").value("network_calls").toList().size(), 5);
        peer.store.setForeground(true);
        QCOMPARE(control(peer, "stats").value("network_calls").toList().size(), 5);
    }

    void resumeNeverRetriesUncertainMessageSends() {
        Peer peer; ready(peer); open(peer);
        QVERIFY(peer.store.sendText("timeout"));
        peer.store.setForeground(false); peer.store.setForeground(true);
        QCOMPARE(control(peer, "stats").value("send_count").toInt(), 1);
        QVERIFY(!control(peer, "releaseSend").isEmpty());
        QTRY_VERIFY(!row(peer.store, "3001").isEmpty());
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
