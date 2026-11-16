#include "PrivateFile.hpp"
#include "JsonCodec.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

bool PrivateFile::read(const QString &path, QVariantMap *value)
{
    const QByteArray name = QFile::encodeName(path);
    struct stat info;
    if (::lstat(name.constData(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > 65536 ||
        !QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner)) return false;
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && JsonCodec::decode(file.read(65537), value);
}

bool PrivateFile::write(const QString &path, const QVariantMap &value)
{
    QFileInfo destination(path);
    const QString directory = destination.absolutePath();
    if ((!QFileInfo(directory).exists() && !QDir().mkpath(directory)) ||
        QFileInfo(directory).isSymLink() || QFileInfo(path).isSymLink() ||
        !QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) return false;
    QByteArray bytes = JsonCodec::encode(value);
    if (bytes.isEmpty() || bytes.size() > 65536) return false;
    QTemporaryFile temporary(directory + "/.private-XXXXXX");
    if (!temporary.open() || !temporary.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
        temporary.write(bytes) != bytes.size() || !temporary.flush() || ::fsync(temporary.handle()) != 0) return false;
    const QByteArray source = QFile::encodeName(temporary.fileName());
    const QByteArray target = QFile::encodeName(path);
    if (::rename(source.constData(), target.constData()) != 0) return false;
    temporary.setAutoRemove(false);
    return true;
}
