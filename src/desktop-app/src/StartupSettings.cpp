#include "StartupSettings.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace
{
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
    return false;
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
    Q_UNUSED(enabled);
    if (errorMessage)
    {
        *errorMessage = QStringLiteral("开机启动设置目前仅支持 Windows。");
    }
    return false;
#endif
}
