#include "EmojiView.hpp"
#include "EmojiText.hpp"
#include <bb/cascades/Container>
#include <bb/cascades/Label>
#include <bb/cascades/ImageView>
#include <bb/cascades/Image>
#include <bb/cascades/Color>
#include <bb/cascades/TextFormat>
#include <bb/cascades/TextStyle>
#include <bb/cascades/AbsoluteLayout>
#include <bb/cascades/AbsoluteLayoutProperties>
#include <bb/cascades/LayoutUpdateHandler>
#include <QFile>
#include <QTimer>
#include <QCache>
#include <QtGui/QColor>
using namespace bb::cascades;
namespace {
Image emojiImage(const QString &path){
    static QCache<QString,Image> cache(64);
    if(cache.contains(path))return *cache.object(path);
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return Image();
    Image image(file.readAll());cache.insert(path,new Image(image));return image;
}
}
EmojiView::EmojiView(Container *parent):CustomControl(parent),m_root(new Container),m_ink("#202b33"),m_width(500),m_pixels(28),m_pending(false){
    m_root->setLayout(new AbsoluteLayout);setRoot(m_root);
}
void EmojiView::schedule(){if(!m_pending){m_pending=true;QTimer::singleShot(0,this,SLOT(rebuild()));}}
void EmojiView::setText(const QString &v){if(v==m_text)return;m_text=v;schedule();}
void EmojiView::setTextWidth(qreal v){v=qBound(qreal(20),v,qreal(1500));if(v==m_width)return;m_width=v;schedule();}
void EmojiView::setPixelSize(int v){v=qBound(8,v,72);if(v==m_pixels)return;m_pixels=v;schedule();}
void EmojiView::setInk(const QString &v){if(v==m_ink)return;m_ink=v;schedule();}
void EmojiView::word(const QString &text){
    if(text.isEmpty())return;
    Label *label=new Label;label->setText(text);label->setTextFormat(TextFormat::Plain);label->setMultiline(true);
    label->setMaxWidth(m_width);label->textStyle()->setColor(Color::fromARGB(QColor(m_ink).rgba()));
    label->setTopMargin(0);label->setBottomMargin(0);label->setLeftMargin(0);label->setRightMargin(0);
    label->setLayoutProperties(new AbsoluteLayoutProperties);
    m_root->add(label);Part part;part.control=label;part.size=QSizeF(qMin(m_width,qreal(text.size()*m_pixels*.55)),m_pixels*1.4);m_parts.append(part);
    LayoutUpdateHandler *handler=new LayoutUpdateHandler(label);
    connect(handler,SIGNAL(layoutFrameChanged(QRectF)),this,SLOT(measured(QRectF)));
}
void EmojiView::rebuild(){
    m_pending=false;
    foreach(const Part &part,m_parts)if(part.control)disconnect(part.control,0,this,0);
    m_parts.clear();m_root->removeAll();
    const QVariantList tokens=EmojiRenderer::parts(m_text,":/emoji");
    if(tokens.size()==1 && !tokens.first().toMap().contains("emoji")){word(m_text);arrange();emit changed();return;}
    foreach(const QVariant &value,tokens){
        const QVariantMap token=value.toMap();
        if(token.contains("emoji")){
            ImageView *image=new ImageView;image->setImage(emojiImage(token.value("emoji").toString()));
            image->setPreferredSize(m_pixels,m_pixels);image->setMinWidth(m_pixels);image->setMaxWidth(m_pixels);image->setMinHeight(m_pixels);image->setMaxHeight(m_pixels);
            image->setLayoutProperties(new AbsoluteLayoutProperties);m_root->add(image);
            Part part;part.control=image;part.size=QSizeF(m_pixels,m_pixels);m_parts.append(part);
        }else{
            QString run;const QString text=token.value("text").toString();
            for(int i=0;i<text.size();++i){
                QChar c=text.at(i);
                if(!c.isSpace()){run+=c;continue;}
                word(run);run.clear();Part part;part.newline=c=='\n';part.space=!part.newline;part.size=QSizeF(c=='\t'?m_pixels*1.2:m_pixels*.28,m_pixels*1.4);m_parts.append(part);
            }
            word(run);
        }
    }
    arrange();emit changed();
}
void EmojiView::measured(const QRectF &rect){
    LayoutUpdateHandler *handler=qobject_cast<LayoutUpdateHandler*>(sender());if(!handler || rect.width()<=0 || rect.height()<=0)return;
    for(int i=0;i<m_parts.size();++i)if(m_parts[i].control==handler->parent()){
        if(m_parts[i].size==rect.size())return;
        m_parts[i].size=rect.size();arrange();return;
    }
}
void EmojiView::arrange(){
    qreal x=0,y=0,line=m_pixels*1.4,widest=0;
    for(int i=0;i<m_parts.size();++i){
        const Part &part=m_parts.at(i);
        if(part.newline){widest=qMax(widest,x);y+=line;x=0;line=m_pixels*1.4;continue;}
        if(x>0 && x+part.size.width()>m_width){widest=qMax(widest,x);y+=line;x=0;line=m_pixels*1.4;}
        if(part.control){
            AbsoluteLayoutProperties *layout=qobject_cast<AbsoluteLayoutProperties*>(part.control->layoutProperties());
            layout->setPositionX(x);layout->setPositionY(y);
        }
        x+=part.size.width();line=qMax(line,part.size.height());
    }
    widest=qMax(widest,x);setPreferredWidth(qMin(m_width,widest));setPreferredHeight(m_text.isEmpty()?0:y+line);
}
