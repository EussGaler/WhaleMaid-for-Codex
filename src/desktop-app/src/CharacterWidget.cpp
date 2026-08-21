#if defined(WHALE_HAS_LIVE2D)
#include <GL/glew.h>
#endif

#include "CharacterWidget.hpp"

#if defined(WHALE_HAS_LIVE2D)
#include "live2d/Live2DScene.hpp"
#endif

#include <QCoreApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace
{
int physicalPixels(const int logicalPixels, const qreal devicePixelRatio)
{
    return std::max(1, static_cast<int>(std::lround(logicalPixels * devicePixelRatio)));
}
}

CharacterWidget::CharacterWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    animationTimer_.setTimerType(Qt::PreciseTimer);
    animationTimer_.setInterval(16);
    connect(&animationTimer_, &QTimer::timeout, this, [this]() { update(); });
    animationTimer_.start();
}

CharacterWidget::~CharacterWidget()
{
#if defined(WHALE_HAS_LIVE2D)
    if (context())
    {
        makeCurrent();
        live2D_.reset();
        doneCurrent();
    }
#endif
}

void CharacterWidget::setCharacterPixmap(const QPixmap& pixmap)
{
    pixmap_ = pixmap;
    update();
}

void CharacterWidget::setDragLocked(const bool locked)
{
    dragLocked_ = locked;
    primaryDown_ = false;
    setCursor(dragLocked_ ? Qt::ArrowCursor : Qt::OpenHandCursor);
}

void CharacterWidget::initializeGL()
{
#if defined(WHALE_HAS_LIVE2D)
    glewExperimental = GL_TRUE;
    if (glewInit() == GLEW_OK)
    {
        // GLEW may leave GL_INVALID_ENUM behind on a compatibility context.
        while (glGetError() != GL_NO_ERROR)
        {
        }

        live2D_ = std::make_unique<Live2DScene>();
        const qreal ratio = devicePixelRatioF();
        if (!live2D_->initialize(
                QCoreApplication::applicationDirPath()
                    + QStringLiteral("/Resources/WhaleMaid"),
                QStringLiteral("WhaleMaid-runtime-v1"),
                physicalPixels(width(), ratio),
                physicalPixels(height(), ratio)))
        {
            live2D_.reset();
        }
    }
#endif
}

void CharacterWidget::paintGL()
{
#if defined(WHALE_HAS_LIVE2D)
    if (live2D_)
    {
        const QPoint cursor = mapFromGlobal(QCursor::pos());
        const float horizontalRange = std::max(1.0F, static_cast<float>(width()) * 0.9F);
        const float verticalRange = std::max(1.0F, static_cast<float>(height()) * 0.75F);
        const float eyeCenterX = static_cast<float>(width()) * 0.5F;
        const float eyeCenterY = static_cast<float>(height()) * 0.28F;
        const float pointerX = std::clamp(
            (static_cast<float>(cursor.x()) - eyeCenterX) / horizontalRange,
            -1.0F,
            1.0F);
        const float pointerY = std::clamp(
            (eyeCenterY - static_cast<float>(cursor.y())) / verticalRange,
            -1.0F,
            1.0F);
        live2D_->setPointer(pointerX, pointerY);

        const qreal ratio = devicePixelRatioF();
        live2D_->render(
            physicalPixels(width(), ratio),
            physicalPixels(height(), ratio));
        return;
    }
#endif

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (pixmap_.isNull())
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QSize fitted = pixmap_.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target(
        (width() - fitted.width()) / 2,
        height() - fitted.height(),
        fitted.width(),
        fitted.height());
    painter.drawPixmap(target, pixmap_);
}

void CharacterWidget::resizeGL(const int width, const int height)
{
#if defined(WHALE_HAS_LIVE2D)
    if (live2D_)
    {
        const qreal ratio = devicePixelRatioF();
        live2D_->resize(
            physicalPixels(width, ratio),
            physicalPixels(height, ratio));
    }
#else
    Q_UNUSED(width)
    Q_UNUSED(height)
#endif
}

void CharacterWidget::enterEvent(QEnterEvent* event)
{
    Q_EMIT pointerEntered();
    QWidget::enterEvent(event);
}

void CharacterWidget::leaveEvent(QEvent* event)
{
    Q_EMIT pointerLeft();
    QWidget::leaveEvent(event);
}

void CharacterWidget::mouseMoveEvent(QMouseEvent* event)
{
    Q_EMIT pointerMoved(event->position());
    if (primaryDown_)
    {
        Q_EMIT primaryDragged(event->globalPosition().toPoint());
    }
    QWidget::mouseMoveEvent(event);
}

void CharacterWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (dragLocked_)
        {
            event->accept();
            return;
        }
        primaryDown_ = true;
        setCursor(Qt::ClosedHandCursor);
        Q_EMIT primaryPressed(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton)
    {
        Q_EMIT contextMenuRequested(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void CharacterWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && primaryDown_)
    {
        primaryDown_ = false;
        setCursor(dragLocked_ ? Qt::ArrowCursor : Qt::OpenHandCursor);
        Q_EMIT primaryReleased();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}
