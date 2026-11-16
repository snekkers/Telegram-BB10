#ifndef BERRYGRAM_FORMATTED_TEXT_HPP
#define BERRYGRAM_FORMATTED_TEXT_HPP
#include <QString>
#include <QVariant>
#include <QList>
#include <QUrl>
#include <algorithm>
namespace FormattedText {
inline QString html(const QString &piece,int start,const QVariantMap &formatted){
    QList<int> cuts;cuts<<0<<piece.size();const int end=start+piece.size();const QVariantList entities=formatted.value("entities").toList();
    foreach(const QVariant &value,entities){const QVariantMap e=value.toMap();const int at=e.value("offset").toInt(),len=e.value("length").toInt();if(at<0 || len<=0 || at>end || len>65536 || at+len<start)continue;const int left=qBound(0,at-start,piece.size()),right=qBound(0,at+len-start,piece.size());
        if(left==0 || left==piece.size() || !piece.at(left).isLowSurrogate() || !piece.at(left-1).isHighSurrogate())cuts<<left;
        if(right==0 || right==piece.size() || !piece.at(right).isLowSurrogate() || !piece.at(right-1).isHighSurrogate())cuts<<right;}
    std::sort(cuts.begin(),cuts.end());QString output;
    for(int i=1;i<cuts.size();++i){int from=cuts.at(i-1),to=cuts.at(i);if(from==to)continue;
        bool bold=false,italic=false,code=false,underline=false;QString href;
        foreach(const QVariant &value,entities){const QVariantMap e=value.toMap();const int at=e.value("offset").toInt(),len=e.value("length").toInt();if(at<0 || len<=0 || len>65536 || at>start+from || at+len<start+to)continue;const QString type=e.value("type").toMap().value("@type").toString();bold|=type=="textEntityTypeBold";italic|=type=="textEntityTypeItalic";code|=type=="textEntityTypeCode" || type=="textEntityTypePre" || type=="textEntityTypePreCode";underline|=type=="textEntityTypeUnderline" || type=="textEntityTypeUrl" || type=="textEntityTypeTextUrl" || type=="textEntityTypeMention";if(type=="textEntityTypeTextUrl")href=e.value("type").toMap().value("url").toString();else if(type=="textEntityTypeUrl" || type=="textEntityTypeMention")href=formatted.value("text").toString().mid(at,len);}
        if(href.startsWith("@"))href="https://t.me/"+href.mid(1);else if(href.startsWith("t.me/"))href="https://"+href;
        const QUrl url(href);if(!url.isValid() || (url.scheme()!="http" && url.scheme()!="https") || url.host().isEmpty() || !url.userInfo().isEmpty())href.clear();
        if(!href.isEmpty()){href.replace("&","&amp;");href.replace("\"","&quot;");href.replace("<","&lt;");href.replace(">","&gt;");output+="<a href=\""+href+"\">";}
        if(bold)output+="<b>";
        if(italic)output+="<i>";
        if(code)output+="<span style=\"font-family:monospace\">";
        if(underline)output+="<span style=\"text-decoration:underline\">";
        QString text=piece.mid(from,to-from);text.replace("&","&amp;");text.replace("<","&lt;");text.replace(">","&gt;");text.replace("\n","<br/>");output+=text;
        if(underline)output+="</span>";
        if(code)output+="</span>";
        if(italic)output+="</i>";
        if(bold)output+="</b>";
        if(!href.isEmpty())output+="</a>";
    }return output;
}
}
#endif
