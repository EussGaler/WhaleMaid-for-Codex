#include "CodexStatusBridge.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedPointer>

namespace
{
const QString ServerName = QStringLiteral("WhaleMaidPet.CodexStatus.v2");
}

CodexStatusBridge::CodexStatusBridge(QObject* parent)
    : QObject(parent)
    , server_(new QLocalServer(this))
{
    connect(server_, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* socket = server_->nextPendingConnection())
        {
            const auto payload = QSharedPointer<QByteArray>::create();
            const auto handled = QSharedPointer<bool>::create(false);
            auto processPayload = [this, socket, payload, handled]() {
                if (*handled)
                {
                    return;
                }
                payload->append(socket->readAll());

                QJsonParseError error;
                const QJsonDocument document = QJsonDocument::fromJson(*payload, &error);
                if (error.error != QJsonParseError::NoError || !document.isObject())
                {
                    return;
                }

                const QJsonObject object = document.object();
                const QString status = object.value(QStringLiteral("status"))
                                           .toString()
                                           .trimmed()
                                           .toLower();
                if (!status.isEmpty())
                {
                    *handled = true;
                    Q_EMIT eventReceived(
                        status,
                        object.value(QStringLiteral("message")).toString().trimmed(),
                        object.value(QStringLiteral("sessionId")).toString().trimmed(),
                        object.value(QStringLiteral("turnId")).toString().trimmed());
                    socket->write("ok");
                    socket->flush();
                    socket->disconnectFromServer();
                }
            };
            connect(socket, &QLocalSocket::readyRead, socket, processPayload);
            connect(socket, &QLocalSocket::disconnected,
                    socket, &QLocalSocket::deleteLater);

            // Data can already be buffered by the time nextPendingConnection() returns.
            // Queue one read so short-lived status clients cannot race the readyRead hookup.
            QMetaObject::invokeMethod(socket, processPayload, Qt::QueuedConnection);
        }
    });
}

bool CodexStatusBridge::start()
{
    QLocalServer::removeServer(ServerName);
    return server_->listen(ServerName);
}

bool CodexStatusBridge::sendEvent(
    const QString& status,
    const QString& message,
    const QString& sessionId,
    const QString& turnId,
    const QString& hookEvent)
{
    const QJsonObject object{
        {QStringLiteral("status"), status},
        {QStringLiteral("message"), message},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("turnId"), turnId},
        {QStringLiteral("hookEvent"), hookEvent},
    };
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);

    QLocalSocket socket;
    socket.connectToServer(ServerName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(1200))
    {
        return false;
    }

    if (socket.write(payload) != payload.size())
    {
        return false;
    }
    socket.flush();
    if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(1200))
    {
        return false;
    }

    if (!socket.waitForReadyRead(1200))
    {
        return false;
    }
    return socket.readAll() == QByteArrayLiteral("ok");
}
