#include "PetStateController.hpp"
#include "CodexActivityWatcher.hpp"
#include "PetPreferences.hpp"
#include "PlatformPaths.hpp"
#include "StartupSettings.hpp"
#include "WindowPlacement.hpp"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSettings>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class PetStateControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stateChangeIsPublishedOnce();
    void dragStartPreservesWindowPosition();
    void clampingKeepsPartOfPetVisible();
    void completionNoticeSurvivesLaterStateChanges();
    void failureNoticeIsPublished();
    void approvalLogsWithoutIdsAreNotDropped();
    void storeDesktopLogsAreDiscovered();
    void duplicateDesktopLogViewsEmitApprovalOnce();
    void desktopPermissionNotificationIsRecognizedOnce();
    void permissionToolCallsAreRecognized();
    void functionToolCallsAreRecognized();
    void guardianCompletionsAreIgnored();
    void longRunningSessionIsDiscovered();
    void startupSettingCanBeToggled();
    void petPreferencesRoundTrip();
};

void PetStateControllerTests::stateChangeIsPublishedOnce()
{
    PetStateController state;
    QSignalSpy changes(&state, &PetStateController::primaryStateChanged);
    state.setPrimaryState(PetStateController::PrimaryState::Working);
    state.setPrimaryState(PetStateController::PrimaryState::Working);

    QCOMPARE(state.primaryState(), PetStateController::PrimaryState::Working);
    QCOMPARE(changes.count(), 1);
}

void PetStateControllerTests::dragStartPreservesWindowPosition()
{
    const QPoint originalTopLeft(1200, 300);
    const QPoint clickInsideWindow(185, 242);
    const QPoint globalPointer = originalTopLeft + clickInsideWindow;

    QCOMPARE(
        WindowPlacement::requestedTopLeft(globalPointer, clickInsideWindow),
        originalTopLeft);
}

void PetStateControllerTests::clampingKeepsPartOfPetVisible()
{
    const QRect virtualArea(0, 0, 1920, 1080);
    const QSize windowSize(520, 720);
    constexpr int minimumVisible = 96;

    const QPoint clamped = WindowPlacement::clampToVisibleArea(
        QPoint(5000, 5000), windowSize, virtualArea, minimumVisible);

    QCOMPARE(clamped.x(), virtualArea.right() - minimumVisible + 1);
    QCOMPARE(clamped.y(), virtualArea.bottom() - minimumVisible + 1);
}

void PetStateControllerTests::completionNoticeSurvivesLaterStateChanges()
{
    PetStateController state;
    QSignalSpy notices(&state, &PetStateController::completionPosted);

    state.postCompletion(QStringLiteral("done"));
    state.setPrimaryState(PetStateController::PrimaryState::Working);

    QCOMPARE(notices.count(), 1);
    QCOMPARE(notices.at(0).at(0).toString(), QStringLiteral("done"));
    QCOMPARE(state.primaryState(), PetStateController::PrimaryState::Working);
}

void PetStateControllerTests::failureNoticeIsPublished()
{
    PetStateController state;
    QSignalSpy notices(&state, &PetStateController::failurePosted);
    state.postFailure(QStringLiteral("failed"));

    QCOMPARE(notices.count(), 1);
    QCOMPARE(notices.at(0).at(0).toString(), QStringLiteral("failed"));
}

