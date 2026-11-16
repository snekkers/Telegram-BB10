#ifndef BERRYGRAM_PRIVATE_FILE_HPP
#define BERRYGRAM_PRIVATE_FILE_HPP
#include <QString>
#include <QVariantMap>
namespace PrivateFile {
bool read(const QString &path, QVariantMap *value);
bool write(const QString &path, const QVariantMap &value);
}
#endif
