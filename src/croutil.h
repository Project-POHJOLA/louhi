#ifndef CROUTIL_H
#define CROUTIL_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

class CryUtil
{
public:
    static QString encrypt(const QString& plaintext);
    static QString decrypt(const QString& ciphertext);

    static void encryptConfig(QJsonObject& config);
    static void decryptConfig(QJsonObject& config);

private:
    static QByteArray deriveKey();
    static bool isEncryptedValue(const QString& str);
    static void walkJsonValue(QJsonValue& val, const QStringList& fields, bool doEncrypt);
    static void encryptObject(QJsonObject& obj, const QStringList& fields);
    static void decryptObject(QJsonObject& obj, const QStringList& fields);
};

#endif