void PetStateControllerTests::approvalLogsWithoutIdsAreNotDropped()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldLocalAppData = qgetenv("LOCALAPPDATA");
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    qputenv("LOCALAPPDATA", temporary.path().toUtf8());
    qputenv("CODEX_HOME", QDir(temporary.path()).filePath(QStringLiteral("codex-home")).toUtf8());

    const QDate date = QDate::currentDate();
    const QString logDirectory = QDir(temporary.path()).filePath(
        QStringLiteral("Codex/Logs/%1/%2/%3")
            .arg(date.year(), 4, 10, QLatin1Char('0'))
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0')));
    QVERIFY(QDir().mkpath(logDirectory));

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    QFile log(QDir(logDirectory).filePath(QStringLiteral("approval.log")));
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    const auto appendApproval = [&log](const int requestId) {
        const QString line = QStringLiteral(
            "%1 info [desktop-notifications] show approval "
            "conversationId=test-conversation kind=permissionRequest requestId=%2\n")
                                 .arg(
                                     QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
                                 .arg(requestId);
        QCOMPARE(log.write(line.toUtf8()), line.toUtf8().size());
        QVERIFY(log.flush());
    };

    appendApproval(1);
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("approval"));
    QCOMPARE(events.at(0).at(2).toString(), QStringLiteral("test-conversation"));
    QVERIFY(!events.at(0).at(3).toString().isEmpty());

    appendApproval(2);
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 2, 3000);
    QVERIFY(events.at(0).at(3).toString() != events.at(1).at(3).toString());

    if (oldLocalAppData.isNull())
    {
        qunsetenv("LOCALAPPDATA");
    }
    else
    {
        qputenv("LOCALAPPDATA", oldLocalAppData);
    }
    if (oldCodexHome.isNull())
    {
        qunsetenv("CODEX_HOME");
    }
    else
    {
        qputenv("CODEX_HOME", oldCodexHome);
    }
}

void PetStateControllerTests::storeDesktopLogsAreDiscovered()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldLocalAppData = qgetenv("LOCALAPPDATA");
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    qputenv("LOCALAPPDATA", temporary.path().toUtf8());
    qputenv("CODEX_HOME", QDir(temporary.path()).filePath(QStringLiteral("codex-home")).toUtf8());
    const auto cleanup = qScopeGuard([=]() {
        if (oldLocalAppData.isNull()) qunsetenv("LOCALAPPDATA");
        else qputenv("LOCALAPPDATA", oldLocalAppData);
        if (oldCodexHome.isNull()) qunsetenv("CODEX_HOME");
        else qputenv("CODEX_HOME", oldCodexHome);
    });

    const QDate date = QDate::currentDate();
    const QString logDirectory = QDir(temporary.path()).filePath(
        QStringLiteral("Packages/OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/Codex/Logs/%1/%2/%3")
            .arg(date.year(), 4, 10, QLatin1Char('0'))
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0')));
    QVERIFY(QDir().mkpath(logDirectory));
    QFile log(QDir(logDirectory).filePath(QStringLiteral("store-approval.log")));
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    const QString line = QStringLiteral(
        "%1 info [desktop-notifications] show approval conversationId=store-test "
        "kind=permissionRequest requestId=store-request\n")
                             .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    QCOMPARE(log.write(line.toUtf8()), line.toUtf8().size());
    QVERIFY(log.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("approval"));
    QCOMPARE(events.at(0).at(2).toString(), QStringLiteral("store-test"));
}

