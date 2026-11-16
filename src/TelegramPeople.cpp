#include "TelegramStore.hpp"
#include "TelegramSession.hpp"
#include "Presence.hpp"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QCryptographicHash>
#include <algorithm>

namespace {
QString ru(const char *s){return QString::fromUtf8(s);}
QString idOf(const QVariant &v){return QString::number(v.toLongLong());}
QVariant number(const QString &id){return QVariant::fromValue(id.toLongLong());}
QVariantMap method(const char *name){QVariantMap result;result.insert("@type",name);return result;}
bool contactBefore(const QVariantMap &a,const QVariantMap &b){
    return QString::localeAwareCompare(a.value("title").toString().toLower(),b.value("title").toString().toLower())<0;
}
bool recentBefore(const QVariantMap &a,const QVariantMap &b){
    return a.value("mainOrder").toLongLong()>b.value("mainOrder").toLongLong();
}
}
void TelegramStore::loadContacts(){
    if(!m_ready || m_contactsLoading)return;
    const QString request=submit(method("getContacts"),"contacts");
    if(request.isEmpty())return;
    m_contactsLoading=true;m_contactsError.clear();rebuildContacts();emit contactsChanged();
}
void TelegramStore::setContactQuery(const QString &text){
    const QString next=text.trimmed().left(128);if(next==m_contactQuery)return;
    m_contactQuery=next;rebuildContacts();
}
void TelegramStore::setContactSection(int section){
    section=qBound(0,section,1);if(section==m_contactSection)return;
    m_contactSection=section;rebuildContacts();
}
void TelegramStore::cancelContactOpen(){++m_contactSerial;m_openingContact=false;m_pendingContactChat.clear();emit contactsChanged();}
bool TelegramStore::openContact(const QString &id){
    if(!m_ready || m_openingContact || id.toLongLong()<=0 || (!m_users.contains(id) && !m_contactIds.contains(id)))return false;
    QVariantMap call=method("createPrivateChat");call.insert("user_id",number(id));call.insert("force",false);
    const QString request=submit(call,"contact-open");if(request.isEmpty())return false;
    ++m_contactSerial;m_requests[request].insert("contactSerial",m_contactSerial);
    m_openingContact=true;m_contactsError.clear();emit contactsChanged();return true;
}
void TelegramStore::upsertUser(const QVariantMap &user){
    const QString id=idOf(user.value("id"));if(id=="0")return;
    QVariantMap row;row.insert("userId",id);
    row.insert("name",(user.value("first_name").toString()+" "+user.value("last_name").toString()).trimmed());
    if(row.value("name").toString().isEmpty())row.insert("name",ru("Пользователь"));
    const QVariantMap names=user.value("usernames").toMap();
    QString username=names.value("editable_username").toString();
    if(username.isEmpty() && !names.value("active_usernames").toList().isEmpty())username=names.value("active_usernames").toList().first().toString();
    row.insert("username",username);row.insert("status",user.value("status"));row.insert("bot",user.value("type").toMap().value("@type")=="userTypeBot");
    row.insert("isContact",user.value("is_contact"));m_users.insert(id,row);
    if(user.value("is_contact").toBool() && !m_contactIds.contains(id))m_contactIds.append(id);
    else if(user.contains("is_contact") && !user.value("is_contact").toBool())m_contactIds.removeAll(id);
    rememberAvatar("user:"+id,user.value("profile_photo").toMap());
    m_nextPresenceRefresh=0;
    if(m_contactIds.contains(id) || m_contactSection==1)rebuildContacts();
    emit changed();
}
void TelegramStore::rebuildContacts(){
    QList<QVariantMap> rows;
    QString query=m_contactQuery;if(query.startsWith('@'))query.remove(0,1);
    if(m_ready) {
        if(m_contactSection==0)foreach(const QString &id,m_contactIds){
            const QVariantMap user=m_users.value(id);
            QVariantMap row;row.insert("userId",id);row.insert("key","user:"+id);row.insert("chatId","user:"+id);
            row.insert("title",user.value("name",ru("Контакт")));row.insert("avatarKey","user:"+id);
            row.insert("presenceText",Presence::text(user));row.insert("preview",Presence::text(user));row.insert("statusActive",Presence::active(user.value("status").toMap()));
            row.insert("username",user.value("username"));row.insert("time",QString());row.insert("unreadLabel",QString());
            if(query.isEmpty() || row.value("title").toString().contains(query,Qt::CaseInsensitive) || row.value("username").toString().contains(query,Qt::CaseInsensitive))rows.append(row);
        } else foreach(const QVariantMap &chat,m_chats){
            if(chat.value("type")!="chatTypePrivate" || chat.value("mainOrder").toLongLong()<=0 || chat.value("peerId").toLongLong()<=0)continue;
            const QString id=chat.value("peerId").toString();const QVariantMap user=m_users.value(id);
            QVariantMap row=chat;row.insert("userId",id);row.insert("key","user:"+id);row.insert("avatarKey","chat:"+chat.value("chatId").toString());
            row.insert("preview",id==m_account?ru("Сообщения для себя"):Presence::text(user));row.insert("presenceText",row.value("preview"));row.insert("statusActive",Presence::active(user.value("status").toMap()));
            row.insert("username",user.value("username"));row.insert("time",QString());row.insert("unreadLabel",QString());
            if(query.isEmpty() || row.value("title").toString().contains(query,Qt::CaseInsensitive) || row.value("username").toString().contains(query,Qt::CaseInsensitive))rows.append(row);
        }
    }
    std::sort(rows.begin(),rows.end(),m_contactSection==0?contactBefore:recentBefore);
    QVariantList result;
    foreach(QVariantMap row,rows){row.remove("mainOrder");row.remove("archiveOrder");result.append(row);}
    if(result!=m_contactRows){m_contactRows=result;emit contactsChanged();}
}
QString TelegramStore::chatActivity(const QString &chatId) const{
    QStringList names;QString action;int count=0;bool typing=true;
    foreach(const QVariantMap &value,m_chatActions){
        if(value.value("chatId")!=chatId || value.value("until").toLongLong()<=m_clock.elapsed())continue;
        const QString text=Presence::action(value.value("action").toString());if(text.isEmpty())continue;
        action=text;++count;typing=typing && value.value("action")=="chatActionTyping";
        names.append(m_users.value(value.value("userId").toString()).value("name",ru("Участник")).toString());
    }
    if(!count)return QString();
    if(m_chats.value(chatId).value("type")=="chatTypePrivate")return action;
    if(count==1)return names.first()+" "+action;
    if(typing)return count==2?names.join(ru(" и "))+ru(" печатают…"):ru("Несколько участников печатают…");
    return ru("Участники отправляют сообщения…");
}
QString TelegramStore::currentChatStatus() const{
    if(!m_ready)return ru("Нет подключения к Telegram");
    if(!m_online)return ru("Ожидание сети");
    const QString activity=chatActivity(m_current);if(!activity.isEmpty())return activity;
    const QVariantMap chat=m_chats.value(m_current);
    if(chat.value("type")=="chatTypePrivate"){
        const QString id=chat.value("peerId").toString();if(id==m_account)return ru("Избранное");
        return Presence::text(m_users.value(id));
    }
    if(chat.value("type")=="chatTypeSecret")return ru("Секретный чат");
    return chat.value("isChannel").toBool()?ru("канал"):ru("группа");
}
bool TelegramStore::currentStatusActive() const{
    if(!m_ready || !m_online)return false;
    if(!chatActivity(m_current).isEmpty())return true;
    const QVariantMap chat=m_chats.value(m_current);
    return chat.value("type")=="chatTypePrivate" && Presence::active(m_users.value(chat.value("peerId").toString()).value("status").toMap());
}
bool TelegramStore::peopleUpdate(const QVariantMap &object){
    const QString type=object.value("@type").toString();
    if(type=="updateUserStatus"){
        const QString id=idOf(object.value("user_id"));if(m_users.contains(id))m_users[id].insert("status",object.value("status"));
        m_nextPresenceRefresh=0;rebuildContacts();emit changed();return true;
    }
    if(type=="updateChatAction"){
        const QString chat=idOf(object.value("chat_id"));const QVariantMap sender=object.value("sender_id").toMap();
        const QString user=idOf(sender.value("user_id"));if(user==m_account)return true;
        const QString senderKey=sender.value("@type").toString()+":"+idOf(sender.value(user=="0"?"chat_id":"user_id"));
        const QString key=chat+":"+senderKey,action=object.value("action").toMap().value("@type").toString();
        if(action=="chatActionCancel" || Presence::action(action).isEmpty())m_chatActions.remove(key);
        else if(m_chatActions.size()<128 || m_chatActions.contains(key)){
            QVariantMap value;value.insert("chatId",chat);value.insert("userId",user);value.insert("action",action);value.insert("until",m_clock.elapsed()+6000);m_chatActions.insert(key,value);
        }
        rebuildChats();emit changed();return true;
    }
    return false;
}
QVariantMap TelegramStore::avatarFile(const QString &key) const{
    QVariantMap file=m_avatarFiles.value(key);
    if(file.isEmpty() && key.startsWith("chat:")){
        const QVariantMap chat=m_chats.value(key.mid(5));
        if(chat.value("type")=="chatTypePrivate")file=m_avatarFiles.value("user:"+chat.value("peerId").toString());
    }
    if(m_files.contains(file.value("id").toInt()))file=m_files.value(file.value("id").toInt());
    return file;
}
QString TelegramStore::avatarPath(const QString &key) const{
    const QVariantMap file=avatarFile(key);
    if(qMax(file.value("size").toLongLong(),file.value("expected_size").toLongLong())>1024*1024)return QString();
    return fileInfo(file).value("path").toString();
}
QString TelegramStore::avatarCacheDirectory() const{
    if(m_account.isEmpty())return QString();
    return QDir(m_session->dataDirectory()).filePath("avatars/"+QString::fromLatin1(QCryptographicHash::hash(m_account.toUtf8(),QCryptographicHash::Sha1).toHex()));
}
bool TelegramStore::avatarAffected(const QString &key,const QString &changedKey) const{
    if(changedKey.isEmpty() || key==changedKey)return true;
    return key.startsWith("chat:") && m_chats.value(key.mid(5)).value("type")=="chatTypePrivate" &&
        changedKey=="user:"+m_chats.value(key.mid(5)).value("peerId").toString();
}
void TelegramStore::rememberAvatar(const QString &key,const QVariantMap &photo){
    QVariantMap file=photo.value("small").toMap();
    if(file.value("id").toInt()<=0)file=photo.value("big").toMap();
    const QVariantMap old=m_avatarFiles.value(key);
    if(file.value("id").toInt()>0)m_avatarFiles.insert(key,file);else m_avatarFiles.remove(key);
    if(old!=m_avatarFiles.value(key))emit avatarChanged(key);
}
void TelegramStore::requestAvatar(const QString &key){
    if(!m_ready || !m_online)return;
    const QVariantMap file=avatarFile(key);const int id=file.value("id").toInt();
    if(id<=0 || !avatarPath(key).isEmpty() || m_avatarQueued.contains(id) || m_avatarActive.contains(id) ||
        m_avatarFailed.value(id)>m_clock.elapsed() || qMax(file.value("size").toLongLong(),file.value("expected_size").toLongLong())>1024*1024)return;
    if(m_avatarQueue.size()>=64)return;
    m_avatarQueue.append(id);m_avatarQueued.insert(id);pumpAvatars();
}
void TelegramStore::pumpAvatars(){
    while(m_foreground && m_ready && m_online && m_avatarActive.size()<3 && !m_avatarQueue.isEmpty() && m_requests.size()<48){
        const int id=m_avatarQueue.takeFirst();m_avatarQueued.remove(id);
        QVariantMap call=method("downloadFile");call.insert("file_id",id);call.insert("priority",8);call.insert("offset",0);call.insert("limit",1024*1024);call.insert("synchronous",false);
        const QString request=submit(call,"avatar");if(request.isEmpty())return;
        m_requests[request].insert("fileId",id);m_avatarActive.insert(id,m_clock.elapsed()+60000);
    }
}
void TelegramStore::updateAvatarFile(const QVariantMap &file){
    const int id=file.value("id").toInt();if(id<=0)return;
    QStringList keys;
    for(QMap<QString,QVariantMap>::const_iterator it=m_avatarFiles.constBegin();it!=m_avatarFiles.constEnd();++it)if(it.value().value("id").toInt()==id)keys.append(it.key());
    if(keys.isEmpty())return;
    const QVariantMap old=m_files.value(id);
    if(old.value("local").toMap().value("is_downloading_completed").toBool() &&
       !file.value("local").toMap().value("is_downloading_completed").toBool() &&
       !fileInfo(old).value("path").toString().isEmpty())return;
    m_files.insert(id,file);
    if(file.value("local").toMap().value("is_downloading_completed").toBool()){m_avatarActive.remove(id);m_avatarFailed.remove(id);}
    if(old!=file)foreach(const QString &key,keys)emit avatarChanged(key);
    pumpAvatars();
}
bool TelegramStore::peopleResponse(const QVariantMap &object,const QVariantMap &pending){
    const QString kind=pending.value("kind").toString(),type=object.value("@type").toString();const bool error=type=="error";
    if(kind=="contacts"){
        m_contactsLoading=false;
        if(error)m_contactsError=ru("Не удалось загрузить контакты (код %1).").arg(object.value("code").toInt());
        else{
            m_contactIds.clear();foreach(const QVariant &id,object.value("user_ids").toList()){const QString text=idOf(id);if(text!="0" && !m_contactIds.contains(text))m_contactIds.append(text);}
            foreach(const QString &id,m_contactIds)if(!m_users.contains(id) && m_requests.size()<48){QVariantMap call=method("getUser");call.insert("user_id",number(id));submit(call,"contact-user");}
        }
        rebuildContacts();emit contactsChanged();return true;
    }
    if(kind=="contact-user"){return false;} // The user event is consumed by TelegramStore.
    if(kind=="contact-open"){
        if(pending.value("contactSerial").toUInt()!=m_contactSerial)return true;
        if(error || type!="chat"){m_openingContact=false;m_contactsError=ru("Не удалось открыть диалог (код %1).").arg(object.value("code").toInt());emit contactsChanged();}
        else m_pendingContactChat=idOf(object.value("id"));
        return true; // The following chat event installs metadata before contactChatReady.
    }
    if(kind=="avatar"){
        const int id=pending.value("fileId").toInt();
        if(error){m_avatarActive.remove(id);m_avatarFailed.insert(id,m_clock.elapsed()+60000);}
        else if(type=="file")updateAvatarFile(object);
        pumpAvatars();return true;
    }
    return false;
}
void TelegramStore::peopleTick(){
    if(!m_ready)return;
    bool activityChanged=false;
    QMap<QString,QVariantMap>::iterator it=m_chatActions.begin();
    while(it!=m_chatActions.end())if(it.value().value("until").toLongLong()<=m_clock.elapsed()){it=m_chatActions.erase(it);activityChanged=true;}else ++it;
    bool presenceChanged=m_clock.elapsed()>=m_nextPresenceRefresh;
    if(presenceChanged){
        m_nextPresenceRefresh=m_clock.elapsed()+60000;
        const uint now=QDateTime::currentDateTime().toTime_t();
        foreach(const QVariantMap &user,m_users){
            const QVariantMap status=user.value("status").toMap();const uint expires=status.value("expires").toUInt();
            if(status.value("@type")=="userStatusOnline" && expires>now)m_nextPresenceRefresh=qMin(m_nextPresenceRefresh,m_clock.elapsed()+qlonglong(expires-now)*1000);
        }
        rebuildContacts();
    }
    if(activityChanged)rebuildChats();
    if(activityChanged || presenceChanged)emit changed();
    foreach(const int id,m_avatarActive.keys())if(m_avatarActive.value(id)<=m_clock.elapsed()){m_avatarActive.remove(id);m_avatarFailed.insert(id,m_clock.elapsed()+60000);}
    pumpAvatars();
}
