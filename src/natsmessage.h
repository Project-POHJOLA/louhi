#ifndef NATSMESSAGE_H
#define NATSMESSAGE_H

#include <QString>
#include <QDateTime>

struct NatsMessage {
    QString topic;
    QString payload;
    QString senderId;
    QDateTime timestamp;

    NatsMessage() = default;
    NatsMessage(const QString& t, const QString& p)
        : topic(t), payload(p), timestamp(QDateTime::currentDateTime()) {}
};

#endif