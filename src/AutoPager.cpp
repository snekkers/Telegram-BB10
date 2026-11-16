#include "AutoPager.hpp"
AutoPager::AutoPager(QObject *parent, int settleMs)
    : QObject(parent), m_enabled(false), m_atEdge(false), m_count(0)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(qMax(1, settleMs));
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(request()));
}
void AutoPager::schedule()
{
    if (m_enabled && m_atEdge) m_timer.start();
    else m_timer.stop();
}
void AutoPager::setEnabled(bool value)
{
    if (m_enabled == value) return;
    m_enabled = value; schedule(); emit changed();
}
void AutoPager::setAtEdge(bool value)
{
    if (m_atEdge == value) return;
    m_atEdge = value; schedule(); emit changed();
}
void AutoPager::setContentCount(int value)
{
    if (m_count == value) return;
    m_count = value; schedule(); emit changed();
}
void AutoPager::request()
{
    // One request per state/content change. A failed or no-op callback can't spin.
    if (m_enabled && m_atEdge) emit requestMore();
}
