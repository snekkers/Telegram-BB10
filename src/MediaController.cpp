#include "MediaController.hpp"
#include "PhotoCache.hpp"
#include "TelegramStore.hpp"
#include "ChatController.hpp"
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QDateTime>
#include <bb/cascades/ArrayDataModel>
MediaController::MediaController(TelegramStore *store,ChatController *demo,QObject *parent):QObject(parent),m_store(store),m_demo(demo),m_live(false),m_canSave(false),m_index(-1){connect(store,SIGNAL(messagesChanged()),this,SLOT(refresh()));connect(store,SIGNAL(changed()),this,SLOT(refresh()));connect(store,SIGNAL(mediaProperties(QString,QVariantMap)),this,SLOT(properties(QString,QVariantMap)));}
QString MediaController::folder() const{return QDir::current().filePath("data/photo-cache");}
QString MediaController::thumbnail(const QString &url){return PhotoCache::scaled(QUrl(url).toLocalFile(),folder(),320);}
QString MediaController::position() const{return m_index<0?QString():QString::fromUtf8("%1 из %2").arg(m_index+1).arg(m_photos.size());}
QString MediaController::original() const{
    if(m_index<0)return QString();
    if(m_live)return m_store->mediaPath(m_id);
    return QDir::current().filePath("app/native/assets/ui/demo-photo.jpg");
}
bool MediaController::openPhoto(const QString &id,bool live){
    closePhoto();m_live=live;m_chat=m_store->currentChatId();
    if(live){foreach(const QVariant &value,m_store->messages())if(value.toMap().value("media").toMap().value("kind")=="photo")m_photos<<value;}
    else{for(int i=0;i<3;++i){QVariantMap row;row.insert("messageId",QString("demo-photo-%1").arg(i));row.insert("text",QString::fromUtf8("Фотография · демо %1").arg(i+1));m_photos<<row;}}
    for(int i=0;i<m_photos.size();++i)if(m_photos.at(i).toMap().value("messageId")==id || (!live && i==0)){select(i);return true;}
    return false;
}
void MediaController::select(int index){
    if(index<0 || index>=m_photos.size())return;m_index=index;m_id=m_photos.at(index).toMap().value("messageId").toString();m_caption=m_photos.at(index).toMap().value("text").toString();m_source.clear();m_status.clear();m_canSave=!m_live;
    if(m_live)m_store->requestMediaProperties(m_id);
    if(m_live && m_store->mediaPath(m_id).isEmpty())if(!m_store->downloadMedia(m_id))m_status=m_store->errorText();
    refresh();
}
void MediaController::refresh(){
    if(m_id.isEmpty())return;if(m_live && m_chat!=m_store->currentChatId()){closePhoto();return;}
    const QString path=original();
    if(!path.isEmpty()){const QString image=PhotoCache::scaled(path,folder(),1280);m_source=image;if(image.isEmpty())m_status=QString::fromUtf8("Не удалось открыть изображение.");}
    if(m_source.isEmpty() && m_live && !m_store->errorText().isEmpty())m_status=m_store->errorText();
    emit changed();
}
void MediaController::properties(const QString &id,const QVariantMap &properties){if(id==m_id && m_live && m_chat==m_store->currentChatId()){m_canSave=properties.value("can_be_saved").toBool();emit changed();}}
void MediaController::movePhoto(int delta){select(m_index+(delta>0?1:-1));}
void MediaController::closePhoto(){m_id.clear();m_source.clear();m_caption.clear();m_status.clear();m_photos.clear();m_index=-1;emit changed();}
bool MediaController::savePhoto(){
    const QString source=original();if(source.isEmpty() || !canSave())return false;
    const QString directory="/accounts/1000/shared/photos/Berrygram";
    if(!QDir().mkpath(directory))m_status=QString::fromUtf8("Не удалось открыть папку фотографий.");
    else{const QString target=directory+"/"+QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss")+"-"+QUuid::createUuid().toString().mid(1,8)+".jpg";
        if(QFile::copy(source,target)){m_status=QString::fromUtf8("Сохранено в Фото / Berrygram");emit changed();return true;}
        m_status=QString::fromUtf8("Не удалось сохранить фотографию.");}
    emit changed();return false;
}
void MediaController::clearCache(){
    if(!m_id.isEmpty())return;
    foreach(const QFileInfo &file,QDir(folder()).entryInfoList(QStringList()<<"*.jpg",QDir::Files))if(!file.isSymLink())QFile::remove(file.absoluteFilePath());
    m_status=QString::fromUtf8("Кеш фотографий очищен.");emit changed();
}
