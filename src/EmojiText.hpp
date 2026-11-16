#ifndef BERRYGRAM_EMOJI_TEXT_HPP
#define BERRYGRAM_EMOJI_TEXT_HPP
#include <QString>
#include <QVariant>
class EmojiRenderer {
public:
    static bool contains(const QString &text,const QString &assets);
    static QVariantList parts(const QString &text,const QString &assets);
};
#endif
