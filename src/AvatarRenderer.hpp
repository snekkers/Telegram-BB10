#ifndef BERRYGRAM_AVATAR_RENDERER_HPP
#define BERRYGRAM_AVATAR_RENDERER_HPP
#include <QString>
#include <QMap>
class AvatarRenderer {
public:
    QString render(const QString &source,const QString &directory);
    QString demo(const QString &key,const QString &directory);
private:
    QMap<QString,QString> m_cached;
};
#endif
