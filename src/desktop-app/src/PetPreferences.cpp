#include "PetPreferences.hpp"

#include <QSettings>

#include <memory>

namespace
{
constexpr int DefaultScalePercent = 75;
constexpr int MinimumScalePercent = 15;
constexpr int MaximumScalePercent = 200;

std::unique_ptr<QSettings> openSettings()
{
    const QString testPath = qEnvironmentVariable("WHALEMAID_TEST_SETTINGS_REGISTRY");
    if (!testPath.isEmpty())
    {
        return std::make_unique<QSettings>(testPath, QSettings::IniFormat);
    }

#ifdef Q_OS_WIN
    return std::make_unique<QSettings>(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\WhaleMaid"),
        QSettings::NativeFormat);
#else
    return std::make_unique<QSettings>(
        QSettings::NativeFormat,
        QSettings::UserScope,
        QStringLiteral("WhaleMaid"),
        QStringLiteral("WhaleMaid"));
#endif
}

int validatedScale(const int value)
{
    return value >= MinimumScalePercent && value <= MaximumScalePercent
        ? value
        : DefaultScalePercent;
}

QString validatedNoticePlacement(const QString& value)
{
    return value == QStringLiteral("left")
        ? QStringLiteral("left")
        : QStringLiteral("above");
}
}

PetPreferencesData PetPreferences::load()
{
    auto settings = openSettings();
    settings->beginGroup(QStringLiteral("Window"));

    PetPreferencesData result;
    result.scalePercent = validatedScale(
        settings->value(QStringLiteral("ScalePercent"), DefaultScalePercent).toInt());
    result.locked = settings->value(QStringLiteral("Locked"), false).toBool();
    result.noticePlacement = validatedNoticePlacement(settings->value(
        QStringLiteral("NoticePlacement"), QStringLiteral("above")).toString());
    result.hasPosition = settings->value(QStringLiteral("HasPosition"), false).toBool();
    result.topLeft = settings->value(QStringLiteral("TopLeft")).toPoint();
    result.screenName = settings->value(QStringLiteral("ScreenName")).toString();
    result.hasScreenOffset = settings->value(
        QStringLiteral("HasScreenOffset"), false).toBool();
    result.screenOffset = settings->value(QStringLiteral("ScreenOffset")).toPoint();

    settings->endGroup();
    return result;
}

bool PetPreferences::save(const PetPreferencesData& preferences)
{
    auto settings = openSettings();
    settings->beginGroup(QStringLiteral("Window"));
    settings->setValue(QStringLiteral("SchemaVersion"), 2);
    settings->setValue(
        QStringLiteral("ScalePercent"), validatedScale(preferences.scalePercent));
    settings->setValue(QStringLiteral("Locked"), preferences.locked);
    settings->setValue(
        QStringLiteral("NoticePlacement"),
        validatedNoticePlacement(preferences.noticePlacement));
    settings->setValue(QStringLiteral("HasPosition"), preferences.hasPosition);

    if (preferences.hasPosition)
    {
        settings->setValue(QStringLiteral("TopLeft"), preferences.topLeft);
        settings->setValue(QStringLiteral("ScreenName"), preferences.screenName);
        settings->setValue(
            QStringLiteral("HasScreenOffset"), preferences.hasScreenOffset);
        if (preferences.hasScreenOffset)
        {
            settings->setValue(QStringLiteral("ScreenOffset"), preferences.screenOffset);
        }
        else
        {
            settings->remove(QStringLiteral("ScreenOffset"));
        }
    }
    else
    {
        settings->remove(QStringLiteral("TopLeft"));
        settings->remove(QStringLiteral("ScreenName"));
        settings->remove(QStringLiteral("HasScreenOffset"));
        settings->remove(QStringLiteral("ScreenOffset"));
    }

    settings->endGroup();
    settings->sync();
    return settings->status() == QSettings::NoError;
}
