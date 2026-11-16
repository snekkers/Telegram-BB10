#ifndef BERRYGRAM_UI_TOOLS_HPP
#define BERRYGRAM_UI_TOOLS_HPP
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
class ChatController;
class TelegramController;
namespace bb { namespace cascades { class AbstractPane; } }

class UiTools : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checksEnabled READ checksEnabled CONSTANT)
public:
    UiTools(ChatController *demo, TelegramController *live, QObject *parent = 0);
    void setRoot(bb::cascades::AbstractPane *root);
    bool checksEnabled() const { return m_checks; }
    Q_INVOKABLE void openMenu(QObject *page);
    Q_INVOKABLE void copyText(const QString &text);
private slots:
    void checkRequest();
    void finishCheck();
private:
    bool capture(const QString &path);
    QPointer<bb::cascades::AbstractPane> m_root;
    ChatController *m_demo;
    TelegramController *m_live;
    QTimer m_timer;
    QString m_directory, m_id;
    QVariantMap m_result;
    bool m_checks, m_pending;
};
#endif
