#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

class CodexStatusBridge final : public QObject
{
    Q_OBJECT

public:
    explicit CodexStatusBridge(QObject* parent = nullptr);

    [[nodiscard]] bool start();
    [[nodiscard]] static bool sendEvent(
        const QString& status,
        const QString& message = {},
        const QString& sessionId = {},
        const QString& turnId = {},
        const QString& hookEvent = {});

Q_SIGNALS:
    void eventReceived(
        const QString& status,
        const QString& message,
        const QString& sessionId,
        const QString& turnId);

private:
    QLocalServer* server_ = nullptr;
};
