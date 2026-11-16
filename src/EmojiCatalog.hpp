#ifndef BERRYGRAM_EMOJI_CATALOG_HPP
#define BERRYGRAM_EMOJI_CATALOG_HPP
#include <QObject>
#include <bb/cascades/ArrayDataModel>
class EmojiCatalog:public QObject {
    Q_OBJECT
    Q_PROPERTY(bb::cascades::DataModel* entries READ entries CONSTANT)
    Q_PROPERTY(QString position READ position NOTIFY changed)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY changed)
    Q_PROPERTY(bool canNext READ canNext NOTIFY changed)
public:
    explicit EmojiCatalog(QObject *parent=0);
    bb::cascades::DataModel *entries() const{return m_entries;}
    QString position() const;bool canPrevious() const{return m_page>0;}bool canNext() const{return (m_page+1)*30<m_symbols.size();}
    Q_INVOKABLE void setCategory(int index);
    Q_INVOKABLE void movePage(int delta);
    Q_INVOKABLE void remember(const QString &symbol);
    Q_INVOKABLE void release(){m_entries->clear();m_symbols.clear();}
signals:void changed();
private:void rebuild();bb::cascades::ArrayDataModel *m_entries;QStringList m_symbols,m_recent;int m_page;
};
#endif
