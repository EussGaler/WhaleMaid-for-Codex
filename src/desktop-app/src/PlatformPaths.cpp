#include "PlatformPaths.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

QString PlatformPaths::stateDirectory()
{
    const QString testDirectory = qEnvironmentVariable("WHALEMAID_TEST_STATE_DIRECTORY");
    if (!testDirectory.isEmpty())
    {
        return QDir::cleanPath(testDirectory);
    }

#ifdef Q_OS_WIN
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    return localAppData.isEmpty()
        ? QString()
        : QDir(localAppData).filePath(QStringLiteral("WhaleMaid"));
#else
    QString stateRoot = qEnvironmentVariable("XDG_STATE_HOME");
    if (stateRoot.isEmpty())
    {
        const QString home = QDir::homePath();
        if (home.isEmpty())
        {
            return QString();
        }
        stateRoot = QDir(home).filePath(QStringLiteral(".local/state"));
    }
    return QDir(stateRoot).filePath(QStringLiteral("WhaleMaid"));
#endif
}

QString PlatformPaths::manualExitFlagPath()
{
    const QString directory = stateDirectory();
    return directory.isEmpty()
        ? QString()
        : QDir(directory).filePath(QStringLiteral("manual-exit.flag"));
}

QString PlatformPaths::instanceLockPath()
{
    const QString testPath = qEnvironmentVariable("WHALEMAID_TEST_INSTANCE_LOCK");
    if (!testPath.isEmpty())
    {
        const QString path = QDir::cleanPath(testPath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        return path;
    }

    QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDirectory.isEmpty())
    {
#ifdef Q_OS_UNIX
        runtimeDirectory = QDir(QDir::tempPath()).filePath(
            QStringLiteral("whalemaid-%1").arg(static_cast<qulonglong>(getuid())));
#else
        runtimeDirectory = QDir(QDir::tempPath()).filePath(QStringLiteral("WhaleMaid"));
#endif
    }
    QDir().mkpath(runtimeDirectory);
    return QDir(runtimeDirectory).filePath(QStringLiteral("whalemaid-pet.lock"));
}
