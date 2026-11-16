#ifndef BERRYGRAM_JSON_CODEC_HPP
#define BERRYGRAM_JSON_CODEC_HPP

#include <QByteArray>
#include <QVariantMap>

namespace JsonCodec {
bool decode(const QByteArray &bytes, QVariantMap *object);
QByteArray encode(const QVariantMap &object);
}
#endif
