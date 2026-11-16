#include "VoicePlayer.hpp"
#include "TelegramStore.hpp"
#include "PrivateFile.hpp"
#include <bb/multimedia/MediaError>
#include <bb/multimedia/MediaState>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
namespace {QString ru(const char *s){return QString::fromUtf8(s);}}
VoicePlayer::VoicePlayer(TelegramStore *store,QObject *parent):QObject(parent),m_store(store),m_decoder(this),m_loading(false),m_startPending(false),m_checkSample(false),m_resume(0),m_prepareAttempts(0),m_lastPosition(0),m_speed(1.0){
    connect(&m_decoder,SIGNAL(finished()),this,SLOT(decoded()));
    connect(&m_player,SIGNAL(positionChanged(unsigned int)),this,SLOT(progress()));
    connect(&m_player,SIGNAL(durationChanged(unsigned int)),this,SLOT(progress()));
    connect(&m_player,SIGNAL(mediaStateChanged(bb::multimedia::MediaState::Type)),this,SLOT(stateChanged()));
    connect(&m_player,SIGNAL(error(bb::multimedia::MediaError::Type,unsigned int)),this,SLOT(failed()));
    connect(&m_player,SIGNAL(playbackCompleted()),this,SLOT(completed()));
    connect(store,SIGNAL(changed()),this,SLOT(validate()));
    connect(&m_saveTimer,SIGNAL(timeout()),this,SLOT(save()));m_saveTimer.setInterval(15000);
    m_player.setStatusInterval(500);
    PrivateFile::read(positionFile(),&m_positions,32768);
    // Files from an interrupted conversion are private, disposable PCM copies.
    const QString folder=QDir(store->dataDirectory()).filePath("voice-cache");
    if(!QFileInfo(folder).isSymLink())foreach(const QFileInfo &f,QDir(folder).entryInfoList(QStringList()<<"voice-*.wav",QDir::Files))if(!f.isSymLink())QFile::remove(f.absoluteFilePath());
}
VoicePlayer::~VoicePlayer(){close();m_decoder.cancelled.fetchAndStoreOrdered(1);m_decoder.wait();if(!m_decoder.output.isEmpty())QFile::remove(m_decoder.output);}
QString VoicePlayer::positionFile() const{return QDir(m_store->dataDirectory()).filePath("voice-positions.json");}
bool VoicePlayer::playing() const{return m_player.mediaState()==bb::multimedia::MediaState::Started;}
bool VoicePlayer::open(const QString &id){
    const QString path=m_store->mediaPath(id);QVariantMap row;
    foreach(const QVariant &v,m_store->messages())if(v.toMap().value("messageId")==id)row=v.toMap();
    if(!m_store->ready() || path.isEmpty() || row.value("media").toMap().value("kind")!="voice")return false;
    return beginPlayback(id,path,false);
}
bool VoicePlayer::openCheckSample(){
#ifdef BERRYGRAM_UI_CHECKS
    if(m_store->ready())return false;
    return beginPlayback("check-sample",QDir::current().filePath("app/native/assets/ui/voice-check.ogg"),true);
#else
    return false;
#endif
}
bool VoicePlayer::beginPlayback(const QString &id,const QString &path,bool checkSample){
    if(m_loading || m_decoder.isRunning())return false;
    close();m_lastPosition=0;m_checkSample=checkSample;m_player.setVolume(checkSample?0.0:1.0);
    m_id=id;m_chat=m_store->currentChatId();m_account=m_store->accountKey();
    m_key=QString::fromLatin1(QCryptographicHash::hash((m_account+":"+m_chat+":"+id).toUtf8(),QCryptographicHash::Sha1).toHex());
    m_resume=qBound(0,m_positions.value(m_key).toMap().value("position").toInt(),1800000);
    QFile file(path);if(!file.open(QIODevice::ReadOnly)){m_status=ru("Файл недоступен. Скачайте его снова.");return false;}
    const QByteArray header=file.read(16);file.close();
    if(header.startsWith("OggS")){
        m_loading=true;m_status=ru("Подготовка голосового…");m_decoder.input=path;m_decoder.directory=QDir(m_store->dataDirectory()).filePath("voice-cache");m_decoder.cancelled.fetchAndStoreOrdered(0);m_decoder.start();
    }else prepare(path);
    emit changed();return true;
}
void VoicePlayer::decoded(){
    m_loading=false;
    if(m_id.isEmpty()){if(!m_decoder.output.isEmpty())QFile::remove(m_decoder.output);emit changed();return;}
    if(!m_decoder.success){m_status=m_decoder.error;emit changed();return;}
    m_temporary=m_decoder.output;prepare(m_temporary);emit changed();
}
void VoicePlayer::prepare(const QString &path){
    m_source=path;m_startPending=true;m_prepareAttempts=0;m_status.clear();
    if(m_player.setSourceUrl(QUrl::fromLocalFile(path))!=bb::multimedia::MediaError::None || m_player.prepare()!=bb::multimedia::MediaError::None)failed();
    else QTimer::singleShot(0,this,SLOT(startPrepared()));
}
void VoicePlayer::stateChanged(){
    if(m_startPending)QTimer::singleShot(0,this,SLOT(startPrepared()));
    if(playing())m_saveTimer.start();else m_saveTimer.stop();emit changed();
}
void VoicePlayer::startPrepared(){
    if(!m_startPending || m_source.isEmpty() || m_id.isEmpty())return;
    if(duration()<=0){if(++m_prepareAttempts<100)QTimer::singleShot(50,this,SLOT(startPrepared()));else failed();return;}
    m_startPending=false;
    if(m_player.setSpeed(m_speed)!=bb::multimedia::MediaError::None)m_speed=1.0;
    if(m_player.play()!=bb::multimedia::MediaError::None){failed();return;}
    // BB10 play() restarts a prepared/stopped track at zero. Seek afterwards.
    if(m_resume>0 && m_resume<duration()-1000 && m_player.seekTime(m_resume)!=bb::multimedia::MediaError::None)m_status=ru("Не удалось восстановить позицию.");
    m_resume=0;emit changed();
}
void VoicePlayer::progress(){m_lastPosition=position();emit changed();}
void VoicePlayer::toggle(){
    if(!available())return;
    if(playing()){m_player.pause();save();}
    else{m_player.setSpeed(m_speed);if(m_player.play()!=bb::multimedia::MediaError::None)failed();}
}
void VoicePlayer::seek(int position){
    if(!available() || duration()<=0)return;
    if(m_player.seekTime(unsigned(qBound(0,position,qMax(0,duration()-1))))!=bb::multimedia::MediaError::None)m_status=ru("Перемотка недоступна.");
    else {m_status.clear();save();}emit changed();
}
void VoicePlayer::setSpeed(double speed){
    if(!available() || (speed!=1.0 && speed!=1.5 && speed!=2.0))return;
    const bool wasPlaying=playing();
    if(m_player.setSpeed(speed)==bb::multimedia::MediaError::None){m_speed=speed;if(!wasPlaying)m_player.pause();m_status.clear();}
    else m_status=ru("Эта скорость недоступна для файла.");emit changed();
}
void VoicePlayer::failed(){m_startPending=false;m_saveTimer.stop();m_status=ru("Не удалось воспроизвести голосовое сообщение.");m_source.clear();m_player.reset();emit changed();}
void VoicePlayer::completed(){
    if(m_startPending || m_key.isEmpty() || duration()<=0 || qMax(position(),m_lastPosition)<duration()-1500)return;
    m_positions.remove(m_key);PrivateFile::write(positionFile(),m_positions,32768);m_resume=0;m_saveTimer.stop();emit changed();
}
void VoicePlayer::save(){
    if(m_key.isEmpty() || m_source.isEmpty() || loading() || duration()<=0)return;
    QVariantMap item;item.insert("position",position()>=duration()-1000?0:position());item.insert("updated",QDateTime::currentDateTime().toTime_t());m_positions.insert(m_key,item);
    while(m_positions.size()>128){QString oldest;uint time=0xffffffffU;foreach(const QString &key,m_positions.keys()){const uint updated=m_positions.value(key).toMap().value("updated").toUInt();if(updated<=time){time=updated;oldest=key;}}m_positions.remove(oldest);}
    if(!PrivateFile::write(positionFile(),m_positions,32768))m_status=ru("Не удалось запомнить позицию.");
}
void VoicePlayer::close(){
    save();m_id.clear();m_key.clear();m_chat.clear();m_account.clear();m_saveTimer.stop();m_startPending=false;
    if(m_loading)m_decoder.cancelled.fetchAndStoreOrdered(1);
    m_player.reset();m_source.clear();if(!m_temporary.isEmpty())QFile::remove(m_temporary);m_temporary.clear();m_status.clear();m_checkSample=false;emit changed();
}
void VoicePlayer::validate(){if(!m_checkSample && !m_id.isEmpty() && (!m_store->ready() || m_chat!=m_store->currentChatId() || m_account!=m_store->accountKey()))close();}
