#include "SearchController.hpp"
#include "ChatController.hpp"
SearchController::SearchController(TelegramSearch *search,ChatController *demo,QObject *parent)
    : QObject(parent),m_search(search),m_demoSource(demo),m_rows(new bb::cascades::ArrayDataModel(this)),m_demo(false),m_mode(0),m_filter(0),m_demoLimit(8) {
    connect(search,SIGNAL(changed()),this,SLOT(update()));
}
void SearchController::begin(bool demo) { cancel();m_demo=demo;m_query.clear();m_mode=m_filter=0;update(); }
void SearchController::search(const QString &query,int mode,int filter) {
    m_demoLimit=8;
    m_query=query.trimmed().left(256);m_mode=mode;m_filter=filter;
    if(!m_demo)m_search->search(query,mode,filter);
    update();
}
void SearchController::update() {
    if(!m_demo && !m_search->ready())m_liveRecent.clear();
    QVariantList rows=m_demo?m_demoSource->search(m_query,m_mode,m_filter):m_search->rows();
    if(m_query.isEmpty() && !(m_mode==2 && m_filter>0)) {
        rows.clear();foreach(const QString &query,recentQueries()) {
            QVariantMap row;row.insert("key","recent:"+query);row.insert("title",query);row.insert("chatId","recent:"+query);row.insert("recentQuery",query);
            row.insert("preview",QString::fromUtf8("Повторить поиск"));row.insert("time",QString());row.insert("unreadLabel",QString());rows.append(row);
        }
    }
    if(m_demo && m_mode==2 && rows.size()>m_demoLimit)rows=rows.mid(0,m_demoLimit);
    // A query change deliberately replaces the result list; chat history remains untouched.
    for(int i=0;i<rows.size();++i) {
        const QVariantMap next=rows.at(i).toMap();
        if(i>=m_rows->size())m_rows->append(QVariant(next));
        else if(m_rows->value(i).toMap().value("key")!=next.value("key"))m_rows->insert(i,QVariant(next));
        else if(m_rows->value(i).toMap()!=next)m_rows->replace(i,QVariant(next));
    }
    while(m_rows->size()>rows.size())m_rows->removeAt(m_rows->size()-1);
    emit changed();
}
void SearchController::cancel() { m_query.clear();m_search->cancel();m_rows->clear();emit changed(); }
void SearchController::retry() { if(m_demo)update();else m_search->retry(); }
void SearchController::remember() {
    if(m_query.isEmpty())return;QStringList &recent=m_demo?m_demoRecent:m_liveRecent;
    recent.removeAll(m_query);recent.prepend(m_query);while(recent.size()>8)recent.removeLast();emit changed();
}
void SearchController::clearRecent() { (m_demo?m_demoRecent:m_liveRecent).clear();update(); }

bool SearchController::canLoadMore() const {
    return m_demo ? m_mode==2 && m_demoSource->search(m_query,m_mode,m_filter).size()>m_demoLimit : m_search->canLoadMore();
}
void SearchController::loadMore() {
    if(!canLoadMore())return;
    if(m_demo){m_demoLimit+=8;update();}else m_search->loadMore();
}
