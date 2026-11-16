#ifndef BERRYGRAM_PRESENCE_HPP
#define BERRYGRAM_PRESENCE_HPP
#include <QVariantMap>
#include <QDateTime>
#include <QLocale>
namespace Presence {
inline bool active(const QVariantMap &status, uint now=QDateTime::currentDateTime().toTime_t()) {
    return status.value("@type")=="userStatusOnline" && status.value("expires").toUInt()>now;
}
inline QString text(const QVariantMap &user,uint now=QDateTime::currentDateTime().toTime_t()) {
    if(user.value("bot").toBool())return QString::fromUtf8("бот");
    const QVariantMap status=user.value("status").toMap();
    const QString type=status.value("@type").toString();
    if(active(status,now))return QString::fromUtf8("онлайн");
    if(type=="userStatusRecently" || type=="userStatusOnline")return QString::fromUtf8("недавно в сети");
    if(type=="userStatusLastWeek")return QString::fromUtf8("в сети на этой неделе");
    if(type=="userStatusLastMonth")return QString::fromUtf8("в сети в этом месяце");
    if(type=="userStatusOffline") {
        const uint stamp=status.value("was_online").toUInt();if(!stamp)return QString::fromUtf8("давно не в сети");
        const QDateTime last=QDateTime::fromTime_t(stamp),current=QDateTime::fromTime_t(now);
        const QString date=last.date()==current.date()?QString::fromUtf8("сегодня"):last.date()==current.date().addDays(-1)?QString::fromUtf8("вчера"):QLocale("ru_RU").toString(last.date(),"d MMM");
        return QString::fromUtf8("в сети ")+date+QString::fromUtf8(" в ")+last.toString("HH:mm");
    }
    return QString::fromUtf8("статус недоступен");
}
inline QString action(const QString &type) {
    if(type=="chatActionTyping")return QString::fromUtf8("печатает…");
    if(type=="chatActionRecordingVoiceNote")return QString::fromUtf8("записывает голосовое…");
    if(type=="chatActionUploadingVoiceNote")return QString::fromUtf8("отправляет голосовое…");
    if(type=="chatActionUploadingPhoto")return QString::fromUtf8("отправляет фото…");
    if(type=="chatActionUploadingDocument")return QString::fromUtf8("отправляет файл…");
    if(type=="chatActionRecordingVideo" || type=="chatActionRecordingVideoNote")return QString::fromUtf8("записывает видео…");
    if(type=="chatActionUploadingVideo" || type=="chatActionUploadingVideoNote")return QString::fromUtf8("отправляет видео…");
    if(type=="chatActionChoosingSticker")return QString::fromUtf8("выбирает стикер…");
    return QString();
}
}
#endif
