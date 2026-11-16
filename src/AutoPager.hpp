#ifndef BERRYGRAM_AUTO_PAGER_HPP
#define BERRYGRAM_AUTO_PAGER_HPP
#include <QObject>
#include <QTimer>

// Wait for Cascades' asynchronous scroll/layout state to settle before paging.
class AutoPager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool atEdge READ atEdge WRITE setAtEdge NOTIFY changed)
    Q_PROPERTY(int contentCount READ contentCount WRITE setContentCount NOTIFY changed)
public:
    explicit AutoPager(QObject *parent = 0, int settleMs = 200);
    bool enabled() const { return m_enabled; }
    bool atEdge() const { return m_atEdge; }
    int contentCount() const { return m_count; }
    void setEnabled(bool value);
    void setAtEdge(bool value);
    void setContentCount(int value);
signals:
    void changed();
    void requestMore();
private slots:
    void request();
private:
    void schedule();
    QTimer m_timer;
    bool m_enabled, m_atEdge;
    int m_count;
};
#endif
