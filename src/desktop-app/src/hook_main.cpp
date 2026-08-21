#include "CodexStatusBridge.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QStringList>
#include <QThread>
#include <QTextStream>

#include <cstdio>

namespace
{
struct HookStatus
{
    QString status;
    QString message;
};

HookStatus mapHookEvent(const QJsonObject& input)
{
    const QString eventName = input.value(QStringLiteral("hook_event_name")).toString();
    if (eventName == QStringLiteral("SessionStart"))
    {
        return {QStringLiteral("idle"), {}};
    }
    if (eventName == QStringLiteral("UserPromptSubmit"))
    {
        return {QStringLiteral("thinking"), QStringLiteral("正在理解你的任务")};
    }
    if (eventName == QStringLiteral("PreToolUse")
        || eventName == QStringLiteral("PostToolUse")
        || eventName == QStringLiteral("SubagentStart")
        || eventName == QStringLiteral("SubagentStop"))
    {
        return {QStringLiteral("working"), QStringLiteral("正在处理任务")};
    }
    if (eventName == QStringLiteral("PermissionRequest"))
    {
        return {QStringLiteral("approval"), QStringLiteral("需要你的确认")};
    }
    if (eventName == QStringLiteral("Stop"))
    {
        const QString lastMessage = input.value(QStringLiteral("last_assistant_message"))
                                        .toString()
                                        .trimmed();
        if (lastMessage.isEmpty())
        {
            return {QStringLiteral("idle"), {}};
        }
        const QString lower = lastMessage.toLower();
        const QStringList failureMarkers = {
            QStringLiteral("任务失败"),
            QStringLiteral("未能完成"),
            QStringLiteral("无法完成"),
            QStringLiteral("执行失败"),
            QStringLiteral("failed to"),
            QStringLiteral("could not complete"),
            QStringLiteral("unable to complete"),
            QStringLiteral("task failed")};
        for (const QString& marker : failureMarkers)
        {
            if (lower.contains(marker))
            {
                return {QStringLiteral("failed"), QStringLiteral("任务未能完成，请返回 Codex 查看详情")};
            }
        }
        return {QStringLiteral("completed"), QStringLiteral("本轮任务已完成")};
    }
    if (eventName == QStringLiteral("SessionEnd"))
    {
        return {QStringLiteral("idle"), {}};
    }
    return {};
}

bool startPet()
{
    const QString executable = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("WhaleMaidPet.exe"));
    if (!QFile::exists(executable))
    {
        return false;
    }
    return QProcess::startDetached(
        executable,
        {QStringLiteral("--hook-autostart")},
        QCoreApplication::applicationDirPath());
}

QString manualExitFlagPath()
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    return localAppData.isEmpty()
        ? QString()
        : QDir(localAppData).filePath(QStringLiteral("WhaleMaid/manual-exit.flag"));
}

void writeDiagnostic(
    const QString& hookEvent,
    const HookStatus& mapped,
    const QString& sessionId,
    const bool delivered)
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty())
    {
        return;
    }
    const QString directory = QDir(localAppData).filePath(QStringLiteral("WhaleMaid/logs"));
    QDir().mkpath(directory);
    QFile file(QDir(directory).filePath(QStringLiteral("hook-events.log")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate)
           << " event=" << hookEvent
           << " status=" << mapped.status
           << " session=" << sessionId
           << " delivered=" << (delivered ? QStringLiteral("yes") : QStringLiteral("no"))
           << Qt::endl;
}

bool sendWithStartup(
    const HookStatus& mapped,
    const QString& sessionId,
    const QString& turnId,
    const QString& hookEvent)
{
    if (CodexStatusBridge::sendEvent(
            mapped.status, mapped.message, sessionId, turnId, hookEvent))
    {
        return true;
    }

    // A shutdown request must never resurrect a pet that is already closed.
    if (mapped.status == QStringLiteral("shutdown"))
    {
        return true;
    }

    const QString exitFlag = manualExitFlagPath();
    if (!exitFlag.isEmpty() && QFile::exists(exitFlag))
    {
        return true;
    }

    if (!startPet())
    {
        return false;
    }

    for (int attempt = 0; attempt < 40; ++attempt)
    {
        QThread::msleep(75);
        if (CodexStatusBridge::sendEvent(
                mapped.status, mapped.message, sessionId, turnId, hookEvent))
        {
            return true;
        }
    }
    return false;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("WhaleMaid Hook Bridge"));

    const QStringList arguments = app.arguments();
    const qsizetype statusIndex = arguments.indexOf(QStringLiteral("--status"));
    if (statusIndex >= 0 && statusIndex + 1 < arguments.size())
    {
        HookStatus direct{arguments.at(statusIndex + 1), {}};
        const qsizetype messageIndex = arguments.indexOf(QStringLiteral("--message"));
        if (messageIndex >= 0 && messageIndex + 1 < arguments.size())
        {
            direct.message = arguments.at(messageIndex + 1);
        }
        const bool delivered = sendWithStartup(
            direct, QStringLiteral("manual"), {}, QStringLiteral("manual"));
        writeDiagnostic(QStringLiteral("manual"), direct, QStringLiteral("manual"), delivered);
        return delivered ? 0 : 3;
    }

    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly))
    {
        return 0;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return 0;
    }

    const QJsonObject object = document.object();
    const HookStatus mapped = mapHookEvent(object);
    if (mapped.status.isEmpty())
    {
        return 0;
    }

    const bool delivered = sendWithStartup(
        mapped,
        object.value(QStringLiteral("session_id")).toString(),
        object.value(QStringLiteral("turn_id")).toString(),
        object.value(QStringLiteral("hook_event_name")).toString());
    writeDiagnostic(
        object.value(QStringLiteral("hook_event_name")).toString(),
        mapped,
        object.value(QStringLiteral("session_id")).toString(),
        delivered);
    return delivered ? 0 : 3;
}
