#include "CodexActivityWatcher.hpp"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

namespace
{
constexpr qint64 InitialTailBytes = 256 * 1024;
constexpr qint64 MaximumIncrementBytes = 2 * 1024 * 1024;
constexpr int DiscoveryEveryPolls = 8;
constexpr int MaximumDesktopLogsPerDay = 64;
constexpr qint64 NewlyDiscoveredLogTailBytes = 64 * 1024;
constexpr qint64 RecentApprovalWindowMs = 30 * 1000;
constexpr qsizetype MaximumRecentApprovalKeys = 256;

QStringList desktopLogRoots(const QString& localAppData)
{
    QStringList candidates;
    QDir packages(QDir(localAppData).filePath(QStringLiteral("Packages")));

    // Prefer the Store package's real LocalCache location. On some Windows
    // installations the shorter %LOCALAPPDATA%/Codex path is only a virtualized
    // view and is not accessible to an unpackaged WhaleMaid process.
    candidates.append(QDir(packages.absolutePath()).filePath(
        QStringLiteral("OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/Codex/Logs")));
    const QFileInfoList codexPackages = packages.entryInfoList(
        {QStringLiteral("OpenAI.Codex_*")},
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const QFileInfo& package : codexPackages)
    {
        candidates.append(QDir(package.absoluteFilePath()).filePath(
            QStringLiteral("LocalCache/Local/Codex/Logs")));
    }
    candidates.append(QDir(localAppData).filePath(QStringLiteral("Codex/Logs")));

    QStringList roots;
    QSet<QString> seen;
    for (const QString& candidate : candidates)
    {
        const QFileInfo information(candidate);
        if (!information.isDir())
        {
            continue;
        }
        const QString canonical = information.canonicalFilePath();
        const QString key = QDir::cleanPath(
            canonical.isEmpty() ? information.absoluteFilePath() : canonical)
                                .toLower();
        if (!seen.contains(key))
        {
            seen.insert(key);
            roots.append(information.absoluteFilePath());
        }
    }
    return roots;
}

QString sessionIdFromPath(const QString& path)
{
    static const QRegularExpression expression(
        QStringLiteral("-(01[0-9a-z-]+)\\.jsonl$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = expression.match(QFileInfo(path).fileName());
    return match.hasMatch() ? match.captured(1) : QFileInfo(path).baseName();
}

QString captureField(const QString& line, const QString& field)
{
    const QRegularExpression expression(
        QStringLiteral("(?:^|\\s)%1=([^\\s]+)").arg(QRegularExpression::escape(field)));
    const auto match = expression.match(line);
    return match.hasMatch() ? match.captured(1) : QString();
}

bool isPermissionRequest(const QString& toolName, const QString& toolInput)
{
    const bool directPermissionRequest =
        toolName.compare(
            QStringLiteral("request_permissions"),
            Qt::CaseInsensitive) == 0;
    const bool execTool =
        toolName.compare(QStringLiteral("exec"), Qt::CaseInsensitive) == 0
        || toolName.compare(
               QStringLiteral("exec_command"),
               Qt::CaseInsensitive) == 0;

    static const QRegularExpression wrappedPermissionExpression(
        QStringLiteral(
            R"(\bawait\s+tools\s*\.\s*request_permissions\s*\()"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression escalatedPermissionExpression(
        QStringLiteral(
            R"(["']?\bsandbox_permissions\b["']?\s*:\s*["']require_escalated["'])"),
        QRegularExpression::CaseInsensitiveOption);

    return directPermissionRequest
        || (execTool
            && (toolInput.contains(wrappedPermissionExpression)
                || toolInput.contains(escalatedPermissionExpression)));
}

QString toolArguments(const QJsonObject& payload, const QString& field)
{
    const QJsonValue value = payload.value(field);
    if (value.isString())
    {
        return value.toString();
    }
    if (value.isObject())
    {
        return QString::fromUtf8(
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray())
    {
        return QString::fromUtf8(
            QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return QString();
}
}

CodexActivityWatcher::CodexActivityWatcher(QObject* parent)
    : QObject(parent)
    , timer_(new QTimer(this))
{
    timer_->setInterval(250);
    timer_->setTimerType(Qt::CoarseTimer);
    connect(timer_, &QTimer::timeout, this, &CodexActivityWatcher::poll);
}

void CodexActivityWatcher::start()
{
    discoverFiles();
    initialDiscoveryComplete_ = true;
    timer_->start();
}

QString CodexActivityWatcher::codexHome() const
{
    const QString configured = qEnvironmentVariable("CODEX_HOME");
    return configured.isEmpty()
        ? QDir::home().filePath(QStringLiteral(".codex"))
        : QDir(configured).absolutePath();
}

void CodexActivityWatcher::poll()
{
    if (--discoveryCountdown_ <= 0)
    {
        discoverFiles();
        discoveryCountdown_ = DiscoveryEveryPolls;
    }

    const auto sessionPaths = sessionFiles_.keys();
    for (const QString& path : sessionPaths)
    {
        readSessionFile(path, false);
    }

    const auto logPaths = logFiles_.keys();
    for (const QString& path : logPaths)
    {
        readDesktopLog(path);
    }
}

void CodexActivityWatcher::discoverFiles()
{
    const QDate today = QDate::currentDate();
    for (int dayOffset = 0; dayOffset >= -1; --dayOffset)
    {
        const QDate date = today.addDays(dayOffset);
        const QString directoryPath = QDir(codexHome()).filePath(
            QStringLiteral("sessions/%1/%2/%3")
                .arg(date.year(), 4, 10, QLatin1Char('0'))
                .arg(date.month(), 2, 10, QLatin1Char('0'))
                .arg(date.day(), 2, 10, QLatin1Char('0')));
        QDir directory(directoryPath);
        const QFileInfoList files = directory.entryInfoList(
            {QStringLiteral("rollout-*.jsonl")},
            QDir::Files,
            QDir::Time);
        const int count = std::min<int>(static_cast<int>(files.size()), 12);
        for (int index = 0; index < count; ++index)
        {
            const QString path = files.at(index).absoluteFilePath();
            if (!sessionFiles_.contains(path))
            {
                SessionFileState state;
                state.sessionId = sessionIdFromPath(path);
                sessionFiles_.insert(path, state);
                readSessionFile(path, true);
            }
        }
    }

    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty())
    {
        const QStringList roots = desktopLogRoots(localAppData);
        for (const QString& root : roots)
        {
            for (int dayOffset = 0; dayOffset >= -1; --dayOffset)
            {
                const QDate date = today.addDays(dayOffset);
                QDir directory(QDir(root).filePath(
                    QStringLiteral("%1/%2/%3")
                        .arg(date.year(), 4, 10, QLatin1Char('0'))
                        .arg(date.month(), 2, 10, QLatin1Char('0'))
                        .arg(date.day(), 2, 10, QLatin1Char('0'))));
                const QFileInfoList logs = directory.entryInfoList(
                    {QStringLiteral("*.log")}, QDir::Files, QDir::Time);
                const int count = std::min<int>(
                    static_cast<int>(logs.size()), MaximumDesktopLogsPerDay);
                for (int index = 0; index < count; ++index)
                {
                    const QString path = logs.at(index).absoluteFilePath();
                    if (!logFiles_.contains(path))
                    {
                        LogFileState state;
                        const qint64 size = logs.at(index).size();
                        if (initialDiscoveryComplete_)
                        {
                            state.offset = std::max<qint64>(0, size - NewlyDiscoveredLogTailBytes);
                            state.discardPartialLine = state.offset > 0;
                            state.minimumEventUtcMs =
                                QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()
                                - RecentApprovalWindowMs;
                        }
                        else
                        {
                            // Do not resurrect old approval cards at application startup.
                            state.offset = size;
                        }
                        logFiles_.insert(path, state);
                        if (initialDiscoveryComplete_)
                        {
                            readDesktopLog(path);
                        }
                    }
                }
            }
        }
    }
}

void CodexActivityWatcher::readSessionFile(const QString& path, const bool initializeOnly)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }

    SessionFileState& state = sessionFiles_[path];
    const qint64 size = file.size();
    if (initializeOnly)
    {
        state.offset = std::max<qint64>(0, size - InitialTailBytes);
        file.seek(state.offset);
        if (state.offset > 0)
        {
            file.readLine();
        }
        while (!file.atEnd())
        {
            processSessionLine(state, file.readLine(), false);
        }
        state.offset = size;
        if (state.lastStatus == QStringLiteral("thinking")
            || state.lastStatus == QStringLiteral("working")
            || state.lastStatus == QStringLiteral("approval"))
        {
            Q_EMIT eventDetected(
                state.lastStatus,
                state.lastStatus == QStringLiteral("working")
                    ? QStringLiteral("正在处理任务")
                    : QStringLiteral("正在理解你的任务"),
                state.sessionId,
                state.turnId);
        }
        return;
    }

    if (size < state.offset)
    {
        state.offset = 0;
    }
    if (size == state.offset)
    {
        return;
    }
    bool discardPartialLine = false;
    if (size - state.offset > MaximumIncrementBytes)
    {
        state.offset = std::max<qint64>(0, size - InitialTailBytes);
        discardPartialLine = state.offset > 0;
    }
    file.seek(state.offset);
    if (discardPartialLine)
    {
        file.readLine();
    }
    while (!file.atEnd())
    {
        const qint64 lineStart = file.pos();
        const QByteArray line = file.readLine();
        if (!line.endsWith('\n'))
        {
            file.seek(lineStart);
            break;
        }
        processSessionLine(state, line, true);
    }
    state.offset = file.pos();
}

void CodexActivityWatcher::processSessionLine(
    SessionFileState& state,
    const QByteArray& line,
    const bool emitEvents)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return;
    }
    const QJsonObject root = document.object();
    const QString recordType = root.value(QStringLiteral("type")).toString();
    const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
    const QString payloadType = payload.value(QStringLiteral("type")).toString();

    if (recordType == QStringLiteral("session_meta"))
    {
        const QJsonValue source = payload.value(QStringLiteral("source"));
        state.ignored = (source.isObject()
                         && source.toObject().contains(QStringLiteral("subagent")))
            || (source.isString()
                && source.toString().contains(
                    QStringLiteral("subagent"), Qt::CaseInsensitive));
        return;
    }
    if (state.ignored)
    {
        return;
    }

    QString eventTurn = payload.value(QStringLiteral("turn_id")).toString();
    if (eventTurn.isEmpty())
    {
        eventTurn = payload
                        .value(QStringLiteral("internal_chat_message_metadata_passthrough"))
                        .toObject()
                        .value(QStringLiteral("turn_id"))
                        .toString();
    }
    if (!eventTurn.isEmpty())
    {
        state.turnId = eventTurn;
    }

    if (recordType == QStringLiteral("event_msg"))
    {
        if (payloadType == QStringLiteral("task_started")
            || payloadType == QStringLiteral("user_message")
            || payloadType == QStringLiteral("agent_message"))
        {
            publish(state, QStringLiteral("thinking"),
                    QStringLiteral("正在理解你的任务"), state.turnId, emitEvents);
        }
        else if (payloadType == QStringLiteral("web_search_begin")
                 || payloadType == QStringLiteral("web_search_end"))
        {
            publish(state, QStringLiteral("working"),
                    QStringLiteral("正在处理任务"), state.turnId, emitEvents);
        }
        else if (payloadType == QStringLiteral("task_complete"))
        {
            publish(state, QStringLiteral("completed"),
                    QStringLiteral("本轮任务已完成"), state.turnId, emitEvents);
        }
        else if (payloadType == QStringLiteral("turn_aborted"))
        {
            publish(state, QStringLiteral("idle"), {}, state.turnId, emitEvents);
        }
    }
    else if (recordType == QStringLiteral("response_item"))
    {
        const bool customToolCall =
            payloadType == QStringLiteral("custom_tool_call");
        const bool functionCall =
            payloadType == QStringLiteral("function_call");
        if (customToolCall || functionCall)
        {
            const QString toolName = payload.value(QStringLiteral("name")).toString();
            const QString toolInput = toolArguments(
                payload,
                customToolCall
                    ? QStringLiteral("input")
                    : QStringLiteral("arguments"));
            if (isPermissionRequest(toolName, toolInput))
            {
                publish(state, QStringLiteral("approval"),
                        QStringLiteral("需要你的确认"), state.turnId, emitEvents);
            }
            else
            {
                publish(state, QStringLiteral("working"),
                        QStringLiteral("正在处理任务"), state.turnId, emitEvents);
            }
        }
        else if (payloadType == QStringLiteral("reasoning")
                 || payloadType == QStringLiteral("custom_tool_call_output")
                 || payloadType == QStringLiteral("function_call_output"))
        {
            publish(state, QStringLiteral("thinking"),
                    QStringLiteral("正在理解你的任务"), state.turnId, emitEvents);
        }
    }
}

void CodexActivityWatcher::publish(
    SessionFileState& state,
    const QString& status,
    const QString& message,
    const QString& turnId,
    const bool emitEvents)
{
    if (state.lastStatus == status && state.lastStatusTurnId == turnId)
    {
        return;
    }
    state.lastStatus = status;
    state.lastStatusTurnId = turnId;
    if (emitEvents)
    {
        Q_EMIT eventDetected(status, message, state.sessionId, turnId);
    }
}

void CodexActivityWatcher::readDesktopLog(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    LogFileState& state = logFiles_[path];
    if (file.size() < state.offset)
    {
        state.offset = 0;
        state.minimumEventUtcMs =
            QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()
            - RecentApprovalWindowMs;
    }
    file.seek(state.offset);
    if (state.discardPartialLine)
    {
        file.readLine();
        state.discardPartialLine = false;
    }
    while (!file.atEnd())
    {
        const qint64 lineStart = file.pos();
        const QByteArray rawLine = file.readLine();
        if (!rawLine.endsWith('\n'))
        {
            file.seek(lineStart);
            break;
        }
        const QString line = QString::fromUtf8(rawLine);
        const bool desktopApprovalStarted =
            line.contains(QStringLiteral("[desktop-notifications] show approval"),
                          Qt::CaseInsensitive);
        const bool desktopPermissionNotification =
            (line.contains(QStringLiteral("[desktop-notifications] show notification"),
                           Qt::CaseInsensitive)
             || line.contains(QStringLiteral("[desktop-notifications] forward show"),
                              Qt::CaseInsensitive))
            && line.contains(QStringLiteral("kind=permission"), Qt::CaseInsensitive);
        const bool legacyApprovalStarted =
            line.contains(QStringLiteral("permissions/requestApproval"), Qt::CaseInsensitive)
            && !line.contains(QStringLiteral("Sending server response"), Qt::CaseInsensitive);
        if (!desktopApprovalStarted
            && !desktopPermissionNotification
            && !legacyApprovalStarted)
        {
            continue;
        }
        if (state.minimumEventUtcMs > 0)
        {
            const qsizetype timestampEnd = line.indexOf(QLatin1Char(' '));
            const QDateTime timestamp = QDateTime::fromString(
                line.left(timestampEnd), Qt::ISODateWithMs);
            if (timestamp.isValid()
                && timestamp.toMSecsSinceEpoch() < state.minimumEventUtcMs)
            {
                continue;
            }
        }
        const QString sessionId = captureField(line, QStringLiteral("conversationId"));
        const QString turnId = captureField(line, QStringLiteral("turnId"));
        QString requestId = captureField(line, QStringLiteral("requestId"));
        if (requestId.isEmpty())
        {
            requestId = captureField(line, QStringLiteral("notificationId"));
            requestId.remove(QRegularExpression(
                QStringLiteral("^approval-(?:local-)?"),
                QRegularExpression::CaseInsensitiveOption));
        }
        if (requestId.isEmpty() && legacyApprovalStarted)
        {
            requestId = captureField(line, QStringLiteral("id"));
        }
        const QString key = !requestId.isEmpty()
            ? QFileInfo(path).fileName() + QStringLiteral(":approval:") + requestId
            : path + QLatin1Char(':') + QString::number(lineStart);
        if (!recentApprovalKeys_.contains(key))
        {
            recentApprovalKeys_.insert(key);
            recentApprovalKeyOrder_.append(key);
            if (recentApprovalKeyOrder_.size() > MaximumRecentApprovalKeys)
            {
                recentApprovalKeys_.remove(recentApprovalKeyOrder_.takeFirst());
            }
            Q_EMIT eventDetected(
                QStringLiteral("approval"),
                QStringLiteral("需要你的确认"),
                sessionId.isEmpty() ? QStringLiteral("codex-log") : sessionId,
                turnId.isEmpty() ? key : turnId);
        }
    }
    state.offset = file.pos();
    state.minimumEventUtcMs = 0;
}
