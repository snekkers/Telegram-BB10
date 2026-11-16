#ifndef BERRYGRAM_EMOJI_VIEW_HPP
#define BERRYGRAM_EMOJI_VIEW_HPP
#include <bb/cascades/CustomControl>
#include <QList>
#include <QSizeF>
namespace bb {namespace cascades {class Container;class Control;}}
class EmojiView:public bb::cascades::CustomControl {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(qreal textWidth READ textWidth WRITE setTextWidth NOTIFY changed)
    Q_PROPERTY(int pixelSize READ pixelSize WRITE setPixelSize NOTIFY changed)
    Q_PROPERTY(QString ink READ ink WRITE setInk NOTIFY changed)
public:
    explicit EmojiView(bb::cascades::Container *parent=0);
    QString text() const{return m_text;}void setText(const QString &v);
    qreal textWidth() const{return m_width;}void setTextWidth(qreal v);
    int pixelSize() const{return m_pixels;}void setPixelSize(int v);
    QString ink() const{return m_ink;}void setInk(const QString &v);
signals:
    void changed();
private slots:
    void rebuild();
    void measured(const QRectF &rect);
private:
    struct Part {bb::cascades::Control *control;QSizeF size;bool newline,space;Part():control(0),newline(false),space(false){}};
    void schedule();void arrange();void word(const QString &text);
    bb::cascades::Container *m_root;
    QList<Part> m_parts;
    QString m_text,m_ink;
    qreal m_width;int m_pixels;bool m_pending;
};
#endif