void PetStateControllerTests::duplicateDesktopLogViewsEmitApprovalOnce()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldLocalAppData = qgetenv("LOCALAPPDATA");
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    qputenv("LOCALAPPDATA", temporary.path().toUtf8());
    qputenv("CODEX_HOME", QDir(temporary.path()).filePath(QStringLiteral("codex-home")).toUtf8());
    const auto cleanup = qScopeGuard([=]() {
        if (oldLocalAppData.isNull()) qunsetenv("LOCALAPPDATA");
        else qputenv("LOCALAPPDATA", oldLocalAppData);
        if (oldCodexHome.isNull()) qunsetenv("CODEX_HOME");
        else qputenv("CODEX_HOME", oldCodexHome);
    });

    const QDate date = QDate::currentDate();
    const QString suffix = QStringLiteral("%1/%2/%3")
                               .arg(date.year(), 4, 10, QLatin1Char('0'))
                               .arg(date.month(), 2, 10, QLatin1Char('0'))
                               .arg(date.day(), 2, 10, QLatin1Char('0'));
    const QString legacyDirectory = QDir(temporary.path()).filePath(
        QStringLiteral("Codex/Logs/%1").arg(suffix));
    const QString storeDirectory = QDir(temporary.path()).filePath(
        QStringLiteral("Packages/OpenAI.Codex_another-family/LocalCache/Local/Codex/Logs/%1")
            .arg(suffix));
    QVERIFY(QDir().mkpath(legacyDirectory));
    QVERIFY(QDir().mkpath(storeDirectory));
    QFile legacyLog(QDir(legacyDirectory).filePath(QStringLiteral("approval.log")));
    QFile storeLog(QDir(storeDirectory).filePath(QStringLiteral("approval.log")));
    QVERIFY(legacyLog.open(QIODevice::WriteOnly | QIODevice::Append));
    QVERIFY(storeLog.open(QIODevice::WriteOnly | QIODevice::Append));

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    const QString line = QStringLiteral(
        "%1 info [desktop-notifications] show approval conversationId=duplicate-test "
        "kind=permissionRequest requestId=same-request\n")
                             .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    QCOMPARE(legacyLog.write(line.toUtf8()), line.toUtf8().size());
    QCOMPARE(storeLog.write(line.toUtf8()), line.toUtf8().size());
    QVERIFY(legacyLog.flush());
    QVERIFY(storeLog.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QTest::qWait(750);
    QCOMPARE(events.count(), 1);
}

void PetStateControllerTests::desktopPermissionNotificationIsRecognizedOnce()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldLocalAppData = qgetenv("LOCALAPPDATA");
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    qputenv("LOCALAPPDATA", temporary.path().toUtf8());
    qputenv("CODEX_HOME", QDir(temporary.path()).filePath(QStringLiteral("codex-home")).toUtf8());
    const auto cleanup = qScopeGuard([=]() {
        if (oldLocalAppData.isNull()) qunsetenv("LOCALAPPDATA");
        else qputenv("LOCALAPPDATA", oldLocalAppData);
        if (oldCodexHome.isNull()) qunsetenv("CODEX_HOME");
        else qputenv("CODEX_HOME", oldCodexHome);
    });

    const QDate date = QDate::currentDate();
    const QString logDirectory = QDir(temporary.path()).filePath(
        QStringLiteral("Packages/OpenAI.Codex_2p2nqsd0c76g0/LocalCache/Local/Codex/Logs/%1/%2/%3")
            .arg(date.year(), 4, 10, QLatin1Char('0'))
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0')));
    QVERIFY(QDir().mkpath(logDirectory));
    QFile log(QDir(logDirectory).filePath(QStringLiteral("notification.log")));
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QString approvalLine = QStringLiteral(
        "%1 info [electron-message-handler] [desktop-notifications] show approval "
        "conversationId=notification-test kind=commandExecution requestId=17\n")
                                     .arg(timestamp);
    const QString forwardedLine = QStringLiteral(
        "%1 info [notifications-service] [desktop-notifications] forward show "
        "kind=permission notificationId=approval-local-17\n")
                                      .arg(timestamp);
    const QString notificationLine = QStringLiteral(
        "%1 info [desktop-notifications] show notification actionCount=0 "
        "kind=permission notificationId=approval-local-17\n")
                                         .arg(timestamp);
    QCOMPARE(log.write(approvalLine.toUtf8()), approvalLine.toUtf8().size());
    QCOMPARE(log.write(forwardedLine.toUtf8()), forwardedLine.toUtf8().size());
    QCOMPARE(log.write(notificationLine.toUtf8()), notificationLine.toUtf8().size());
    QVERIFY(log.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QTest::qWait(750);
    QCOMPARE(events.count(), 1);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("approval"));
}

