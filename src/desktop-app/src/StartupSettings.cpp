#include "StartupSettings.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

namespace
{
#ifdef Q_OS_WIN
QString preferenceRegistryPath()
{
    const QString testPath = qEnvironmentVariable("WHALEMAID_TEST_SETTINGS_REGISTRY");
    return testPath.isEmpty()
        ? QStringLiteral("HKEY_CURRENT_USER\\Software\\WhaleMaid")
        : testPath;
}

QString runRegistryPath()
{
    const QString testPath = qEnvironmentVariable("WHALEMAID_TEST_RUN_REGISTRY");
    return testPath.isEmpty()
        ? QStringLiteral(
              "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run")
        : testPath;
}

QSettings::Format preferenceFormat()
{
    return qEnvironmentVariableIsSet("WHALEMAID_TEST_SETTINGS_REGISTRY")
        ? QSettings::IniFormat
        : QSettings::NativeFormat;
}

QSettings::Format runFormat()
{
    return qEnvironmentVariableIsSet("WHALEMAID_TEST_RUN_REGISTRY")
        ? QSettings::IniFormat
        : QSettings::NativeFormat;
}

QString legacyShortcutPath()
{
    const QString testPath = qEnvironmentVariable("WHALEMAID_TEST_LEGACY_SHORTCUT");
    if (!testPath.isEmpty())
    {
        return testPath;
    }
    return QDir(qEnvironmentVariable("APPDATA")).filePath(
        QStringLiteral("Microsoft/Windows/Start Menu/Programs/Startup/WhaleMaid.lnk"));
}

QString startupCommand()
{
    return QStringLiteral("\"%1\" --manual-start").arg(
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}
#else
QString autostartFilePath()
{
    const QString testPath = qEnvironmentVariable("WHALEMAID_TEST_AUTOSTART_PATH");
    if (!testPath.isEmpty())
    {
        return QDir::cleanPath(testPath);
    }

    QString configRoot = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configRoot.isEmpty())
    {
        configRoot = QDir(QDir::homePath()).filePath(QStringLiteral(".config"));
    }
    return QDir(configRoot).filePath(QStringLiteral("autostart/whalemaid.desktop"));
}

QString desktopEntryQuoted(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    value.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    value.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    return QStringLiteral("\"%1\"").arg(value);
}

QByteArray autostartDesktopEntry()
{
    const QString executable = desktopEntryQuoted(QCoreApplication::applicationFilePath());
    return QStringLiteral(
               "[Desktop Entry]\n"
               "Type=Application\n"
               "Version=1.0\n"
               "Name=WhaleMaid\n"
               "Comment=WhaleMaid desktop pet\n"
               "Exec=%1 --manual-start\n"
               "Terminal=false\n"
               "X-GNOME-Autostart-enabled=true\n")
        .arg(executable)
        .toUtf8();
}
#endif
}

bool StartupSettings::isEnabled()
{
#ifdef Q_OS_WIN
    QSettings preferences(preferenceRegistryPath(), preferenceFormat());
    if (preferences.contains(QStringLiteral("StartWithWindows")))
    {
        return preferences.value(QStringLiteral("StartWithWindows")).toInt() != 0;
    }

    QSettings runSettings(runRegistryPath(), runFormat());
    return runSettings.contains(QStringLiteral("WhaleMaid"))
        || QFileInfo::exists(legacyShortcutPath());
#else
    return QFileInfo::exists(autostartFilePath());
#endif
}

bool StartupSettings::setEnabled(const bool enabled, QString* errorMessage)
{
#ifdef Q_OS_WIN
    QSettings preferences(preferenceRegistryPath(), preferenceFormat());
    preferences.setValue(QStringLiteral("StartWithWindows"), enabled ? 1 : 0);
    preferences.sync();

    QSettings runSettings(runRegistryPath(), runFormat());
    if (enabled)
    {
        runSettings.setValue(QStringLiteral("WhaleMaid"), startupCommand());
    }
    else
    {
        runSettings.remove(QStringLiteral("WhaleMaid"));
    }
    runSettings.sync();

    const QString shortcut = legacyShortcutPath();
    if (QFileInfo::exists(shortcut) && !QFile::remove(shortcut))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法移除旧版开机启动快捷方式：%1").arg(shortcut);
        }
        return false;
    }

    if (preferences.status() != QSettings::NoError
        || runSettings.status() != QSettings::NoError)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Windows 无法保存当前用户的开机启动设置。");
        }
        return false;
    }
    return true;
#else
    const QString path = autostartFilePath();
    if (!enabled)
    {
        if (!QFileInfo::exists(path) || QFile::remove(path))
        {
            return true;
        }
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法移除当前用户的开机启动文件：%1").arg(path);
        }
        return false;
    }

    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法创建当前用户的开机启动目录：%1").arg(directory);
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(autostartDesktopEntry()) < 0
        || !file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法保存当前用户的开机启动文件：%1").arg(path);
        }
        return false;
    }
    return true;
#endif
}
