#include "VoiceDecoder.hpp"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QDataStream>
class VoiceTests:public QObject {
    Q_OBJECT
private slots:
    void telegramOpusBecomesBoundedSeekableWav(){
        QTemporaryDir folder;VoiceDecoder decoder;decoder.input="/work/voice/sample.ogg";decoder.directory=folder.path();decoder.start();QVERIFY(decoder.wait(15000));QVERIFY2(decoder.success,qPrintable(decoder.error));
        QFile wav(decoder.output);QVERIFY(wav.open(QIODevice::ReadOnly));QByteArray header=wav.read(44);QCOMPARE(header.left(4),QByteArray("RIFF"));QCOMPARE(header.mid(8,4),QByteArray("WAVE"));QCOMPARE(header.mid(36,4),QByteArray("data"));
        QVERIFY(qAbs(wav.size()-44-60*32000)<1000);QVERIFY(!QFileInfo(decoder.output).isSymLink());
        QVERIFY(!(QFileInfo(decoder.output).permissions() & (QFile::ReadOther|QFile::WriteOther)));
    }
    void invalidCancelledAndOverlongFilesLeaveNoPcm(){
        QTemporaryDir folder;QFile invalid(folder.path()+"/bad.ogg");QVERIFY(invalid.open(QIODevice::WriteOnly));invalid.write("OggS-invalid-input");invalid.close();
        VoiceDecoder decoder;decoder.input=invalid.fileName();decoder.directory=folder.path();decoder.start();QVERIFY(decoder.wait(5000));QVERIFY(!decoder.success);
        decoder.input="/work/voice/sample.ogg";decoder.cancelled.fetchAndStoreOrdered(1);decoder.start();QVERIFY(decoder.wait(5000));QVERIFY(!decoder.success);
        decoder.cancelled.fetchAndStoreOrdered(0);decoder.input="/work/voice/overlong.ogg";decoder.start();QVERIFY(decoder.wait(5000));QVERIFY(!decoder.success);
        QCOMPARE(QDir(folder.path()).entryList(QStringList()<<"voice-*.wav",QDir::Files).size(),0);
    }
};
QTEST_MAIN(VoiceTests)
#include "voice-tests.moc"