void PetStateControllerTests::permissionToolCallsAreRecognized()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldLocalAppData = qgetenv("LOCALAPPDATA");
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    const QString codexHome = QDir(temporary.path()).filePath(QStringLiteral("codex-home"));
    qputenv("LOCALAPPDATA", temporary.path().toUtf8());
    qputenv("CODEX_HOME", codexHome.toUtf8());

    const auto cleanup = qScopeGuard([=]() {
        if (oldLocalAppData.isNull()) qunsetenv("LOCALAPPDATA");
        else qputenv("LOCALAPPDATA", oldLocalAppData);
        if (oldCodexHome.isNull()) qunsetenv("CODEX_HOME");
        else qputenv("CODEX_HOME", oldCodexHome);
    });

    const QDate date = QDate::currentDate();
    const QString sessionDirectory = QDir(codexHome).filePath(
        QStringLiteral("sessions/%1/%2/%3")
            .arg(date.year(), 4, 10, QLatin1Char('0'))
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0')));
    QVERIFY(QDir().mkpath(sessionDirectory));

    QFile session(QDir(sessionDirectory).filePath(
        QStringLiteral("rollout-test-01permission-session.jsonl")));
    QVERIFY(session.open(QIODevice::WriteOnly | QIODevice::Append));
    const QByteArray metadata =
        "{\"type\":\"session_meta\",\"payload\":{\"type\":\"session_meta\"," 
        "\"source\":\"vscode\"}}\n";
    QCOMPARE(session.write(metadata), metadata.size());
    QVERIFY(session.flush());

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    const QByteArray normalExec =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"exec\"," 
        "\"input\":\"const r = await tools.exec_command({});\"," 
        "\"internal_chat_message_metadata_passthrough\":{"
        "\"turn_id\":\"turn-permission-test\"}}}\n";
    QCOMPARE(session.write(normalExec), normalExec.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("working"));
    QCOMPARE(events.at(0).at(3).toString(), QStringLiteral("turn-permission-test"));

    const QByteArray toolOutput =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call_output\"}}\n";
    QCOMPARE(session.write(toolOutput), toolOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 2, 3000);
    QCOMPARE(events.at(1).at(0).toString(), QStringLiteral("thinking"));

    const QByteArray wrappedPermission =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"exec\"," 
        "\"input\":\"const r = await tools.request_permissions({});\"}}\n";
    QCOMPARE(session.write(wrappedPermission), wrappedPermission.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 3, 3000);
    QCOMPARE(events.at(2).at(0).toString(), QStringLiteral("approval"));

    QCOMPARE(session.write(toolOutput), toolOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 4, 3000);

    const QByteArray directPermission =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"request_permissions\"," 
        "\"input\":\"{}\"}}\n";
    QCOMPARE(session.write(directPermission), directPermission.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 5, 3000);
    QCOMPARE(events.at(4).at(0).toString(), QStringLiteral("approval"));

    QCOMPARE(session.write(toolOutput), toolOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 6, 3000);

    const QByteArray escalatedExec =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"exec\","
        "\"input\":\"const r = await tools.exec_command({"
        "sandbox_permissions: \\\"require_escalated\\\","
        "justification: \\\"approval test one\\\"});\"}}\n";
    QCOMPARE(session.write(escalatedExec), escalatedExec.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 7, 3000);
    QCOMPARE(events.at(6).at(0).toString(), QStringLiteral("approval"));

    QCOMPARE(session.write(toolOutput), toolOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 8, 3000);

    const QByteArray escalatedExecSingleQuotes =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"exec_command\","
        "\"input\":\"sandbox_permissions   :   'require_escalated'\"}}\n";
    QCOMPARE(
        session.write(escalatedExecSingleQuotes),
        escalatedExecSingleQuotes.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 9, 3000);
    QCOMPARE(events.at(8).at(0).toString(), QStringLiteral("approval"));

    QCOMPARE(session.write(toolOutput), toolOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 10, 3000);

    const QByteArray escalatedExecQuotedKey =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"exec\","
        "\"input\":\"{\\\"sandbox_permissions\\\":"
        "\\\"require_escalated\\\"}\"}}\n";
    QCOMPARE(session.write(escalatedExecQuotedKey), escalatedExecQuotedKey.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 11, 3000);
    QCOMPARE(events.at(10).at(0).toString(), QStringLiteral("approval"));

    QCOMPARE(session.write(toolOutput), toolOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 12, 3000);

    const QByteArray defaultSandboxExec =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"exec\","
        "\"input\":\"sandbox_permissions: \\\"use_default\\\"\"}}\n";
    QCOMPARE(session.write(defaultSandboxExec), defaultSandboxExec.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 13, 3000);
    QCOMPARE(events.at(12).at(0).toString(), QStringLiteral("working"));

    QCOMPARE(session.write(toolOutput), toolOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 14, 3000);

    const QByteArray normalExecCommand =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"custom_tool_call\",\"name\":\"exec_command\","
        "\"input\":\"{\\\"cmd\\\":\\\"echo ok\\\"}\"}}\n";
    QCOMPARE(session.write(normalExecCommand), normalExecCommand.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 15, 3000);
    QCOMPARE(events.at(14).at(0).toString(), QStringLiteral("working"));
}

