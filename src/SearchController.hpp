#ifndef BERRYGRAM_SEARCH_CONTROLLER_HPP
#define BERRYGRAM_SEARCH_CONTROLLER_HPP
#include "TelegramSearch.hpp"
#include <bb/cascades/ArrayDataModel>
class ChatController;
class SearchController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bb::cascades::DataModel* results READ results CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QStringList recentQueries READ recentQueries NOTIFY changed)
public:
    SearchController(TelegramSearch *search,ChatController *demo,QObject *parent=0);
    bb::cascades::DataModel *results() const { return m_rows; }
    bool loading() const { return !m_demo && m_search->loading(); }
    bool canLoadMore() const { return !m_demo && m_search->canLoadMore(); }
    int count() const { return m_rows->size(); }
    QString errorText() const { return m_demo?QString():m_search->errorText(); }
    QStringList recentQueries() const { return m_demo?m_demoRecent:m_liveRecent; }
    Q_INVOKABLE void begin(bool demo);
    Q_INVOKABLE void search(const QString &query,int mode,int filter);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void loadMore() { if(!m_demo)m_search->loadMore(); }
    Q_INVOKABLE void retry();
    Q_INVOKABLE void remember();
    Q_INVOKABLE void clearRecent();
signals:
    void changed();
private slots:
    void update();
private:
    TelegramSearch *m_search;ChatController *m_demoSource;
    bb::cascades::ArrayDataModel *m_rows;
    bool m_demo;QString m_query;int m_mode,m_filter;
    QStringList m_liveRecent,m_demoRecent;
};
#endif
