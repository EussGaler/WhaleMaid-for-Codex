#pragma once

#include <QPixmap>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include <memory>

class QEnterEvent;
class QMouseEvent;

class Live2DScene;

class CharacterWidget final : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit CharacterWidget(QWidget* parent = nullptr);
    ~CharacterWidget() override;

    void setCharacterPixmap(const QPixmap& pixmap);
    void setDragLocked(bool locked);

Q_SIGNALS:
    void pointerEntered();
    void pointerLeft();
    void pointerMoved(const QPointF& localPosition);
    void primaryPressed(const QPoint& globalPosition);
    void primaryDragged(const QPoint& globalPosition);
    void primaryReleased();
    void contextMenuRequested(const QPoint& globalPosition);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPixmap pixmap_;
    std::unique_ptr<Live2DScene> live2D_;
    QTimer animationTimer_;
    bool primaryDown_ = false;
    bool dragLocked_ = false;
};
