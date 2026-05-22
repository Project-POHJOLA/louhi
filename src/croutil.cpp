#include "croutil.h"
#include <QCryptographicHash>
#include <QSysInfo>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

static const QString ENC_PREFIX = QStringLiteral("#enc#");
static const QStringList SENSITIVE_FIELDS = {
    QStringLiteral("certPassword"),
    QStringLiteral("certFilePath"),
    QStringLiteral("certData"),
    QStringLiteral("url")
};

QString CryUtil::encrypt(const QString& plaintext)
{
    if (plaintext.isEmpty())
        return plaintext;

    QByteArray key = deriveKey();

    QByteArray iv(16, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), 16) != 1) {
        qWarning() << "CryUtil: Failed to generate random IV";
        return plaintext;
    }

    QByteArray plain = plaintext.toUtf8();

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "CryUtil: Failed to create cipher context";
        return plaintext;
    }

    QByteArray cipher(plain.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int cipherLen = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                       reinterpret_cast<const unsigned char*>(key.constData()),
                       reinterpret_cast<const unsigned char*>(iv.constData()));

    EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(cipher.data()), &cipherLen,
                      reinterpret_cast<const unsigned char*>(plain.constData()), plain.size());

    int finalLen = 0;
    EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(cipher.data()) + cipherLen, &finalLen);
    cipherLen += finalLen;
    cipher.resize(cipherLen);

    EVP_CIPHER_CTX_free(ctx);

    QByteArray combined = iv + cipher;
    return ENC_PREFIX + QString::fromLatin1(combined.toBase64());
}

QString CryUtil::decrypt(const QString& ciphertext)
{
    if (ciphertext.isEmpty())
        return ciphertext;

    if (!isEncryptedValue(ciphertext))
        return ciphertext;

    QByteArray key = deriveKey();

    QByteArray combined = QByteArray::fromBase64(
        ciphertext.mid(ENC_PREFIX.size()).toLatin1());

    if (combined.size() < 17) {
        qWarning() << "CryUtil: Invalid ciphertext length for" << ciphertext.left(20) << "...";
        return QString();
    }

    QByteArray iv = combined.left(16);
    QByteArray cipher = combined.mid(16);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        qWarning() << "CryUtil: Failed to create cipher context";
        return QString();
    }

    QByteArray plain(cipher.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int plainLen = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                       reinterpret_cast<const unsigned char*>(key.constData()),
                       reinterpret_cast<const unsigned char*>(iv.constData()));

    EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plain.data()), &plainLen,
                      reinterpret_cast<const unsigned char*>(cipher.constData()), cipher.size());

    int finalLen = 0;
    int ret = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plain.data()) + plainLen, &finalLen);
    if (ret != 1) {
        qWarning() << "CryUtil: Decryption failed (wrong key or corrupted data)";
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    plainLen += finalLen;
    plain.resize(plainLen);

    EVP_CIPHER_CTX_free(ctx);

    return QString::fromUtf8(plain);
}

QByteArray CryUtil::deriveKey()
{
    QByteArray machineId = QSysInfo::machineUniqueId();

    if (machineId.isEmpty()) {
        qWarning() << "CryUtil: No machine unique ID available, encryption key will be weak!";
        machineId = QByteArrayLiteral("louhi-fallback-key");
    }

    QString input = QStringLiteral("louhi-crypto-v1/") + QString::fromLatin1(machineId);
    return QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
}

bool CryUtil::isEncryptedValue(const QString& str)
{
    return str.startsWith(ENC_PREFIX);
}

void CryUtil::encryptConfig(QJsonObject& config)
{
    encryptObject(config, SENSITIVE_FIELDS);
}

void CryUtil::decryptConfig(QJsonObject& config)
{
    decryptObject(config, SENSITIVE_FIELDS);
}

void CryUtil::walkJsonValue(QJsonValue& val, const QStringList& fields, bool doEncrypt)
{
    if (val.isObject()) {
        QJsonObject obj = val.toObject();
        if (doEncrypt)
            encryptObject(obj, fields);
        else
            decryptObject(obj, fields);
        val = obj;
    } else if (val.isArray()) {
        QJsonArray arr = val.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonValue elem = arr[i];
            walkJsonValue(elem, fields, doEncrypt);
            arr[i] = elem;
        }
        val = arr;
    }
}

void CryUtil::encryptObject(QJsonObject& obj, const QStringList& fields)
{
    QStringList keys = obj.keys();
    for (const QString& key : keys) {
        QJsonValue val = obj.value(key);

        if (val.isString() && fields.contains(key)) {
            QString s = val.toString();
            if (!isEncryptedValue(s))
                obj[key] = encrypt(s);
        } else {
            walkJsonValue(val, fields, true);
            obj[key] = val;
        }
    }
}

void CryUtil::decryptObject(QJsonObject& obj, const QStringList& fields)
{
    QStringList keys = obj.keys();
    for (const QString& key : keys) {
        QJsonValue val = obj.value(key);

        if (val.isString() && fields.contains(key)) {
            QString s = val.toString();
            if (isEncryptedValue(s))
                obj[key] = decrypt(s);
        } else {
            walkJsonValue(val, fields, false);
            obj[key] = val;
        }
    }
}
