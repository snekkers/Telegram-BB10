#include "AvatarRenderer.hpp"
#include "EmojiText.hpp"
#include "PhotoCache.hpp"
#include <QResource>

#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QImage>
#include <QUrl>
#include <QPainter>
class AvatarTests:public QObject{
    Q_OBJECT
private slots:
    void emojiSequencesAreMatchedWithoutLosingText(){
        QVERIFY(QResource::registerResource("/source/assets/emoji.rcc"));
        const QString assets=":/emoji";
        QStringList sequences;sequences<<QString::fromUtf8("😀")<<QString::fromUtf8("👍🏽")<<QString::fromUtf8("🇷🇺")<<QString::fromUtf8("👨‍👩‍👧‍👦")<<QString::fromUtf8("🏳️‍🌈")<<QString::fromUtf8("1️⃣");
        foreach(const QString &emoji,sequences){
            const QVariantList parts=EmojiRenderer::parts(emoji,assets);QCOMPARE(parts.size(),1);
            QCOMPARE(parts.first().toMap().value("text").toString(),emoji);
            QImage image(parts.first().toMap().value("emoji").toString());QVERIFY(!image.isNull());
        }
        const QString text=QString::fromUtf8("Привет 😀 <b>literal</b>\nСледующая строка 👍🏽");
        QString restored;foreach(const QVariant &part,EmojiRenderer::parts(text,assets))restored+=part.toMap().value("text").toString();
        QCOMPARE(restored,text);QVERIFY(!EmojiRenderer::contains("Hello <img src=x> Привет 123",assets));
    }
    void photosDecodeToBoundedPrivateThumbnails(){
        QTemporaryDir temp;QImage original(2400,1600,QImage::Format_RGB32);original.fill(Qt::blue);const QString source=temp.path()+"/original.jpg";QVERIFY(original.save(source));
        const QString url=PhotoCache::scaled(source,temp.path()+"/cache",320);QVERIFY(!url.isEmpty());QImage thumbnail(QUrl(url).toLocalFile());QCOMPARE(thumbnail.width(),320);QVERIFY(thumbnail.height()<=320);QCOMPARE(PhotoCache::scaled(source,temp.path()+"/cache",320),url);
        QVERIFY(QFile::link(source,temp.path()+"/link.jpg"));QVERIFY(PhotoCache::scaled(temp.path()+"/link.jpg",temp.path()+"/cache",320).isEmpty());
    }
    void circleCropIsCachedPrivateAndOpaqueInside(){
        QTemporaryDir temp;const QString source=temp.path()+"/wide.png",folder=temp.path()+"/cache";
        QImage input(240,120,QImage::Format_RGB32);input.fill(QColor("#c25a48"));
        {QPainter p(&input);p.fillRect(100,0,40,120,QColor("#164cb0"));}
        QVERIFY(input.save(source));
        AvatarRenderer renderer;const QString url=renderer.render(source,folder);QVERIFY(!url.isEmpty());
        const QString path=QUrl(url).toLocalFile();QImage circle(path);QCOMPARE(circle.size(),QSize(160,160));
        QCOMPARE(qAlpha(circle.pixel(0,0)),0);QCOMPARE(qAlpha(circle.pixel(80,80)),255);
        QVERIFY(qBlue(circle.pixel(80,80))>qRed(circle.pixel(80,80)));
        QCOMPARE(renderer.render(source,folder),url);
        AvatarRenderer restarted;QCOMPARE(restarted.render(source,folder),url);
        QCOMPARE(QDir(folder).entryList(QStringList()<<"*-160.png",QDir::Files).size(),1);
        QCOMPARE(int(QFile::permissions(path)&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)),0);
    }
    void invalidImagesAndSymlinksAreRejected(){
        QTemporaryDir temp;const QString path=temp.path()+"/bad.png";QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("not an image");file.close();
        AvatarRenderer renderer;QVERIFY(renderer.render(path,temp.path()+"/cache").isEmpty());
        QImage image(32,32,QImage::Format_RGB32);image.fill(Qt::red);const QString source=temp.path()+"/valid.png";QVERIFY(image.save(source));
        const QString link=temp.path()+"/link.png";QVERIFY(QFile::link(source,link));QVERIFY(renderer.render(link,temp.path()+"/cache").isEmpty());
        QDir().mkpath(temp.path()+"/outside");QVERIFY(QFile::link(temp.path()+"/outside",temp.path()+"/linked-cache"));
        QVERIFY(renderer.render(source,temp.path()+"/linked-cache").isEmpty());
        QImage huge(2049,1,QImage::Format_RGB32);huge.fill(Qt::blue);QVERIFY(huge.save(path));QVERIFY(renderer.render(path,temp.path()+"/cache").isEmpty());
    }
    void removedPhotoDoesNotReuseAnotherUsersCache(){
        QTemporaryDir temp;QImage photo(32,32,QImage::Format_RGB32);photo.fill(Qt::red);const QString path=temp.path()+"/photo.png";QVERIFY(photo.save(path));
        AvatarRenderer renderer;QVERIFY(!renderer.render(path,temp.path()+"/cache").isEmpty());QVERIFY(QFile::remove(path));
        QVERIFY(renderer.render(path,temp.path()+"/cache").isEmpty());QVERIFY(renderer.render(QString(),temp.path()+"/cache").isEmpty());
        QVERIFY(!renderer.demo("demo:anna",temp.path()+"/demo").isEmpty());QVERIFY(renderer.demo("demo:unknown",temp.path()+"/demo").isEmpty());
    }
};
int main(int argc,char **argv){QCoreApplication app(argc,argv);AvatarTests tests;return QTest::qExec(&tests,argc,argv);}
#include "avatar-tests.moc"
