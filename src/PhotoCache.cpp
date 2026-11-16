#include "PhotoCache.hpp"
#include <QtGui/QImageReader>
#include <QtGui/QImage>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QCryptographicHash>
#include <QDateTime>
#include <QTemporaryFile>
#include <cstdio>
namespace { int cacheMegabytes=24; }
void PhotoCache::setLimit(int value){if(value==8||value==24||value==64)cacheMegabytes=value;}
int PhotoCache::limit(){return cacheMegabytes;}
QString PhotoCache::scaled(const QString &path,const QString &folder,int edge){
    QFileInfo file(path);if(!file.isFile() || file.isSymLink() || file.size()>50*1024*1024 || edge<32 || edge>1280)return QString();
    if(QFileInfo(folder).isSymLink() || !QDir().mkpath(folder))return QString();
    QFile::setPermissions(folder,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    const QString key=path+QString::number(file.size())+QString::number(file.lastModified().toMSecsSinceEpoch())+QString::number(edge);
    const QString target=QDir(folder).filePath(QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha1).toHex())+".jpg");
    if(QFileInfo(target).isSymLink())return QString();if(QFileInfo(target).isFile())return QUrl::fromLocalFile(target).toString();
    QImageReader reader(path);QSize size=reader.size();if(!size.isValid() || size.width()>12000 || size.height()>12000 || qlonglong(size.width())*size.height()>40000000)return QString();
    size.scale(edge,edge,Qt::KeepAspectRatio);reader.setScaledSize(size);const QImage image=reader.read();if(image.isNull())return QString();
    QTemporaryFile output(folder+"/.photo-XXXXXX");if(!output.open() || !output.setPermissions(QFile::ReadOwner|QFile::WriteOwner) || !image.save(&output,"JPG",85) || !output.flush())return QString();
    if(std::rename(QFile::encodeName(output.fileName()).constData(),QFile::encodeName(target).constData())!=0)return QString();output.setAutoRemove(false);
    const QFileInfoList old=QDir(folder).entryInfoList(QStringList()<<"*.jpg",QDir::Files,QDir::Time);qlonglong bytes=0;
    for(int i=0;i<old.size();++i){bytes+=old.at(i).size();if((i>=80 || bytes>qlonglong(cacheMegabytes)*1024*1024) && old.at(i).absoluteFilePath()!=target && !old.at(i).isSymLink())QFile::remove(old.at(i).absoluteFilePath());}
    return QUrl::fromLocalFile(target).toString();
}
