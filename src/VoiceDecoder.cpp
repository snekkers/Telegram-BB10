#include "VoiceDecoder.hpp"
#include <opusfile.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QDataStream>

void VoiceDecoder::run(){
    success=false;output.clear();error=QString::fromUtf8("Не удалось прочитать голосовое сообщение.");
    if(QFileInfo(input).isSymLink() || !QFileInfo(input).isFile() || QFileInfo(input).size()>50*1024*1024)return;
    int code=0;OggOpusFile *opus=op_open_file(QFile::encodeName(input).constData(),&code);if(!opus)return;
    const ogg_int64_t frames=op_pcm_total(opus,-1);
    // Bound both decoding work and the temporary WAV to about 58 MB.
    if(frames<=0 || frames>48000LL*1800){error=QString::fromUtf8("Поддерживаются голосовые продолжительностью до 30 минут.");op_free(opus);return;}
    if(QFileInfo(directory).isSymLink() || !QDir().mkpath(directory) || !QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)){op_free(opus);return;}
    QTemporaryFile wav(directory+"/voice-XXXXXX.wav");
    if(!wav.open() || !wav.setPermissions(QFile::ReadOwner|QFile::WriteOwner)){op_free(opus);return;}
    if(wav.write(QByteArray(44,'\0'))!=44){op_free(opus);return;}
    opus_int16 pcm[11520];qint64 decoded=0;int accumulator=0,count=0,holes=0;bool ok=true;
    for(;;){
        if(cancelled.fetchAndAddOrdered(0)){ok=false;break;}
        const int n=op_read_stereo(opus,pcm,11520);
        if(n==0)break;
        if(n==OP_HOLE && ++holes<100)continue;
        if(n<0 || (decoded+=n)>48000LL*1800){ok=false;break;}
        QByteArray bytes;bytes.reserve(n*2/3+4);
        for(int i=0;i<n;++i){accumulator+=int(pcm[2*i])+int(pcm[2*i+1]);if(++count==3){const short value=short(accumulator/6);bytes.append(char(value&255));bytes.append(char((static_cast<unsigned short>(value)>>8)&255));accumulator=count=0;}}
        if(wav.write(bytes)!=bytes.size()){ok=false;break;}
    }
    op_free(opus);
    if(!ok || cancelled.fetchAndAddOrdered(0) || wav.size()<=44)return;
    const quint32 size=quint32(wav.size()-44);wav.seek(0);QDataStream header(&wav);header.setByteOrder(QDataStream::LittleEndian);
    header.writeRawData("RIFF",4);header<<quint32(size+36);header.writeRawData("WAVEfmt ",8);header<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(16000)<<quint32(32000)<<quint16(2)<<quint16(16);header.writeRawData("data",4);header<<size;
    if(header.status()!=QDataStream::Ok || !wav.flush())return;
    output=wav.fileName();wav.setAutoRemove(false);success=true;error.clear();
}
