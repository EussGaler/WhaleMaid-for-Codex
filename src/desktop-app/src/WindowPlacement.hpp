#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

namespace WindowPlacement
{
[[nodiscard]] QPoint requestedTopLeft(
    const QPoint& globalPointer,
    const QPoint& pointerToWindowOffset);

[[nodiscard]] QPoint clampToVisibleArea(
    const QPoint& requested,
    const QSize& windowSize,
    const QRect& virtualArea,
    int minimumVisiblePixels);
}