void PetStateControllerTests::functionToolCallsAreRecognized()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldLocalAppData = qgetenv("LOCALAPPDATA");
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    const QString codexHome = QDir(temporary.path()).filePath(QStringLiteral("codex-home"));
    qputenv("LOCALAPPDATA", temporary.path().toUtf8());
    qputenv("CODEX_HOME", codexHome.toUtf8());
    const auto cleanup = qScopeGuard([=]() {
        if (oldLocalAppData.isNull()) qunsetenv("LOCALAPPDATA");
        else qputenv("LOCALAPPDATA", oldLocalAppData);
        if (oldCodexHome.isNull()) qunsetenv("CODEX_HOME");
        else qputenv("CODEX_HOME", oldCodexHome);
    });

    const QDate date = QDate::currentDate();
    const QString sessionDirectory = QDir(codexHome).filePath(
        QStringLiteral("sessions/%1/%2/%3")
            .arg(date.year(), 4, 10, QLatin1Char('0'))
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0')));
    QVERIFY(QDir().mkpath(sessionDirectory));
    QFile session(QDir(sessionDirectory).filePath(
        QStringLiteral("rollout-2026-08-24T09-00-00-01a03303-1923-71d0-8f26-96d5aad7c3b0.jsonl")));
    QVERIFY(session.open(QIODevice::WriteOnly | QIODevice::Append));
    const QByteArray metadata =
        "{\"type\":\"session_meta\",\"payload\":{\"type\":\"session_meta\","
        "\"source\":\"vscode\"}}\n";
    QCOMPARE(session.write(metadata), metadata.size());
    QVERIFY(session.flush());

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    const QByteArray normalFunctionCall =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"function_call\",\"name\":\"exec_command\","
        "\"arguments\":\"{\\\"cmd\\\":\\\"dir\\\"}\","
        "\"call_id\":\"call-normal\"}}\n";
    QCOMPARE(session.write(normalFunctionCall), normalFunctionCall.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("working"));

    const QByteArray functionOutput =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"function_call_output\",\"call_id\":\"call-normal\","
        "\"output\":\"done\"}}\n";
    QCOMPARE(session.write(functionOutput), functionOutput.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 2, 3000);
    QCOMPARE(events.at(1).at(0).toString(), QStringLiteral("thinking"));

    const QByteArray escalatedFunctionCall =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"function_call\",\"name\":\"exec_command\","
        "\"arguments\":\"{\\\"cmd\\\":\\\"dir\\\","
        "\\\"sandbox_permissions\\\":\\\"require_escalated\\\","
        "\\\"justification\\\":\\\"approval test\\\"}\","
        "\"call_id\":\"call-approval\"}}\n";
    QCOMPARE(session.write(escalatedFunctionCall), escalatedFunctionCall.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 3, 3000);
    QCOMPARE(events.at(2).at(0).toString(), QStringLiteral("approval"));

    const QByteArray objectArgumentsFunctionCall =
        "{\"type\":\"response_item\",\"payload\":{"
        "\"type\":\"function_call\",\"name\":\"exec_command\","
        "\"arguments\":{\"cmd\":\"dir\","
        "\"sandbox_permissions\":\"require_escalated\"},"
        "\"call_id\":\"call-object-approval\"}}\n";
    QCOMPARE(session.write(functionOutput), functionOutput.size());
    QCOMPARE(session.write(objectArgumentsFunctionCall), objectArgumentsFunctionCall.size());
    QVERIFY(session.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 5, 3000);
    QCOMPARE(events.at(4).at(0).toString(), QStringLiteral("approval"));
}

