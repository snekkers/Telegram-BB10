#ifndef BERRYGRAM_PRIVATE_FILE_HPP
#define BERRYGRAM_PRIVATE_FILE_HPP
#include <QString>
#include <QVariantMap>
namespace PrivateFile {
bool read(const QString &path, QVariantMap *value, int maximumBytes = 65536);
bool write(const QString &path, const QVariantMap &value, int maximumBytes = 65536);
}
#endif
