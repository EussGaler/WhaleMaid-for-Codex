#include "PetStateController.hpp"
#include "CodexActivityWatcher.hpp"
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
    void startupSettingCanBeToggled();
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
    const auto appendApproval = [&log]() {
        const QString line = QStringLiteral(
            "%1 info [electron-message-handler] Sending server response "
            "id=40 method=item/permissions/requestApproval rendererWebContentsId=1\n")
                                 .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        QCOMPARE(log.write(line.toUtf8()), line.toUtf8().size());
        QVERIFY(log.flush());
    };

    appendApproval();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("approval"));
    QVERIFY(!events.at(0).at(3).toString().isEmpty());

    appendApproval();
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
#endif
}

QTEST_GUILESS_MAIN(PetStateControllerTests)

#include "PetStateControllerTests.moc"
