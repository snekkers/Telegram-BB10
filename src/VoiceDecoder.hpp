#ifndef TELEGRAM_VOICE_DECODER_HPP
#define TELEGRAM_VOICE_DECODER_HPP
#include <QThread>
#include <QAtomicInt>
#include <QString>
class VoiceDecoder:public QThread {
public:
    explicit VoiceDecoder(QObject *parent=0):QThread(parent),cancelled(0),success(false){}
    QString input,directory,output,error;
    QAtomicInt cancelled;
    bool success;
protected:void run();
};
#endif
