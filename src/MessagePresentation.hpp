#ifndef BERRYGRAM_MESSAGE_PRESENTATION_HPP
#define BERRYGRAM_MESSAGE_PRESENTATION_HPP
#include <QDateTime>
#include <QLocale>
#include <QVariantList>

// Shared presentation for cached Telegram messages and the offline demo.
namespace MessagePresentation {
inline QString dateLabel(const QDate &date, const QDate &today)
{
    if (date == today) return QString::fromUtf8("Сегодня");
    if (date == today.addDays(-1)) return QString::fromUtf8("Вчера");
    return QLocale("ru_RU").toString(date,
        date.year() == today.year() ? "d MMMM" : "d MMMM yyyy");
}

inline QVariantList decorate(const QVariantList &messages, bool group,
                            const QString &readInbox = QString(), bool hasUnread = false,
                            const QDate &today = QDate::currentDate())
{
    QVariantList result;
    QDate previousDate;
    QString previousSender;
    uint previousTime = 0;
    bool placedUnread = false;
    for (int i = 0; i < messages.size(); ++i) {
        QVariantMap row = messages.at(i).toMap();
        const uint stamp = row.value("date").toUInt();
        const QDateTime time = QDateTime::fromTime_t(stamp);
        const QDate day = stamp ? time.date() : today;
        const bool outgoing = row.value("outgoing").toBool();
        QString sender = outgoing ? QString("self") : "user:" + row.value("senderId").toString();
        if (!outgoing && row.value("senderId").toString().isEmpty())
            sender = "chat:" + row.value("senderChatId").toString() + ":" + row.value("author").toString();
        const bool startsDay = i == 0 || day != previousDate;
        const bool startsUnread = !placedUnread && hasUnread && !outgoing &&
            !readInbox.isEmpty() && row.value("messageId").toLongLong() > readInbox.toLongLong();
        const bool joined = i > 0 && !startsDay && !startsUnread && sender == previousSender &&
            stamp != 0 && previousTime != 0 && stamp >= previousTime && stamp - previousTime <= 300;
        row.insert("startsDay", startsDay);
        row.insert("dateLabel", dateLabel(day, today));
        row.insert("startsUnread", startsUnread);
        row.insert("joinedPrevious", joined);
        row.insert("showAuthor", group && !outgoing && !joined);
        if (stamp) row.insert("time", time.toString("HH:mm"));
        result.append(row);
        previousDate = day; previousSender = sender; previousTime = stamp;
        placedUnread = placedUnread || startsUnread;
    }
    return result;
}
}
#endif
