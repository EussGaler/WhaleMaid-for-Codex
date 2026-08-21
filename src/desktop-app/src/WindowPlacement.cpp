#include "WindowPlacement.hpp"

#include <algorithm>

QPoint WindowPlacement::requestedTopLeft(
    const QPoint& globalPointer,
    const QPoint& pointerToWindowOffset)
{
    return globalPointer - pointerToWindowOffset;
}

QPoint WindowPlacement::clampToVisibleArea(
    const QPoint& requested,
    const QSize& windowSize,
    const QRect& virtualArea,
    const int minimumVisiblePixels)
{
    if (!virtualArea.isValid() || windowSize.isEmpty())
    {
        return requested;
    }

    const int visibleX = std::clamp(minimumVisiblePixels, 1, windowSize.width());
    const int visibleY = std::clamp(minimumVisiblePixels, 1, windowSize.height());
    const int minimumX = virtualArea.left() - windowSize.width() + visibleX;
    const int maximumX = virtualArea.right() - visibleX + 1;
    const int minimumY = virtualArea.top() - windowSize.height() + visibleY;
    const int maximumY = virtualArea.bottom() - visibleY + 1;

    return {
        std::clamp(requested.x(), minimumX, maximumX),
        std::clamp(requested.y(), minimumY, maximumY)
    };
}

