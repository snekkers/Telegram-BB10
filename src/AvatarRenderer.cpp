#include "AvatarRenderer.hpp"
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QLinearGradient>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QBuffer>
#include <QTemporaryFile>
#include <QCryptographicHash>
#include <QDateTime>
#include <cstdio>

namespace {
bool directory(const QString &path){
    if(path.isEmpty())return false;
    QString cursor=QFileInfo(path).absoluteFilePath();
    for(;;){const QFileInfo info(cursor);if(info.isSymLink())return false;const QString parent=info.absolutePath();if(parent==cursor)break;cursor=parent;}
    return QDir().mkpath(path) && QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
}
bool regular(const QString &path,int limit){
    const QFileInfo info(path);return info.isFile() && !info.isSymLink() && info.size()>0 && info.size()<=limit;
}
bool save(const QImage &image,const QString &directoryPath,const QString &path){
    if(!directory(directoryPath) || QFileInfo(path).isSymLink())return false;
    QByteArray bytes;QBuffer buffer(&bytes);if(!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer,"PNG"))return false;
    QTemporaryFile file(directoryPath+"/.avatar-XXXXXX");
    if(!file.open() || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) || file.write(bytes)!=bytes.size() || !file.flush())return false;
    if(std::rename(QFile::encodeName(file.fileName()).constData(),QFile::encodeName(path).constData())!=0)return false;
    file.setAutoRemove(false);return true;
}
}
QString AvatarRenderer::render(const QString &source,const QString &folder){
    if(!regular(source,1024*1024) || !directory(folder))return QString();
    const QFileInfo info(source);
    const QString signature=info.canonicalFilePath()+":"+QString::number(info.size())+":"+QString::number(info.lastModified().toMSecsSinceEpoch())+":"+folder;
    if(m_cached.contains(signature) && regular(m_cached.value(signature),256*1024) && QImageReader(m_cached.value(signature)).size()==QSize(160,160))return QUrl::fromLocalFile(m_cached.value(signature)).toString();
    QFile input(source);if(!input.open(QIODevice::ReadOnly))return QString();const QByteArray bytes=input.read(1024*1024+1);
    if(bytes.isEmpty() || bytes.size()>1024*1024)return QString();
    QBuffer encoded;encoded.setData(bytes);encoded.open(QIODevice::ReadOnly);QImageReader reader(&encoded);
    const QSize size=reader.size();if(size.width()<=0 || size.height()<=0 || size.width()>2048 || size.height()>2048 || qlonglong(size.width())*size.height()>4*1024*1024)return QString();
    const QString target=QDir(folder).filePath(QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha1).toHex())+"-160.png");
    if(QFileInfo(target).isSymLink())return QString();
    if(regular(target,256*1024) && QImageReader(target).size()==QSize(160,160)){m_cached.insert(signature,target);return QUrl::fromLocalFile(target).toString();}
    const QImage decoded=reader.read();if(decoded.isNull())return QString();
    const QImage scaled=decoded.scaled(160,160,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
    QImage result(160,160,QImage::Format_ARGB32_Premultiplied);result.fill(0);
    {QPainter painter(&result);painter.drawImage((160-scaled.width())/2,(160-scaled.height())/2,scaled);}
    QImage mask(160,160,QImage::Format_ARGB32_Premultiplied);mask.fill(0);
    {QPainter painter(&mask);painter.setRenderHint(QPainter::Antialiasing,true);painter.setPen(Qt::NoPen);painter.setBrush(Qt::white);painter.drawEllipse(QRectF(0.5,0.5,159,159));}
    {QPainter painter(&result);painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);painter.drawImage(0,0,mask);}
    if(!save(result,folder,target))return QString();
    m_cached.insert(signature,target);while(m_cached.size()>256)m_cached.erase(m_cached.begin());
    const QFileInfoList old=QDir(folder).entryInfoList(QStringList()<<"*-160.png",QDir::Files,QDir::Time);
    for(int i=128;i<old.size();++i)if(!old.at(i).isSymLink())QFile::remove(old.at(i).absoluteFilePath());
    return QUrl::fromLocalFile(target).toString();
}
QString AvatarRenderer::demo(const QString &key,const QString &folder){
    if(key!="demo:anna" && key!="demo:classic" && key!="demo:group" && key!="demo:channel")return QString();
    const QString source=QDir(folder).filePath(key.mid(5)+"-source.png");
    if(!regular(source,256*1024)){
        QImage sample(160,160,QImage::Format_ARGB32_Premultiplied);sample.fill(0);
        QPainter painter(&sample);QLinearGradient sky(0,0,160,160);sky.setColorAt(0,QColor(key=="demo:anna"?"#f4ac87":"#70adc8"));sky.setColorAt(1,QColor(key=="demo:anna"?"#6478b3":"#244d74"));
        painter.fillRect(0,0,160,160,sky);painter.setRenderHint(QPainter::Antialiasing,true);painter.setPen(Qt::NoPen);painter.setBrush(QColor("#ffe6ae"));painter.drawEllipse(QRectF(103,28,28,28));
        QPolygonF mountain;mountain<<QPointF(-10,160)<<QPointF(54,58)<<QPointF(112,130)<<QPointF(140,90)<<QPointF(180,160);painter.setBrush(QColor("#37536a"));painter.drawPolygon(mountain);
        if(key=="demo:group" || key=="demo:channel"){
            painter.fillRect(0,0,160,160,QColor(key=="demo:group"?"#458c85":"#7762b0"));painter.setBrush(Qt::white);
            if(key=="demo:group"){painter.drawEllipse(QRectF(36,43,30,30));painter.drawEllipse(QRectF(94,43,30,30));painter.drawEllipse(QRectF(59,35,42,42));painter.drawRoundedRect(QRectF(23,83,114,40),19,19);}
            else{QPolygonF speaker;speaker<<QPointF(34,67)<<QPointF(62,67)<<QPointF(110,43)<<QPointF(110,114)<<QPointF(62,91)<<QPointF(34,91);painter.drawPolygon(speaker);painter.drawRoundedRect(QRectF(54,88,17,36),5,5);}
        }
        painter.end();
        if(!save(sample,folder,source))return QString();
    }
    return render(source,folder);
}
