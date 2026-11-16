#ifndef BERRYGRAM_EMOJI_VIEW_HPP
#define BERRYGRAM_EMOJI_VIEW_HPP
#include <bb/cascades/CustomControl>
#include <QList>
#include <QSizeF>
#include <QVariantMap>
namespace bb {namespace cascades {class Container;class Control;class ActiveTextTriggeredEvent;}}
class EmojiView:public bb::cascades::CustomControl {
    Q_OBJECT
    Q_PROPERTY(QVariantMap formatted READ formatted WRITE setFormatted NOTIFY changed)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(bool fillWidth READ fillWidth WRITE setFillWidth NOTIFY changed)
    Q_PROPERTY(qreal textWidth READ textWidth WRITE setTextWidth NOTIFY changed)
    Q_PROPERTY(int pixelSize READ pixelSize WRITE setPixelSize NOTIFY changed)
    Q_PROPERTY(int textSize READ textSize WRITE setTextSize NOTIFY changed)
    Q_PROPERTY(QString ink READ ink WRITE setInk NOTIFY changed)
public:
    explicit EmojiView(bb::cascades::Container *parent=0);
    QVariantMap formatted() const{return m_formatted;}void setFormatted(const QVariantMap &v);
    QString text() const{return m_text;}void setText(const QString &v);
    bool fillWidth() const {return m_fillWidth;}void setFillWidth(bool value);
    qreal textWidth() const{return m_width;}void setTextWidth(qreal v);
    int pixelSize() const{return m_pixels;}void setPixelSize(int v);
    int textSize() const{return m_textSize;}void setTextSize(int v);
    QString ink() const{return m_ink;}void setInk(const QString &v);
signals:
    void changed();
    void linkActivated(const QString &url);
private slots:
    void rebuild();
    void measured(const QRectF &rect);
    void activateLink(bb::cascades::ActiveTextTriggeredEvent *event);
private:
    struct Part {bb::cascades::Control *control;QSizeF size;bool newline,space;Part():control(0),newline(false),space(false){}};
    void schedule();void arrange();void word(const QString &text,bool paragraph=false);
    bb::cascades::Container *m_root;
    QList<Part> m_parts;
    QString m_text,m_ink;QVariantMap m_formatted;int m_offset;
    qreal m_width;int m_pixels,m_textSize;bool m_pending,m_fillWidth;
};
#endif
