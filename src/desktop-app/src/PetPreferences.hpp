#pragma once

#include <QPoint>
#include <QString>

struct PetPreferencesData
{
    int scalePercent = 75;
    bool locked = false;
    bool hasPosition = false;
    QPoint topLeft;
    QString screenName;
    bool hasScreenOffset = false;
    QPoint screenOffset;
};

namespace PetPreferences
{
[[nodiscard]] PetPreferencesData load();
bool save(const PetPreferencesData& preferences);
}