void PetStateControllerTests::guardianCompletionsAreIgnored()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldLocalAppData = qgetenv("LOCALAPPDATA");
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    const QString codexHome = QDir(temporary.path()).filePath(QStringLiteral("codex-home"));
    qputenv("LOCALAPPDATA", temporary.path().toUtf8());
    qputenv("CODEX_HOME", codexHome.toUtf8());

    const auto cleanup = qScopeGuard([=]() {
        if (oldLocalAppData.isNull()) qunsetenv("LOCALAPPDATA");
        else qputenv("LOCALAPPDATA", oldLocalAppData);
        if (oldCodexHome.isNull()) qunsetenv("CODEX_HOME");
        else qputenv("CODEX_HOME", oldCodexHome);
    });

    const QDate date = QDate::currentDate();
    const QString sessionDirectory = QDir(codexHome).filePath(
        QStringLiteral("sessions/%1/%2/%3")
            .arg(date.year(), 4, 10, QLatin1Char('0'))
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0')));
    QVERIFY(QDir().mkpath(sessionDirectory));

    QFile guardian(QDir(sessionDirectory).filePath(
        QStringLiteral("rollout-test-01guardian-session.jsonl")));
    QFile primary(QDir(sessionDirectory).filePath(
        QStringLiteral("rollout-test-01primary-session.jsonl")));
    QVERIFY(guardian.open(QIODevice::WriteOnly | QIODevice::Append));
    QVERIFY(primary.open(QIODevice::WriteOnly | QIODevice::Append));

    const QByteArray guardianMeta =
        "{\"type\":\"session_meta\",\"payload\":{\"type\":\"session_meta\","
        "\"source\":{\"subagent\":{\"other\":\"guardian\"}}}}\n";
    const QByteArray primaryMeta =
        "{\"type\":\"session_meta\",\"payload\":{\"type\":\"session_meta\","
        "\"source\":\"vscode\"}}\n";
    QCOMPARE(guardian.write(guardianMeta), guardianMeta.size());
    QCOMPARE(primary.write(primaryMeta), primaryMeta.size());
    QVERIFY(guardian.flush());
    QVERIFY(primary.flush());

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    const QByteArray guardianComplete =
        "{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\","
        "\"turn_id\":\"guardian-turn\"}}\n";
    QCOMPARE(guardian.write(guardianComplete), guardianComplete.size());
    QVERIFY(guardian.flush());
    QTest::qWait(800);
    QCOMPARE(events.count(), 0);

    const QByteArray primaryComplete =
        "{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_complete\","
        "\"turn_id\":\"primary-turn\"}}\n";
    QCOMPARE(primary.write(primaryComplete), primaryComplete.size());
    QVERIFY(primary.flush());
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("completed"));
}

