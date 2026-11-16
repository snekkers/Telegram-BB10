#include "JsonCodec.hpp"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QJsonDocument>
#include <QJsonParseError>
#else
#include <bb/data/JsonDataAccess>
#endif

bool JsonCodec::decode(const QByteArray &bytes, QVariantMap *object)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    *object = document.toVariant().toMap();
#else
    bb::data::JsonDataAccess json;
    QVariant result = json.loadFromBuffer(bytes);
    if (json.hasError() || result.type() != QVariant::Map)
        return false;
    *object = result.toMap();
#endif
    return true;
}

QByteArray JsonCodec::encode(const QVariantMap &object)
{
    QByteArray bytes;
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    bytes = QJsonDocument::fromVariant(object).toJson(QJsonDocument::Indented);
#else
    bb::data::JsonDataAccess json;
    json.saveToBuffer(object, &bytes);
    if (json.hasError())
        return QByteArray();
#endif
    // JsonDataAccess can pretty-print. The pipe protocol requires one JSON line.
    // Remove only whitespace outside strings; escaped quotes and UTF-8 survive.
    QByteArray compact;
    bool quoted = false;
    bool escaped = false;
    for (int i = 0; i < bytes.size(); ++i) {
        const char c = bytes.at(i);
        if (!quoted && (c == ' ' || c == '\t' || c == '\n' || c == '\r'))
            continue;
        compact.append(c);
        if (escaped)
            escaped = false;
        else if (quoted && c == '\\')
            escaped = true;
        else if (c == '"')
            quoted = !quoted;
    }
    return compact;
}
