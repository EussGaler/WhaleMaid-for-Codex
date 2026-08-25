#pragma once

#include <QString>

namespace PlatformPaths
{
[[nodiscard]] QString stateDirectory();
[[nodiscard]] QString manualExitFlagPath();
[[nodiscard]] QString instanceLockPath();
}
