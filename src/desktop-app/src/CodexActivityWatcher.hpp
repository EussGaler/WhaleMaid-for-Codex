#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QTimer;

class CodexActivityWatcher final : public QObject
{
    Q_OBJECT

public:
    explicit CodexActivityWatcher(QObject* parent = nullptr);
    void start();

Q_SIGNALS:
    void eventDetected(
        const QString& status,
        const QString& message,
        const QString& sessionId,
        const QString& turnId);

private:
    struct SessionFileState
    {
        qint64 offset = 0;
        QString sessionId;
        QString turnId;
        QString lastStatus;
        QString lastStatusTurnId;
        bool ignored = false;
    };

    struct LogFileState
    {
        qint64 offset = 0;
        qint64 minimumEventUtcMs = 0;
        bool discardPartialLine = false;
    };

    void poll();
    void discoverFiles();
    void readSessionFile(const QString& path, bool initializeOnly);
    void processSessionLine(SessionFileState& state, const QByteArray& line, bool emitEvents);
    void publish(
        SessionFileState& state,
        const QString& status,
        const QString& message,
        const QString& turnId,
        bool emitEvents);
    void readDesktopLog(const QString& path);
    [[nodiscard]] QString codexHome() const;

    QTimer* timer_ = nullptr;
    QHash<QString, SessionFileState> sessionFiles_;
    QHash<QString, LogFileState> logFiles_;
    QSet<QString> recentApprovalKeys_;
    QStringList recentApprovalKeyOrder_;
    int discoveryCountdown_ = 0;
    bool initialDiscoveryComplete_ = false;
};
