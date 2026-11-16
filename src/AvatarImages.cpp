#include "AvatarImages.hpp"
#include "TelegramStore.hpp"
#include <QDir>
AvatarProvider::AvatarProvider(TelegramStore *store,QObject *parent):QObject(parent),m_store(store){
    connect(store,SIGNAL(avatarChanged(QString)),this,SIGNAL(avatarChanged(QString)));
}
QString AvatarProvider::url(const QString &key){
    if(key.startsWith("demo:"))return m_renderer.demo(key,QDir::currentPath()+"/data/demo-avatars");
    return m_renderer.render(m_store->avatarPath(key),m_store->avatarCacheDirectory());
}
void AvatarProvider::request(const QString &key){if(!key.startsWith("demo:"))m_store->requestAvatar(key);}
bool AvatarProvider::affected(const QString &key,const QString &changedKey) const{return m_store->avatarAffected(key,changedKey);}
void AvatarImage::setProvider(QObject *provider){
    AvatarProvider *next=qobject_cast<AvatarProvider*>(provider);if(next==m_provider.data())return;
    if(m_provider)disconnect(m_provider.data(),0,this,0);m_provider=next;
    if(m_provider)connect(m_provider.data(),SIGNAL(avatarChanged(QString)),this,SLOT(refresh(QString)));
    refresh();
}
void AvatarImage::setKey(const QString &key){if(key==m_key)return;m_key=key;refresh();}
void AvatarImage::refresh(const QString &changedKey){
    if(!m_provider || !m_provider->affected(m_key,changedKey))return;
    const QString source=m_key.isEmpty()?QString():m_provider->url(m_key);
    if(m_source!=source){m_source=source;emit changed();}
    if(!m_key.isEmpty())m_provider->request(m_key);
}
