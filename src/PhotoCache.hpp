#ifndef BERRYGRAM_PHOTO_CACHE_HPP
#define BERRYGRAM_PHOTO_CACHE_HPP
#include <QString>
class PhotoCache {
public:
    static QString scaled(const QString &path,const QString &folder,int edge);
};
#endif