void PetStateControllerTests::longRunningSessionIsDiscovered()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldCodexHome = qgetenv("CODEX_HOME");
    const QString codexHome = QDir(temporary.path()).filePath(QStringLiteral("codex-home"));
    qputenv("CODEX_HOME", codexHome.toUtf8());
    const auto cleanup = qScopeGuard([=]() {
        if (oldCodexHome.isNull()) qunsetenv("CODEX_HOME");
        else qputenv("CODEX_HOME", oldCodexHome);
    });

    const QDate sessionDate = QDate::currentDate().addDays(-3);
    const QString sessionDirectory = QDir(codexHome).filePath(
        QStringLiteral("sessions/%1/%2/%3")
            .arg(sessionDate.year(), 4, 10, QLatin1Char('0'))
            .arg(sessionDate.month(), 2, 10, QLatin1Char('0'))
            .arg(sessionDate.day(), 2, 10, QLatin1Char('0')));
    QVERIFY(QDir().mkpath(sessionDirectory));

    QFile session(QDir(sessionDirectory).filePath(
        QStringLiteral("rollout-long-running-01long-session.jsonl")));
    QVERIFY(session.open(QIODevice::WriteOnly | QIODevice::Append));
    const QByteArray metadata =
        "{\"type\":\"session_meta\",\"payload\":{\"type\":\"session_meta\"}}\n";
    const QByteArray taskStarted =
        "{\"type\":\"event_msg\",\"payload\":{\"type\":\"task_started\","
        "\"turn_id\":\"long-turn\"}}\n";
    QCOMPARE(session.write(metadata), metadata.size());
    QCOMPARE(session.write(taskStarted), taskStarted.size());
    QVERIFY(session.flush());

    CodexActivityWatcher watcher;
    QSignalSpy events(&watcher, &CodexActivityWatcher::eventDetected);
    watcher.start();

    QCOMPARE(events.count(), 1);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("thinking"));
    QCOMPARE(events.at(0).at(2).toString(), QStringLiteral("01long-session"));
    QCOMPARE(events.at(0).at(3).toString(), QStringLiteral("long-turn"));
}

