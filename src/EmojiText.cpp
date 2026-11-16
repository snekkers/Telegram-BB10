#include "EmojiText.hpp"
#include <QDir>
#include <QFile>
#include <QSet>
#include <QMap>
#include <QStringList>

namespace {
struct Match { QString name; int length; Match():length(0){} };
const QMap<QString,QString> &names(const QString &folder) {
    static QString cachedFolder;
    static QMap<QString,QString> result;
    if(cachedFolder!=folder) {
        cachedFolder=folder;result.clear();
        foreach(const QString &name,QDir(folder).entryList(QStringList()<<"*.png",QDir::Files)){QString key=name.left(name.size()-4);key.replace("-fe0f","");result.insert(key,name.left(name.size()-4));}
    }
    return result;
}
Match match(const QString &text,int start,const QMap<QString,QString> &available) {
    Match best;QString key;
    // Longest match keeps flags, modifiers, keycaps and ZWJ families together.
    for(int i=start,points=0;i<text.size() && points<32;++points) {
        uint cp=text.at(i++).unicode();
        if(cp>=0xd800 && cp<=0xdbff && i<text.size() && text.at(i).unicode()>=0xdc00 && text.at(i).unicode()<=0xdfff)
            cp=0x10000+((cp-0xd800)<<10)+(text.at(i++).unicode()-0xdc00);
        if(cp!=0xfe0f) {if(!key.isEmpty())key+='-';key+=QString::number(cp,16);}
        if(available.contains(key)){best.name=available.value(key);best.length=i-start;}
        if(points==0 && cp<0x2000 && cp!='#' && cp!='*' && (cp<'0' || cp>'9') && cp!=0xa9 && cp!=0xae)break;
    }
    return best;
}
}
bool EmojiRenderer::contains(const QString &text,const QString &assets) {
    const QMap<QString,QString> &available=names(assets);
    for(int i=0;i<text.size();++i)if(match(text,i,available).length)return true;
    return false;
}
QVariantList EmojiRenderer::parts(const QString &text,const QString &assets) {
    QVariantList result;QString plain;
    const QMap<QString,QString> &available=names(assets);
    for(int i=0;i<text.size();) {
        const Match found=match(text,i,available);
        if(!found.length){plain+=text.at(i++);continue;}
        if(!plain.isEmpty()){QVariantMap item;item.insert("text",plain);result.append(item);plain.clear();}
        QVariantMap item;item.insert("emoji",QDir(assets).filePath(found.name+".png"));item.insert("text",text.mid(i,found.length));result.append(item);i+=found.length;
    }
    if(!plain.isEmpty()){QVariantMap item;item.insert("text",plain);result.append(item);}
    return result;
}
