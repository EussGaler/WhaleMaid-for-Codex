#pragma once

#include <QString>

namespace StartupSettings
{
[[nodiscard]] bool isEnabled();
bool setEnabled(bool enabled, QString* errorMessage = nullptr);
}
