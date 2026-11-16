#include "TelegramStore.hpp"
#include "TelegramSession.hpp"
#include <QDateTime>
#include <QFileInfo>
#include <algorithm>
#ifdef Q_OS_BLACKBERRY
#include <bps/netstatus.h>
#endif
namespace {
QString ru(const char *s){return QString::fromUtf8(s);}
QVariantMap method(const char *s){QVariantMap v;v.insert("@type",s);return v;}
QVariant number(const QString &s){return QVariant::fromValue(s.toLongLong());}
bool earlier(const QVariant &a,const QVariant &b){return a.toMap().value("sendDate").toUInt()<b.toMap().value("sendDate").toUInt();}
}
QString TelegramStore::dataDirectory() const{return m_session->dataDirectory();}
QString TelegramStore::diagnostics() const{
    int active=0,unknown=0;qlonglong oldest=0;
    foreach(const QVariantMap &v,m_requests){if(v.value("expired").toBool())++unknown;else{++active;oldest=qMax(oldest,m_clock.elapsed()-v.value("started").toLongLong());}}
    return ru("Соединение: %1\nСообщений в памяти: %2\nФайлов в памяти: %3\nАктивных запросов: %4\nОжидание самого старого: %5 с\nОтправок без подтверждения: %6\nОтветов: %7\nСреднее ожидание ответа: %8 мс\nТаймаутов: %9")
        .arg(m_connection).arg(m_messages.size()).arg(m_files.size()).arg(active).arg(oldest/1000).arg(unknown).arg(m_completedRequests)
        .arg(m_completedRequests?m_requestLatency/m_completedRequests:0).arg(m_timedOutRequests);
}
void TelegramStore::closeScheduled(){++m_scheduledSerial;m_scheduledOpen=m_scheduledBusy=false;m_scheduledRows.clear();m_scheduledStatus.clear();emit scheduledChanged();}
void TelegramStore::loadScheduled(){
    if(!m_ready || m_current.isEmpty() || m_scheduledBusy)return;
    m_scheduledOpen=true;
    QVariantMap call=method("getChatScheduledMessages");call.insert("chat_id",number(m_current));
    const QString request=submit(call,"scheduled-list",m_current);if(request.isEmpty())return;
    m_requests[request].insert("scheduledSerial",m_scheduledSerial);m_scheduledBusy=true;m_scheduledStatus=ru("Загрузка…");emit scheduledChanged();
}
bool TelegramStore::changeScheduled(const QString &id,const QString &text,uint date,bool changeText){
    if(!m_ready || m_scheduledBusy || !m_scheduledOpen)return false;
    QVariantMap row;foreach(const QVariant &v,m_scheduledRows)if(v.toMap().value("messageId")==id)row=v.toMap();
    if(row.isEmpty())return false;
    QVariantMap call=method(changeText?"editMessageText":"editMessageSchedulingState");
    if(changeText)call.insert("reply_markup",QVariant());
    call.insert("chat_id",number(m_current));call.insert("message_id",number(id));
    if(changeText){
        if(!row.value("textMessage").toBool() || text.trimmed().isEmpty() || text.size()>4096)return false;
        QVariantMap input=method("inputMessageText"),formatted=method("formattedText");formatted.insert("text",text);formatted.insert("entities",QVariantList());input.insert("text",formatted);input.insert("clear_draft",false);call.insert("input_message_content",input);
    }else{
        const qint64 now=QDateTime::currentDateTime().toTime_t();
        if(qint64(date)<=now+10 || qint64(date)>now+366LL*86400 || date>2147483647U){m_scheduledStatus=ru("Выберите будущее время в пределах года.");emit scheduledChanged();return false;}
        QVariantMap state=method("messageSchedulingStateSendAtDate");state.insert("send_date",date);state.insert("repeat_period",0);call.insert("scheduling_state",state);
    }
    const QString request=submit(call,"scheduled-change",m_current);if(request.isEmpty())return false;
    m_requests[request].insert("scheduledSerial",m_scheduledSerial);m_scheduledBusy=true;m_scheduledStatus=ru("Сохраняем…");emit scheduledChanged();return true;
}
bool TelegramStore::cancelScheduled(const QString &id){
    if(!m_ready || !m_scheduledOpen || m_scheduledBusy)return false;
    bool found=false;foreach(const QVariant &v,m_scheduledRows)if(v.toMap().value("messageId")==id)found=true;if(!found)return false;
    QVariantMap call=method("deleteMessages");call.insert("chat_id",number(m_current));call.insert("message_ids",QVariantList()<<number(id));call.insert("revoke",true);
    const QString request=submit(call,"scheduled-change",m_current);if(request.isEmpty())return false;
    m_requests[request].insert("scheduledSerial",m_scheduledSerial);m_scheduledBusy=true;m_scheduledStatus=ru("Отменяем отправку…");emit scheduledChanged();return true;
}
bool TelegramStore::toolsResponse(const QVariantMap &object,const QVariantMap &pending){
    const QString kind=pending.value("kind").toString();
    if(kind=="download-cancel"){
        const int id=pending.value("fileId").toInt();
        if(m_downloads.contains(id) && pending.value("fileSerial")==m_downloads.value(id).value("serial")){m_downloads[id].insert("cancelPending",false);m_downloads[id].insert("active",false);m_downloads[id].insert("status",object.value("@type")=="error"?ru("Не удалось отменить. Обновите список."):ru("Отменено"));emit downloadsChanged();}return true;
    }
    if(!kind.startsWith("scheduled-"))return false;
    if(!m_scheduledOpen || pending.value("scheduledSerial").toUInt()!=m_scheduledSerial || pending.value("generation").toUInt()!=m_generation || pending.value("chatId")!=m_current)return true;
    m_scheduledBusy=false;
    if(object.value("@type")=="error")m_scheduledStatus=ru("Telegram отклонил действие (код %1). Обновите список для проверки.").arg(object.value("code").toInt());
    else if(kind=="scheduled-list"){
        QVariantList rows;
        foreach(const QVariant &v,object.value("messages").toList()){
            const QVariantMap message=v.toMap();if(message.value("chat_id").toLongLong()!=m_current.toLongLong())continue;
            QVariantMap row=messageRow(message);const QVariantMap state=message.value("scheduling_state").toMap();const uint date=state.value("send_date").toUInt();
            row.insert("sendDate",date);row.insert("sendTime",date?QDateTime::fromTime_t(date).toString("dd.MM.yyyy HH:mm"):ru("Когда получатель будет в сети"));
            row.insert("title",row.value("text").toString().left(160));rows<<row;
        }
        std::sort(rows.begin(),rows.end(),earlier);m_scheduledRows=rows;m_scheduledStatus=rows.isEmpty()?ru("Нет отложенных сообщений"):ru("Сообщений: %1").arg(rows.size());
    }else loadScheduled();
    emit scheduledChanged();return true;
}
bool TelegramStore::muteFor(int seconds){
    if(!m_ready || m_current.isEmpty() || seconds<0)return false;
    QVariantMap settings=m_chats.value(m_current).value("notificationSettings").toMap();if(settings.isEmpty())return false;
    settings.insert("use_default_mute_for",false);settings.insert("mute_for",seconds);
    QVariantMap call=method("setChatNotificationSettings");call.insert("chat_id",number(m_current));call.insert("notification_settings",settings);
    const QString request=submit(call,"notifications",m_current);if(request.isEmpty())return false;
    m_requests[request].insert("settings",settings);return true;
}
QVariantList TelegramStore::downloads() const{QVariantList rows;foreach(const QVariantMap &item,m_downloads)rows<<item;return rows;}
void TelegramStore::updateDownload(const QVariantMap &file){
    const int id=file.value("id").toInt();if(!m_downloads.contains(id))return;
    QVariantMap row=m_downloads.value(id),fields=fileInfo(file);const bool complete=file.value("local").toMap().value("is_downloading_completed").toBool();
    foreach(const QString &key,fields.keys())row.insert(key,fields.value(key));
    row.insert("active",fields.value("downloading").toBool() && !m_autoSuppressed.contains(id));
    row.insert("status",complete?ru("Загружено"):m_autoSuppressed.contains(id)?ru("Отменено"):fields.value("downloading").toBool()?ru("Загрузка…"):ru("Ожидание или остановлено"));
    if(row!=m_downloads.value(id)){m_downloads.insert(id,row);emit downloadsChanged();}
}
bool TelegramStore::cancelDownload(int id){
    if(!m_ready || !m_downloads.contains(id) || !m_downloads.value(id).value("active").toBool())return false;
    QVariantMap call=method("cancelDownloadFile");call.insert("file_id",id);call.insert("only_if_pending",false);
    const QString request=submit(call,"download-cancel");if(request.isEmpty())return false;
    m_requests[request].insert("fileId",id);m_downloads[id].insert("serial",++m_downloadSerial);m_requests[request].insert("fileSerial",m_downloadSerial);m_downloads[id].insert("cancelPending",true);m_autoSuppressed.insert(id);m_downloading.remove(id);m_downloads[id].insert("active",false);m_downloads[id].insert("status",ru("Отменяем…"));emit downloadsChanged();return true;
}
bool TelegramStore::retryDownload(int id){
    const QVariantMap row=m_downloads.value(id);
    if(!m_ready || row.isEmpty() || row.value("active").toBool() || row.value("cancelPending").toBool() || m_downloading.size()>=4 || !row.value("path").toString().isEmpty())return false;
    QVariantMap call=method("downloadFile");call.insert("file_id",id);call.insert("priority",16);call.insert("offset",0);call.insert("limit",50*1024*1024);call.insert("synchronous",false);
    const QString request=submit(call,"download",row.value("chatId").toString());if(request.isEmpty())return false;
    m_requests[request].insert("fileId",id);m_downloads[id].insert("serial",++m_downloadSerial);m_requests[request].insert("fileSerial",m_downloadSerial);m_downloading.insert(id);m_autoSuppressed.remove(id);m_downloads[id].insert("active",true);m_downloads[id].insert("status",ru("Загрузка…"));emit downloadsChanged();return true;
}
void TelegramStore::autoDownload(){
    if(!m_autoDownload || !m_foreground || !m_ready || !m_online || m_loadingHistory || m_downloading.size()>=2)return;
#ifdef Q_OS_BLACKBERRY
    netstatus_interface_details_t *network=0;
    if(netstatus_get_interface_details(0,&network)!=BPS_SUCCESS || !network)return;
    const netstatus_interface_type_t type=netstatus_interface_get_type(network);
    const bool permitted=type==NETSTATUS_INTERFACE_TYPE_WIFI?m_autoWifiAllowed:type==NETSTATUS_INTERFACE_TYPE_CELLULAR?m_autoMobileAllowed:false;
    netstatus_free_interface_details(&network);if(!permitted)return;
#endif
    QMap<qlonglong,QVariantMap>::const_iterator it=m_messages.constEnd();int inspected=0;
    while(it!=m_messages.constBegin() && inspected++<10 && m_downloading.size()<2){--it;const QVariantMap media=it.value().value("media").toMap();const int id=media.value("fileId").toInt();
        if(id<=0 || m_downloads.contains(id) || m_autoSuppressed.contains(id) || !media.value("path").toString().isEmpty() || media.value("size").toLongLong()<=0 || media.value("size").toLongLong()>5*1024*1024 || (media.value("kind")!="photo" && media.value("kind")!="voice"))continue;
        downloadMedia(it.value().value("messageId").toString());
    }
}
