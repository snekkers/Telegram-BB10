#include "TelegramSession.hpp"
#include "JsonCodec.hpp"
#include "PrivateFile.hpp"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

class SessionTests : public QObject {
    Q_OBJECT
private:
    QString engine() const { return "/source/tests/fake-telegram-engine.py"; }
    void fixture(const QString &fileName, const QByteArray &scenario) {
        QFile file(fileName);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(scenario), qint64(scenario.size()));
    }
    void parameters(TelegramSession &session) {
        session.start();
        QTRY_COMPARE(session.phase(), QString("parameters"));
        QTRY_VERIFY(session.version().contains(QString::fromUtf8("Тест 😀")));
        QVERIFY(!session.configure("0", "bad"));
        QVERIFY(!session.errorText().isEmpty());
        QVERIFY(session.configure("12345", QString(32, 'a')));
        QVERIFY(!session.configure("12345", QString(32, 'a')));
    }
private slots:
    void developmentProfileRemembersExplicitChoiceWithoutSavingKeys() {
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so", data = directory.path() + "/private";
        fixture(library, "test-api");
        TelegramSession session(engine(), library, data);
        QVERIFY(!session.useTestConfiguration()); // Never starts or configures itself.
        session.start(); QTRY_COMPARE(session.phase(), QString("parameters"));
#ifdef BERRYGRAM_DEVELOPMENT_TEST_API
        QVERIFY(session.testConfigurationAvailable());
        QVERIFY(session.useTestConfiguration());
        QTRY_COMPARE(session.phase(), QString("phone"));
        QVERIFY(!session.useTestConfiguration());
        QVERIFY(session.hasSavedConfiguration());
        QVERIFY(!QFile::exists(data + "/api.json"));
        QVariantMap profile; QVERIFY(PrivateFile::read(data + "/connection.json", &profile));
        QCOMPARE(profile.value("profile").toString(), QString("development-test"));
        session.stop(); QTRY_COMPARE(session.phase(), QString("stopped"));
        session.start(); QTRY_COMPARE(session.phase(), QString("parameters"));
        QVERIFY(session.hasSavedConfiguration());
        QVERIFY(session.useSavedConfiguration()); QTRY_COMPARE(session.phase(), QString("phone"));
        QVERIFY(session.forgetConfiguration()); QVERIFY(!session.hasSavedConfiguration());
        QVERIFY(!QFile::exists(data + "/connection.json"));
#else
        QVERIFY(QDir().mkpath(data));
        QVariantMap profile; profile.insert("profile", "development-test");
        QVERIFY(PrivateFile::write(data + "/connection.json", profile));
        QVERIFY(!session.hasSavedConfiguration());
        QVERIFY(!session.useSavedConfiguration());
        QVERIFY(!session.testConfigurationAvailable());
        QVERIFY(!session.useTestConfiguration());
        QCOMPARE(session.phase(), QString("parameters"));
#endif
        session.stop(); QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void rememberedApiSurvivesRestartAndCanBeRemoved() {
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so", data = directory.path() + "/private";
        fixture(library, "full");
        TelegramSession session(engine(), library, data);
        session.start(); QTRY_COMPARE(session.phase(), QString("parameters"));
        QVERIFY(session.configureAndRemember("12345", QString(32, 'a')));
        QTRY_COMPARE(session.phase(), QString("phone"));
        QVERIFY(session.hasSavedConfiguration()); QCOMPARE(session.savedApiId(), QString("12345"));
        QFile api(data + "/api.json");
        QCOMPARE(api.permissions() & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther), QFile::Permissions());
        session.stop(); QTRY_COMPARE(session.phase(), QString("stopped"));
        session.start(); QTRY_COMPARE(session.phase(), QString("parameters"));
        QVERIFY(session.useSavedConfiguration()); QTRY_COMPARE(session.phase(), QString("phone"));
        QVERIFY(session.forgetConfiguration()); QVERIFY(!session.hasSavedConfiguration());
        QVERIFY(QFile::exists(data + "/database.key"));
        session.stop(); QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void rejectedDatabaseRestoreShowsSafeErrorAndAllowsRetry() {
        QTemporaryDir directory;
        QString library=directory.path()+"/libtdjson.so", data=directory.path()+"/private";
        fixture(library,"database-locked");
        TelegramSession session(engine(),library,data);
        session.start();QTRY_COMPARE(session.phase(),QString("parameters"));
        QVERIFY(session.configureAndRemember("12345",QString(32,'a')));
        QTRY_VERIFY(!session.busy());QCOMPARE(session.phase(),QString("parameters"));
        QVERIFY(session.errorText().contains(QString::fromUtf8("другим экземпляром")));
        QVERIFY(!session.errorText().contains("private-account-path"));
        QVERIFY(session.hasSavedConfiguration());
        QFile key(data+"/database.key");QVERIFY(key.open(QIODevice::ReadOnly));const QByteArray original=key.readAll();key.close();
        QVERIFY(session.useSavedConfiguration());QTRY_COMPARE(session.phase(),QString("phone"));
        QVERIFY(session.errorText().isEmpty());
        QVERIFY(key.open(QIODevice::ReadOnly));QCOMPARE(key.readAll(),original);key.close();
        session.stop();QTRY_COMPARE(session.phase(),QString("stopped"));
    }

    void privateFileRejectsSymlinksAndInvalidJson() {
        QTemporaryDir directory;
        QString path = directory.path() + "/private/settings.json";
        QVariantMap settings; settings.insert("value", QString::fromUtf8("Тест 😀"));
        QVERIFY(PrivateFile::write(path, settings));
        QVariantMap read; QVERIFY(PrivateFile::read(path, &read)); QCOMPARE(read, settings);
        QFile file(path); QVERIFY(file.remove());
        fixture(directory.path() + "/outside", "untouched");
        QVERIFY(QFile::link(directory.path() + "/outside", path));
        QVERIFY(!PrivateFile::read(path, &read)); QVERIFY(!PrivateFile::write(path, settings));
        QVERIFY(QFile::remove(path)); fixture(path, "{invalid"); QVERIFY(!PrivateFile::read(path, &read));
    }
    void codec() {
        QVariantMap input;
        input.insert("@type", "test");
        input.insert("text", QString::fromUtf8("  Привет 😀 \" \\ \n\t "));
        input.insert("number", 12345);
        QByteArray wire = JsonCodec::encode(input);
        QVERIFY(!wire.contains('\n'));
        QVariantMap output;
        QVERIFY(JsonCodec::decode(wire, &output));
        QCOMPARE(output, input);
        QVERIFY(!JsonCodec::decode("[]", &output));
        QVERIFY(!JsonCodec::decode("{bad", &output));
    }

    void phoneCodePassword() {
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so";
        QString data = directory.path() + "/private";
        fixture(library, "full");
        QByteArray originalLoaderPath = qgetenv("LD_LIBRARY_PATH");
        TelegramSession session(engine(), library, data);
        QVERIFY(session.available());
        parameters(session);
        QTRY_COMPARE(session.phase(), QString("phone"));
        QVERIFY(session.errorText().isEmpty());
        QVERIFY(!session.submit("79990000000"));
        QVERIFY(session.submit("+79990000000"));
        QTest::qWait(200);
        QVERIFY(session.busy());
        QVERIFY(!session.submit("+79990000000"));
        QTRY_COMPARE(session.phase(), QString("code"));
        QVERIFY(session.submit("bad"));
        QTRY_VERIFY(!session.busy());
        QVERIFY(session.errorText().contains(QString::fromUtf8("Неверный код")));
        QVERIFY(session.submit("flood"));
        QTRY_VERIFY(!session.busy());
        QVERIFY(session.errorText().contains("42"));
        QVERIFY(session.submit("unknown"));
        QTRY_VERIFY(!session.busy());
        QVERIFY(!session.errorText().contains("secret"));
        QVERIFY(session.submit("12345"));
        QTRY_COMPARE(session.phase(), QString("password"));
        QVERIFY(session.submit(QString::fromUtf8("  Пароль \" \\ \n😀  ")));
        QTRY_COMPARE(session.phase(), QString("ready"));
        QVERIFY(session.errorText().isEmpty());
        QCOMPARE(qgetenv("LD_LIBRARY_PATH"), originalLoaderPath);
        QFile key(data + "/database.key");
        QVERIFY(key.open(QIODevice::ReadOnly));
        QByteArray originalKey = key.readAll();
        QCOMPARE(originalKey.size(), 32);
        QCOMPARE(key.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther), QFile::Permissions());
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
        parameters(session);
        QTRY_COMPARE(session.phase(), QString("phone"));
        QFile again(data + "/database.key");
        QVERIFY(again.open(QIODevice::ReadOnly));
        QCOMPARE(again.readAll(), originalKey);
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void email() {
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so";
        fixture(library, "email");
        TelegramSession session(engine(), library, directory.path() + "/private");
        parameters(session);
        QTRY_COMPARE(session.phase(), QString("email"));
        QVERIFY(session.submit("test@example.com"));
        QTRY_COMPARE(session.phase(), QString("emailCode"));
        QVERIFY(session.submit("12345"));
        QTRY_COMPARE(session.phase(), QString("ready"));
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void unsupported_data() {
        QTest::addColumn<QByteArray>("scenario");
        QTest::newRow("registration") << QByteArray("registration");
        QTest::newRow("premium") << QByteArray("premium");
    }
    void unsupported() {
        QFETCH(QByteArray, scenario);
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so";
        fixture(library, scenario);
        TelegramSession session(engine(), library, directory.path() + "/private");
        parameters(session);
        QTRY_COMPARE(session.phase(), QString("unsupported"));
        QVERIFY(!session.submit("never-register"));
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void brokenEngine_data() {
        QTest::addColumn<QByteArray>("scenario");
        QTest::newRow("malformed-json") << QByteArray("malformed");
        QTest::newRow("oversized-response") << QByteArray("oversized");
        QTest::newRow("crash") << QByteArray("crash");
    }
    void brokenEngine() {
        QFETCH(QByteArray, scenario);
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so";
        fixture(library, scenario);
        TelegramSession session(engine(), library, directory.path() + "/private");
        session.start();
        QTRY_COMPARE(session.phase(), QString("failed"));
        QTRY_VERIFY(!session.busy());
        QVERIFY(!session.errorText().isEmpty());
        fixture(library, "full");
        session.start();
        QTRY_COMPARE(session.phase(), QString("parameters"));
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void damagedKeyIsNotReplaced() {
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so";
        QString data = directory.path() + "/private";
        fixture(library, "full");
        QVERIFY(QDir().mkpath(data));
        fixture(data + "/database.key", "broken");
        TelegramSession session(engine(), library, data);
        session.start();
        QTRY_COMPARE(session.phase(), QString("parameters"));
        QVERIFY(!session.configure("12345", QString(32, 'a')));
        QFile key(data + "/database.key");
        QVERIFY(key.open(QIODevice::ReadOnly));
        QCOMPARE(key.readAll(), QByteArray("broken"));
        key.close();
        QVERIFY(key.remove());
        fixture(data + "/existing-database", "saved-session");
        QVERIFY(!session.configure("12345", QString(32, 'a')));
        QVERIFY(!QFile::exists(data + "/database.key"));
        QVERIFY(QFile::remove(data + "/existing-database"));
        fixture(directory.path() + "/outside", "unchanged");
        QVERIFY(QFile::link(directory.path() + "/outside", data + "/database.key"));
        QVERIFY(!session.configure("12345", QString(32, 'a')));
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void unavailable() {
        TelegramSession session("/not/present", "/not/present.so", "/not/used");
        QVERIFY(!session.available());
        session.start();
        QCOMPARE(session.phase(), QString("stopped"));
        QVERIFY(!session.busy());
    }

    void realTdlibOffline() {
        QTemporaryDir directory;
        TelegramSession session("/work/berrygram-tdjson", "/work/real-tdlib/libtdjson.so", directory.path() + "/private");
        QVERIFY(session.available());
        session.start();
        QTRY_COMPARE(session.phase(), QString("parameters"));
        QTRY_COMPARE(session.version(), QString("1.8.69"));
        QVERIFY(!QFile::exists(directory.path() + "/private"));
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
        QVERIFY(session.errorText().isEmpty());
    }

    void timeoutClosesAndCanRestart() {
        QTemporaryDir directory;
        QString library = directory.path() + "/libtdjson.so";
        fixture(library, "full");
        TelegramSession session(engine(), library, directory.path() + "/private");
        session.start();
        QTRY_COMPARE(session.phase(), QString("parameters"));
        // Drive the timer handler directly; no 60-second sleep in the test.
        QVERIFY(QMetaObject::invokeMethod(&session, "timedOut", Qt::DirectConnection));
        QCOMPARE(session.phase(), QString("failed"));
        QTRY_VERIFY(!session.busy());
        session.start();
        QTRY_COMPARE(session.phase(), QString("parameters"));
        session.stop();
        QTRY_COMPARE(session.phase(), QString("stopped"));
    }

    void failedToStart() {
        QTemporaryDir directory;
        QString executable = directory.path() + "/engine";
        QString library = directory.path() + "/libtdjson.so";
        fixture(executable, "#!/missing/interpreter\n");
        fixture(library, "full");
        QVERIFY(QFile::setPermissions(executable, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        TelegramSession session(executable, library, directory.path() + "/private");
        QVERIFY(session.available());
        session.start();
        QTRY_COMPARE(session.phase(), QString("failed"));
        QTRY_VERIFY(!session.busy());
    }
};

QTEST_GUILESS_MAIN(SessionTests)
#include "session-tests.moc"