void PetStateControllerTests::startupSettingCanBeToggled()
{
#ifdef Q_OS_WIN
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldSettings = qgetenv("WHALEMAID_TEST_SETTINGS_REGISTRY");
    const QByteArray oldRun = qgetenv("WHALEMAID_TEST_RUN_REGISTRY");
    const QByteArray oldShortcut = qgetenv("WHALEMAID_TEST_LEGACY_SHORTCUT");
    const QString identifier = QUuid::createUuid().toString(QUuid::Id128);
    const QString settingsPath = QDir(temporary.path()).filePath(
        QStringLiteral("settings-%1.ini").arg(identifier));
    const QString runPath = QDir(temporary.path()).filePath(
        QStringLiteral("run-%1.ini").arg(identifier));
    const QString shortcutPath = QDir(temporary.path()).filePath(QStringLiteral("WhaleMaid.lnk"));
    qputenv("WHALEMAID_TEST_SETTINGS_REGISTRY", settingsPath.toUtf8());
    qputenv("WHALEMAID_TEST_RUN_REGISTRY", runPath.toUtf8());
    qputenv("WHALEMAID_TEST_LEGACY_SHORTCUT", shortcutPath.toUtf8());

    const auto cleanup = qScopeGuard([=]() {
        QSettings(settingsPath, QSettings::IniFormat).clear();
        QSettings(runPath, QSettings::IniFormat).clear();
        if (oldSettings.isNull()) qunsetenv("WHALEMAID_TEST_SETTINGS_REGISTRY");
        else qputenv("WHALEMAID_TEST_SETTINGS_REGISTRY", oldSettings);
        if (oldRun.isNull()) qunsetenv("WHALEMAID_TEST_RUN_REGISTRY");
        else qputenv("WHALEMAID_TEST_RUN_REGISTRY", oldRun);
        if (oldShortcut.isNull()) qunsetenv("WHALEMAID_TEST_LEGACY_SHORTCUT");
        else qputenv("WHALEMAID_TEST_LEGACY_SHORTCUT", oldShortcut);
    });

    QFile shortcut(shortcutPath);
    QVERIFY(shortcut.open(QIODevice::WriteOnly));
    shortcut.close();

    QString error;
    QVERIFY2(StartupSettings::setEnabled(true, &error), qPrintable(error));
    QVERIFY(StartupSettings::isEnabled());
    QVERIFY(!QFileInfo::exists(shortcutPath));
    QSettings runSettings(runPath, QSettings::IniFormat);
    QVERIFY(runSettings.value(QStringLiteral("WhaleMaid")).toString().contains(
        QStringLiteral("--manual-start")));

    QVERIFY2(StartupSettings::setEnabled(false, &error), qPrintable(error));
    QVERIFY(!StartupSettings::isEnabled());
    QSettings disabledRunSettings(runPath, QSettings::IniFormat);
    QVERIFY(!disabledRunSettings.contains(QStringLiteral("WhaleMaid")));
#else
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldAutostartPath = qgetenv("WHALEMAID_TEST_AUTOSTART_PATH");
    const QString autostartPath = QDir(temporary.path()).filePath(
        QStringLiteral("autostart/whalemaid.desktop"));
    qputenv("WHALEMAID_TEST_AUTOSTART_PATH", autostartPath.toUtf8());

    const auto cleanup = qScopeGuard([=]() {
        if (oldAutostartPath.isNull()) qunsetenv("WHALEMAID_TEST_AUTOSTART_PATH");
        else qputenv("WHALEMAID_TEST_AUTOSTART_PATH", oldAutostartPath);
    });

    QString error;
    QVERIFY2(StartupSettings::setEnabled(true, &error), qPrintable(error));
    QVERIFY(StartupSettings::isEnabled());
    QFile desktopFile(autostartPath);
    QVERIFY(desktopFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray contents = desktopFile.readAll();
    QVERIFY(contents.startsWith("[Desktop Entry]\n"));
    QVERIFY(contents.contains("Type=Application\n"));
    QVERIFY(contents.contains(" --manual-start\n"));
    QVERIFY(contents.contains("Terminal=false\n"));

    QVERIFY2(StartupSettings::setEnabled(false, &error), qPrintable(error));
    QVERIFY(!StartupSettings::isEnabled());
    QVERIFY(!QFileInfo::exists(autostartPath));
#endif
}

void PetStateControllerTests::petPreferencesRoundTrip()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray oldSettings = qgetenv("WHALEMAID_TEST_SETTINGS_REGISTRY");
    const QString settingsPath = QDir(temporary.path()).filePath(
        QStringLiteral("pet-preferences.ini"));
    qputenv("WHALEMAID_TEST_SETTINGS_REGISTRY", settingsPath.toUtf8());

    const auto cleanup = qScopeGuard([=]() {
        QSettings(settingsPath, QSettings::IniFormat).clear();
        if (oldSettings.isNull()) qunsetenv("WHALEMAID_TEST_SETTINGS_REGISTRY");
        else qputenv("WHALEMAID_TEST_SETTINGS_REGISTRY", oldSettings);
    });

    PetPreferencesData saved;
    saved.scalePercent = 15;
    saved.locked = true;
    saved.noticePlacement = QStringLiteral("left");
    saved.hasPosition = true;
    saved.topLeft = QPoint(1380, 420);
    saved.screenName = QStringLiteral("secondary-display");
    saved.hasScreenOffset = true;
    saved.screenOffset = QPoint(100, 120);

    QVERIFY(PetPreferences::save(saved));
    const PetPreferencesData loaded = PetPreferences::load();
    QCOMPARE(loaded.scalePercent, saved.scalePercent);
    QCOMPARE(loaded.locked, saved.locked);
    QCOMPARE(loaded.noticePlacement, saved.noticePlacement);
    QCOMPARE(loaded.hasPosition, saved.hasPosition);
    QCOMPARE(loaded.topLeft, saved.topLeft);
    QCOMPARE(loaded.screenName, saved.screenName);
    QCOMPARE(loaded.hasScreenOffset, saved.hasScreenOffset);
    QCOMPARE(loaded.screenOffset, saved.screenOffset);
}

QTEST_GUILESS_MAIN(PetStateControllerTests)

#include "PetStateControllerTests.moc"
