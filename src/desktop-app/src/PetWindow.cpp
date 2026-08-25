#include "PetWindow.hpp"

#include "CharacterWidget.hpp"
#include "CodexActivityWatcher.hpp"
#include "CodexStatusBridge.hpp"
#include "PetPreferences.hpp"
#include "PlatformPaths.hpp"
#include "StartupSettings.hpp"
#include "WindowPlacement.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFrame>
#include <QFile>
#include <QFontDatabase>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QImage>
#include <QMenu>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QToolButton>
#include <QTimer>
#include <QWidgetAction>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int BaseWindowWidth = 520;
constexpr int BaseWindowHeight = 720;
constexpr int DefaultWindowScalePercent = 75;
constexpr int MinimumVisiblePixels = 96;
constexpr int BaseNoticeBubbleWidth = 200;
constexpr int BaseNoticeCloseGutter = 24;
constexpr int BaseNoticeWidth = BaseNoticeBubbleWidth + BaseNoticeCloseGutter;
constexpr int BaseNoticeHeight = 98;
constexpr int MinimumNoticeScalePercent = 30;
constexpr int BaseOuterMargin = 12;
// The Live2D canvas contains transparent padding around the visible model.
// These offsets let the thought-bubble tail use that padding so the cards sit
// close to the character without covering the artwork.
constexpr int BaseNoticePetOverlap = 60;
constexpr int BaseNoticeSideOverlap = 130;
constexpr int BaseNoticeSideTopOffset = 55;

int scaledPixels(const int basePixels, const int percent, const int minimum = 1)
{
    return std::max(
        minimum,
        static_cast<int>(std::lround(
            static_cast<double>(basePixels) * percent / 100.0)));
}

QSize scaledWindowSize(const int percent)
{
    const double scale = static_cast<double>(percent) / 100.0;
    return {
        static_cast<int>(std::lround(BaseWindowWidth * scale)),
        static_cast<int>(std::lround(BaseWindowHeight * scale))};
}

QColor noticeFill(const QString& kind)
{
    if (kind == QStringLiteral("completed"))
    {
        return QColor(QStringLiteral("#F0FAF2"));
    }
    if (kind == QStringLiteral("failed"))
    {
        return QColor(QStringLiteral("#FFF1F2"));
    }
    if (kind == QStringLiteral("approval"))
    {
        return QColor(QStringLiteral("#FFF6E8"));
    }
    return QColor(Qt::white);
}

class ThoughtBubbleCard final : public QFrame
{
public:
    ThoughtBubbleCard(
        const QString& kind,
        const QString& title,
        const QString& fontFamily,
        const bool persistent,
        QWidget* parent)
        : QFrame(parent)
        , kind_(kind)
        , fontFamily_(fontFamily)
    {
        setObjectName(QStringLiteral("noticeCard"));
        setProperty("kind", kind);
        setProperty("persistent", persistent);
        setProperty("fading", false);
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);

        row_ = new QHBoxLayout(this);
        title_ = new QLabel(title, this);
        title_->setObjectName(QStringLiteral("noticeTitle"));
        title_->setAlignment(Qt::AlignCenter);
        row_->addWidget(title_, 1, Qt::AlignCenter);

        if (persistent)
        {
            close_ = new QToolButton(this);
            close_->setObjectName(QStringLiteral("noticeClose"));
            close_->setText(QStringLiteral("×"));
            close_->setToolTip(QStringLiteral("关闭提示"));
            close_->setCursor(Qt::PointingHandCursor);
        }
    }

    void setVisualScale(const int petScalePercent)
    {
        const int scale = std::max(MinimumNoticeScalePercent, petScalePercent);
        setFixedSize(
            scaledPixels(BaseNoticeWidth, scale),
            scaledPixels(BaseNoticeHeight, scale));

        row_->setContentsMargins(
            scaledPixels(18, scale, 6),
            scaledPixels(10, scale, 3),
            scaledPixels(18 + BaseNoticeCloseGutter, scale, 13),
            scaledPixels(28, scale, 8));
        row_->setSpacing(0);

        QFont titleFont = fontFamily_.isEmpty()
            ? QApplication::font()
            : QFont(fontFamily_);
        titleFont.setPixelSize(scaledPixels(29, scale, 10));
        title_->setFont(titleFont);
        title_->setStyleSheet(QStringLiteral("color: #11131a; background: transparent;"));

        if (close_)
        {
            QFont closeFont = QApplication::font();
            closeFont.setPixelSize(scaledPixels(18, scale));
            closeFont.setBold(true);
            close_->setFont(closeFont);
            const int closeSize = scaledPixels(22, scale);
            close_->setFixedSize(closeSize, closeSize);
            close_->setStyleSheet(QStringLiteral(
                "QToolButton { color: #11131a; background: transparent; border: none; padding: 0; }"
                "QToolButton:hover { background: rgba(17, 19, 26, 20); border-radius: 8px; }"));
            const int bubbleWidth = scaledPixels(BaseNoticeBubbleWidth, scale);
            const int cloudRight = static_cast<int>(std::ceil(bubbleWidth * 0.96));
            const int closeCenter = cloudRight + scaledPixels(5, scale);
            close_->move(
                closeCenter - closeSize / 2,
                scaledPixels(13, scale));
            close_->raise();
        }
        update();
    }

    [[nodiscard]] QToolButton* closeButton() const
    {
        return close_;
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal widthValue = static_cast<qreal>(width());
        const qreal heightValue = static_cast<qreal>(height());
        const qreal bubbleWidth = widthValue
            * static_cast<qreal>(BaseNoticeBubbleWidth)
            / static_cast<qreal>(BaseNoticeWidth);
        const qreal stroke = std::max<qreal>(1.25, bubbleWidth / 150.0);

        QPainterPath cloud;
        cloud.moveTo(bubbleWidth * 0.50, heightValue * 0.10);
        cloud.cubicTo(bubbleWidth * 0.57, heightValue * 0.06,
                      bubbleWidth * 0.64, heightValue * 0.08,
                      bubbleWidth * 0.68, heightValue * 0.18);
        cloud.cubicTo(bubbleWidth * 0.76, heightValue * 0.12,
                      bubbleWidth * 0.86, heightValue * 0.18,
                      bubbleWidth * 0.84, heightValue * 0.30);
        cloud.cubicTo(bubbleWidth * 0.95, heightValue * 0.32,
                      bubbleWidth * 0.96, heightValue * 0.44,
                      bubbleWidth * 0.91, heightValue * 0.50);
        cloud.cubicTo(bubbleWidth * 0.94, heightValue * 0.60,
                      bubbleWidth * 0.81, heightValue * 0.72,
                      bubbleWidth * 0.72, heightValue * 0.65);
        cloud.cubicTo(bubbleWidth * 0.65, heightValue * 0.78,
                      bubbleWidth * 0.56, heightValue * 0.76,
                      bubbleWidth * 0.50, heightValue * 0.68);
        cloud.cubicTo(bubbleWidth * 0.44, heightValue * 0.76,
                      bubbleWidth * 0.35, heightValue * 0.78,
                      bubbleWidth * 0.28, heightValue * 0.65);
        cloud.cubicTo(bubbleWidth * 0.19, heightValue * 0.72,
                      bubbleWidth * 0.06, heightValue * 0.60,
                      bubbleWidth * 0.09, heightValue * 0.50);
        cloud.cubicTo(bubbleWidth * 0.04, heightValue * 0.44,
                      bubbleWidth * 0.05, heightValue * 0.32,
                      bubbleWidth * 0.16, heightValue * 0.30);
        cloud.cubicTo(bubbleWidth * 0.14, heightValue * 0.18,
                      bubbleWidth * 0.24, heightValue * 0.12,
                      bubbleWidth * 0.32, heightValue * 0.18);
        cloud.cubicTo(bubbleWidth * 0.36, heightValue * 0.08,
                      bubbleWidth * 0.43, heightValue * 0.06,
                      bubbleWidth * 0.50, heightValue * 0.10);
        cloud.closeSubpath();

        QPen outline(QColor(QStringLiteral("#11131A")), stroke);
        outline.setCapStyle(Qt::RoundCap);
        outline.setJoinStyle(Qt::RoundJoin);
        painter.setPen(outline);
        painter.setBrush(noticeFill(kind_));
        painter.drawPath(cloud);

        painter.drawEllipse(
            QRectF(bubbleWidth * 0.25, heightValue * 0.79,
                   heightValue * 0.12, heightValue * 0.12));
        painter.drawEllipse(
            QRectF(bubbleWidth * 0.34, heightValue * 0.93,
                   heightValue * 0.065, heightValue * 0.065));
    }

private:
    QString kind_;
    QString fontFamily_;
    QHBoxLayout* row_ = nullptr;
    QLabel* title_ = nullptr;
    QToolButton* close_ = nullptr;
};
}

PetWindow::PetWindow(QWidget* parent)
    : QWidget(parent)
    , state_(this)
{
    setWindowTitle(QStringLiteral("鲸鱼娘桌宠 v3"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    windowScalePercent_ = DefaultWindowScalePercent;
    setFixedSize(scaledWindowSize(windowScalePercent_));
    setFocusPolicy(Qt::StrongFocus);

    buildUi();
    updateScaledUiMetrics();
    loadAssets();
    connectInteractions();
    character_->setCharacterPixmap(pixmaps_.value(QStringLiteral("idle")));
    restorePreferences();
}

PetWindow::~PetWindow()
{
    delete noticeHost_;
    noticeHost_ = nullptr;
}

void PetWindow::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 8);
    layout->setSpacing(0);

    // Keep status cards in their own native window. Resizing an ancestor of a
    // QOpenGLWidget recreates its backing buffer and causes a one-frame flash.
    // A sibling tool window lets cards change size without touching Live2D.
    noticeHost_ = new QWidget(
        nullptr,
        Qt::Tool
            | Qt::FramelessWindowHint
            | Qt::WindowStaysOnTopHint
            | Qt::WindowDoesNotAcceptFocus);
    noticeHost_->setObjectName(QStringLiteral("noticeHost"));
    noticeHost_->setAttribute(Qt::WA_TranslucentBackground);
    noticeHost_->setAttribute(Qt::WA_ShowWithoutActivating);
    noticeLayout_ = new QVBoxLayout(noticeHost_);
    noticeLayout_->setContentsMargins(0, 0, 0, 0);
    noticeLayout_->setSpacing(7);
    noticeHost_->hide();

    character_ = new CharacterWidget(this);
    character_->setMinimumSize(0, 0);
    character_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(character_, 0);
}

void PetWindow::updateScaledUiMetrics()
{
    const int margin = scaledPixels(BaseOuterMargin, windowScalePercent_);
    if (layout())
    {
        layout()->setContentsMargins(
            margin,
            margin,
            margin,
            scaledPixels(8, windowScalePercent_));
        layout()->setSpacing(0);
    }

    const QSize baseSize = scaledWindowSize(windowScalePercent_);
    character_->setFixedHeight(std::max(
        1,
        baseSize.height()
            - margin
            - scaledPixels(8, windowScalePercent_)));

    const int noticeScale = std::max(
        MinimumNoticeScalePercent, windowScalePercent_);
    noticeLayout_->setSpacing(scaledPixels(7, noticeScale, 2));
    const int noticeWidth = scaledPixels(BaseNoticeWidth, noticeScale);
    noticeHost_->setFixedWidth(noticeWidth);

    const auto cards = noticeHost_->findChildren<QFrame*>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QFrame* card : cards)
    {
        if (auto* bubble = dynamic_cast<ThoughtBubbleCard*>(card))
        {
            bubble->setVisualScale(windowScalePercent_);
        }
    }

    positionNoticeHost();
}

void PetWindow::positionNoticeHost()
{
    if (!noticeHost_ || !noticeHost_->isVisible())
    {
        return;
    }
    noticeLayout_->activate();
    const int desiredHeight = noticeLayout_->sizeHint().height();
    noticeHost_->setFixedHeight(desiredHeight);

    if (desiredHeight <= 0)
    {
        return;
    }

    const QRect petGeometry = frameGeometry();
    const bool placeOnLeft = noticePlacement_ == QStringLiteral("left");
    QPoint requested(
        petGeometry.left(),
        placeOnLeft
            ? petGeometry.top()
                + scaledPixels(BaseNoticeSideTopOffset, windowScalePercent_)
            : petGeometry.top()
                + scaledPixels(BaseNoticePetOverlap, windowScalePercent_)
                - desiredHeight);
    if (const QScreen* screen = QGuiApplication::screenAt(petGeometry.center()))
    {
        const QRect area = screen->availableGeometry();
        if (placeOnLeft)
        {
            const int sideOverlap = scaledPixels(
                BaseNoticeSideOverlap, windowScalePercent_);
            const int leftSide = petGeometry.left()
                - noticeHost_->width()
                + sideOverlap;
            const int rightSide = petGeometry.right()
                + 1
                - sideOverlap;
            if (leftSide >= area.left())
            {
                requested.setX(leftSide);
            }
            else if (rightSide + noticeHost_->width() - 1 <= area.right())
            {
                requested.setX(rightSide);
            }
        }
        requested.setX(std::clamp(
            requested.x(), area.left(), area.right() - noticeHost_->width() + 1));
        requested.setY(std::clamp(
            requested.y(),
            area.top(),
            std::max(area.top(), area.bottom() - desiredHeight + 1)));
    }
    if (noticeHost_->pos() != requested)
    {
        noticeHost_->move(requested);
    }
    if (!windowDragActive_)
    {
        noticeHost_->raise();
    }
}

void PetWindow::connectInteractions()
{
    connect(&state_, &PetStateController::primaryStateChanged,
            this, [this](const PetStateController::PrimaryState state) {
                switch (state)
                {
                case PetStateController::PrimaryState::Idle:
                    fadeTransientNotices(0);
                    break;
                case PetStateController::PrimaryState::Thinking:
                    fadePersistentNotices(0);
                    addNotice(
                        QStringLiteral("thinking"),
                        QStringLiteral("思考中"),
                        false);
                    break;
                case PetStateController::PrimaryState::Working:
                    fadePersistentNotices(0);
                    addNotice(
                        QStringLiteral("working"),
                        QStringLiteral("工作中"),
                        false);
                    break;
                case PetStateController::PrimaryState::AwaitingApproval:
                    addNotice(
                        QStringLiteral("approval"),
                        QStringLiteral("请求批准"),
                        true);
                    break;
                }
            });
    connect(&state_, &PetStateController::completionPosted,
            this, [this](const QString&) {
                addNotice(
                    QStringLiteral("completed"),
                    QStringLiteral("任务完成"),
                    true);
            });
    connect(&state_, &PetStateController::failurePosted,
            this, [this](const QString&) {
                addNotice(
                    QStringLiteral("failed"),
                    QStringLiteral("任务失败"),
                    true);
            });

    connect(character_, &CharacterWidget::primaryPressed,
            this, &PetWindow::beginWindowDrag);
    connect(character_, &CharacterWidget::primaryDragged,
            this, &PetWindow::continueWindowDrag);
    connect(character_, &CharacterWidget::primaryReleased,
            this, &PetWindow::endWindowDrag);
    connect(character_, &CharacterWidget::contextMenuRequested,
            this, &PetWindow::showContextMenu);

    const QStringList arguments = QCoreApplication::arguments();
    if (!arguments.contains(QStringLiteral("--composite-smoke"))
        && !arguments.contains(QStringLiteral("--live2d-only")))
    {
        auto* bridge = new CodexStatusBridge(this);
        connect(bridge, &CodexStatusBridge::eventReceived,
                this, &PetWindow::handleCodexEvent);
        (void)bridge->start();

        auto* activityWatcher = new CodexActivityWatcher(this);
        connect(activityWatcher, &CodexActivityWatcher::eventDetected,
                this, &PetWindow::handleCodexEvent);
        // Initial discovery can immediately restore an active task. Defer it
        // until construction and the first widget layout are complete.
        QTimer::singleShot(0, activityWatcher, [activityWatcher]() {
            activityWatcher->start();
        });
    }
}

void PetWindow::loadAssets()
{
    const QString base = QCoreApplication::applicationDirPath() + QStringLiteral("/assets/");
    const QHash<QString, QString> files = {
        {QStringLiteral("idle"), QStringLiteral("pose-idle-front-v2.png")},
    };

    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        pixmaps_.insert(it.key(), QPixmap(base + it.value()));
    }

    const int fontId = QFontDatabase::addApplicationFont(
        base + QStringLiteral("fonts/站酷快乐体2016修订版.ttf"));
    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (!families.isEmpty())
    {
        noticeFontFamily_ = families.constFirst();
    }
}

void PetWindow::showContextMenu(const QPoint& globalPosition)
{
    QMenu menu;

    QMenu* scaleMenu = menu.addMenu(QStringLiteral("调整窗口大小"));
    auto* scaleWidget = new QWidget(scaleMenu);
    auto* scaleLayout = new QVBoxLayout(scaleWidget);
    scaleLayout->setContentsMargins(12, 8, 12, 8);
    scaleLayout->setSpacing(5);
    auto* scaleValue = new QLabel(
        QStringLiteral("%1%").arg(windowScalePercent_), scaleWidget);
    scaleValue->setAlignment(Qt::AlignCenter);
    auto* scaleSlider = new QSlider(Qt::Horizontal, scaleWidget);
    scaleSlider->setRange(15, 200);
    scaleSlider->setSingleStep(1);
    scaleSlider->setPageStep(5);
    scaleSlider->setValue(windowScalePercent_);
    scaleSlider->setMinimumWidth(230);
    scaleSlider->setToolTip(QStringLiteral("15%–200%"));
    scaleLayout->addWidget(scaleValue);
    scaleLayout->addWidget(scaleSlider);
    auto* scaleAction = new QWidgetAction(scaleMenu);
    scaleAction->setDefaultWidget(scaleWidget);
    scaleMenu->addAction(scaleAction);
    connect(scaleSlider, &QSlider::valueChanged, scaleValue,
            [scaleValue](const int value) {
                scaleValue->setText(QStringLiteral("%1%").arg(value));
            });
    auto* scaleFrameTimer = new QTimer(scaleMenu);
    scaleFrameTimer->setSingleShot(true);
    scaleFrameTimer->setTimerType(Qt::PreciseTimer);
    scaleFrameTimer->setInterval(16);
    connect(scaleSlider, &QSlider::valueChanged, scaleFrameTimer,
            [this, scaleFrameTimer]() {
                beginWindowScale();
                if (!scaleFrameTimer->isActive())
                {
                    scaleFrameTimer->start();
                }
            });
    connect(scaleFrameTimer, &QTimer::timeout, this, [this, scaleSlider]() {
        applyWindowScale(scaleSlider->value(), false);
    });
    connect(scaleSlider, &QSlider::sliderReleased, this,
            [this, scaleSlider, scaleFrameTimer]() {
                scaleFrameTimer->stop();
                applyWindowScale(scaleSlider->value(), false);
                endWindowScale();
            });
    connect(scaleMenu, &QMenu::aboutToHide, this,
            [this, scaleSlider, scaleFrameTimer]() {
        scaleFrameTimer->stop();
        applyWindowScale(scaleSlider->value(), false);
        endWindowScale();
        savePreferences();
    });

    QMenu* placementMenu = menu.addMenu(QStringLiteral("显示位置"));
    QAction* abovePlacement = placementMenu->addAction(QStringLiteral("上方"));
    QAction* leftPlacement = placementMenu->addAction(QStringLiteral("左侧"));
    abovePlacement->setCheckable(true);
    leftPlacement->setCheckable(true);
    abovePlacement->setChecked(noticePlacement_ == QStringLiteral("above"));
    leftPlacement->setChecked(noticePlacement_ == QStringLiteral("left"));

    QAction* lockPetAction = menu.addAction(QStringLiteral("锁定桌宠"));
    lockPetAction->setCheckable(true);
    lockPetAction->setChecked(petLocked_);
    QAction* clearNoticesAction = menu.addAction(QStringLiteral("清除提示"));
    clearNoticesAction->setEnabled(!noticeHost_->findChildren<QFrame*>(
        QString(), Qt::FindDirectChildrenOnly).isEmpty());
    menu.addSeparator();
    QAction* about = menu.addAction(QStringLiteral("关于"));
    QAction* quit = menu.addAction(QStringLiteral("退出"));

    QAction* selected = menu.exec(globalPosition);
    if (!selected)
    {
        return;
    }

    if (selected == abovePlacement || selected == leftPlacement)
    {
        noticePlacement_ = selected == leftPlacement
            ? QStringLiteral("left")
            : QStringLiteral("above");
        positionNoticeHost();
        savePreferences();
    }
    else if (selected == about)
    {
        showAboutDialog();
    }
    else if (selected == lockPetAction)
    {
        petLocked_ = lockPetAction->isChecked();
        windowDragActive_ = false;
        character_->setDragLocked(petLocked_);
        savePreferences();
    }
    else if (selected == clearNoticesAction)
    {
        clearNotices();
    }
    else if (selected == quit)
    {
        close();
    }
}

// Shared by the context menu and the visual packaging checks.
void PetWindow::setWindowScale(const int percent)
{
    applyWindowScale(percent, true);
}

void PetWindow::applyWindowScale(const int percent, const bool persist)
{
    const int boundedPercent = std::clamp(percent, 15, 200);
    if (boundedPercent == windowScalePercent_)
    {
        return;
    }

    const QPoint bottomRightAnchor = windowScaleActive_
        ? windowScaleBottomRightAnchor_
        : frameGeometry().bottomRight();
    windowScalePercent_ = boundedPercent;
    setFixedSize(scaledWindowSize(windowScalePercent_));
    updateScaledUiMetrics();

    const QPoint requested(
        bottomRightAnchor.x() - width() + 1,
        bottomRightAnchor.y() - height() + 1);
    move(clampedTopLeft(requested));
    if (persist)
    {
        savePreferences();
    }
}

void PetWindow::beginWindowScale()
{
    if (windowScaleActive_)
    {
        return;
    }
    windowScaleBottomRightAnchor_ = frameGeometry().bottomRight();
    windowScaleActive_ = true;
}

void PetWindow::endWindowScale()
{
    windowScaleActive_ = false;
}

void PetWindow::showAboutDialog()
{
    QDialog about(this);
    about.setWindowTitle(QStringLiteral("关于 WhaleMaid-for-Codex"));
    about.setModal(true);
    about.setMinimumWidth(380);

    auto* layout = new QVBoxLayout(&about);
    auto* information = new QLabel(&about);
    information->setTextFormat(Qt::RichText);
    information->setTextInteractionFlags(Qt::TextBrowserInteraction);
    information->setOpenExternalLinks(true);
    information->setText(QStringLiteral(
        "<p><b>WhaleMaid-for-Codex v1.3.3</b></p>"
        "<p>作者：EussGaler</p>"
        "<p>bilibili：<a href=\"https://space.bilibili.com/222999797\">"
        "space.bilibili.com/222999797</a></p>"
        "<p>GitHub：<a href=\"https://github.com/EussGaler\">"
        "github.com/EussGaler</a></p>"));
    layout->addWidget(information);

    auto* separator = new QFrame(&about);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    auto* startWithWindows = new QCheckBox(
        QStringLiteral("开机时自动启动 WhaleMaid"), &about);
    startWithWindows->setChecked(StartupSettings::isEnabled());
    startWithWindows->setToolTip(QStringLiteral("更改会立即生效，手动启动不会修改此选择"));
    layout->addWidget(startWithWindows);

    connect(startWithWindows, &QCheckBox::clicked, &about,
            [&about, startWithWindows](const bool enabled) {
                QString errorMessage;
                if (StartupSettings::setEnabled(enabled, &errorMessage))
                {
                    return;
                }
                const QSignalBlocker blocker(startWithWindows);
                startWithWindows->setChecked(!enabled);
                QMessageBox::warning(
                    &about,
                    QStringLiteral("无法修改开机启动"),
                    errorMessage);
            });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &about);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    connect(buttons, &QDialogButtonBox::rejected, &about, &QDialog::reject);
    layout->addWidget(buttons);
    about.exec();
}

void PetWindow::showAboutForTesting()
{
    showAboutDialog();
}

bool PetWindow::saveCompositeForTesting(const QString& path)
{
    QRect bounds = frameGeometry();
    if (noticeHost_->isVisible())
    {
        bounds = bounds.united(noticeHost_->frameGeometry());
    }

    QImage image(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.drawPixmap(frameGeometry().topLeft() - bounds.topLeft(), grab());
    if (noticeHost_->isVisible())
    {
        painter.drawPixmap(
            noticeHost_->frameGeometry().topLeft() - bounds.topLeft(),
            noticeHost_->grab());
    }
    return image.save(path, "PNG");
}

void PetWindow::beginWindowDrag(const QPoint& globalPosition)
{
    if (petLocked_)
    {
        return;
    }
    // Preserve the exact pointer-to-window offset. Do not snap the window to
    // a guessed anchor when the dragged pose becomes active.
    dragOffset_ = globalPosition - frameGeometry().topLeft();
    windowDragActive_ = true;
    systemWindowMoveActive_ = false;
#ifdef Q_OS_LINUX
    if (QWindow* nativeWindow = windowHandle())
    {
        systemWindowMoveActive_ = nativeWindow->startSystemMove();
    }
    if (qEnvironmentVariableIsSet("WHALEMAID_TRACE_INTERACTIONS"))
    {
        qInfo() << "X11 system window move accepted:" << systemWindowMoveActive_;
    }
#endif
}

void PetWindow::continueWindowDrag(const QPoint& globalPosition)
{
    if (!windowDragActive_ || systemWindowMoveActive_)
    {
        return;
    }

    move(clampedTopLeft(WindowPlacement::requestedTopLeft(globalPosition, dragOffset_)));
}

void PetWindow::endWindowDrag()
{
    if (!windowDragActive_)
    {
        return;
    }

    windowDragActive_ = false;
    systemWindowMoveActive_ = false;
    savePreferences();
}

// Public entry point used by both the local Codex bridge and UI smoke tests.
void PetWindow::handleCodexEvent(
    const QString& status,
    const QString& message,
    const QString& sessionId,
    const QString& turnId)
{
    if (status == QStringLiteral("activate"))
    {
        show();
        setWindowState(windowState() & ~Qt::WindowMinimized);
        raise();
        activateWindow();
        if (noticeHost_->isVisible())
        {
            positionNoticeHost();
            noticeHost_->raise();
        }
        return;
    }

    if (status == QStringLiteral("shutdown"))
    {
        QApplication::quit();
        return;
    }

    const QString effectiveSession = sessionId.isEmpty()
        ? QStringLiteral("manual")
        : sessionId;

    if (status == QStringLiteral("idle"))
    {
        sessionStates_.remove(effectiveSession);
        if (latestSessionId_ == effectiveSession)
        {
            applyLatestSessionState();
        }
        return;
    }

    const auto existing = sessionStates_.constFind(effectiveSession);
    if (existing != sessionStates_.constEnd()
        && existing->status == status
        && existing->turnId == turnId)
    {
        return;
    }

    SessionState snapshot;
    snapshot.status = status;
    snapshot.message = message;
    snapshot.turnId = turnId;
    snapshot.sequence = ++sessionSequence_;
    sessionStates_.insert(effectiveSession, snapshot);
    latestSessionId_ = effectiveSession;

    applyLatestSessionState();
}

void PetWindow::applyLatestSessionState()
{
    if (sessionStates_.isEmpty())
    {
        latestSessionId_.clear();
        state_.setPrimaryState(PetStateController::PrimaryState::Idle);
        return;
    }

    auto latest = sessionStates_.constBegin();
    for (auto it = sessionStates_.constBegin(); it != sessionStates_.constEnd(); ++it)
    {
        if (it.value().sequence > latest.value().sequence)
        {
            latest = it;
        }
    }
    latestSessionId_ = latest.key();
    const SessionState& snapshot = latest.value();
    if (snapshot.status == QStringLiteral("thinking"))
    {
        state_.setPrimaryState(PetStateController::PrimaryState::Thinking);
    }
    else if (snapshot.status == QStringLiteral("working"))
    {
        state_.setPrimaryState(PetStateController::PrimaryState::Working);
    }
    else if (snapshot.status == QStringLiteral("approval"))
    {
        state_.setPrimaryState(PetStateController::PrimaryState::AwaitingApproval);
    }
    else if (snapshot.status == QStringLiteral("completed"))
    {
        const QString outcomeKey = latest.key()
            + QChar(0x1f)
            + (snapshot.turnId.isEmpty()
                   ? QStringLiteral("no-turn:") + snapshot.message
                   : snapshot.turnId);
        if (!postedOutcomeKeys_.contains(outcomeKey))
        {
            postedOutcomeKeys_.insert(outcomeKey);
            state_.postCompletion(snapshot.message);
        }
        state_.setPrimaryState(PetStateController::PrimaryState::Idle);
    }
    else if (snapshot.status == QStringLiteral("failed"))
    {
        const QString outcomeKey = latest.key()
            + QChar(0x1f)
            + (snapshot.turnId.isEmpty()
                   ? QStringLiteral("no-turn:") + snapshot.message
                   : snapshot.turnId);
        if (!postedOutcomeKeys_.contains(outcomeKey))
        {
            postedOutcomeKeys_.insert(outcomeKey);
            state_.postFailure(snapshot.message);
        }
        state_.setPrimaryState(PetStateController::PrimaryState::Idle);
    }
    else if (snapshot.status == QStringLiteral("clear"))
    {
        clearNotices();
    }
}

void PetWindow::closeEvent(QCloseEvent* event)
{
    savePreferences();
    noticeHost_->hide();
    const QString flagPath = PlatformPaths::manualExitFlagPath();
    if (!flagPath.isEmpty())
    {
        const QString directory = QFileInfo(flagPath).absolutePath();
        QDir().mkpath(directory);
        QFile flag(flagPath);
        if (flag.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            flag.write("User closed WhaleMaid.\n");
        }
    }
    event->accept();
    QApplication::quit();
}

void PetWindow::restorePreferences()
{
    const PetPreferencesData preferences = PetPreferences::load();
    windowScalePercent_ = preferences.scalePercent;
    setFixedSize(scaledWindowSize(windowScalePercent_));
    updateScaledUiMetrics();

    petLocked_ = preferences.locked;
    noticePlacement_ = preferences.noticePlacement;
    windowDragActive_ = false;
    character_->setDragLocked(petLocked_);

    QPoint requested;
    bool restoredPosition = false;
    if (preferences.hasPosition)
    {
        const auto screens = QGuiApplication::screens();
        if (!preferences.screenName.isEmpty() && preferences.hasScreenOffset)
        {
            for (const QScreen* screen : screens)
            {
                if (screen->name() == preferences.screenName)
                {
                    requested = screen->availableGeometry().topLeft()
                        + preferences.screenOffset;
                    restoredPosition = true;
                    break;
                }
            }
        }

        if (!restoredPosition)
        {
            const QRect savedWindow(preferences.topLeft, size());
            for (const QScreen* screen : screens)
            {
                if (savedWindow.intersects(screen->availableGeometry()))
                {
                    requested = preferences.topLeft;
                    restoredPosition = true;
                    break;
                }
            }
        }
    }

    if (!restoredPosition)
    {
        if (const QScreen* screen = QGuiApplication::primaryScreen())
        {
            const QRect area = screen->availableGeometry();
            requested = QPoint(
                area.right() - width() - 20,
                area.bottom() - height() - 20);
        }
    }

    move(clampedTopLeft(requested));
}

void PetWindow::savePreferences() const
{
    PetPreferencesData preferences;
    preferences.scalePercent = windowScalePercent_;
    preferences.locked = petLocked_;
    preferences.noticePlacement = noticePlacement_;
    preferences.hasPosition = true;
    preferences.topLeft = frameGeometry().topLeft();

    if (const QScreen* screen = QGuiApplication::screenAt(frameGeometry().center()))
    {
        preferences.screenName = screen->name();
        preferences.hasScreenOffset = true;
        preferences.screenOffset = preferences.topLeft
            - screen->availableGeometry().topLeft();
    }

    PetPreferences::save(preferences);
}

void PetWindow::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    positionNoticeHost();
}

void PetWindow::addNotice(
    const QString& kind, const QString& title, const bool persistent)
{
    fadeTransientNotices(0);

    auto* card = new ThoughtBubbleCard(
        kind, title, noticeFontFamily_, persistent, noticeHost_);
    card->setVisualScale(windowScalePercent_);

    if (QToolButton* close = card->closeButton())
    {
        connect(close, &QToolButton::clicked, this, [this, card]() {
            removeNotice(card);
        });
    }

    noticeLayout_->addWidget(card);
    noticeHost_->show();
    positionNoticeHost();
    QTimer::singleShot(0, this, [this]() { positionNoticeHost(); });
}

void PetWindow::fadeTransientNotices(const int delayMilliseconds)
{
    const auto cards = noticeHost_->findChildren<QFrame*>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QFrame* card : cards)
    {
        if (!card->property("persistent").toBool()
            && !card->property("fading").toBool())
        {
            fadeAndRemoveNotice(card, delayMilliseconds);
        }
    }
}

void PetWindow::fadePersistentNotices(const int delayMilliseconds)
{
    const auto cards = noticeHost_->findChildren<QFrame*>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QFrame* card : cards)
    {
        if (card->property("persistent").toBool()
            && !card->property("fading").toBool())
        {
            fadeAndRemoveNotice(card, delayMilliseconds);
        }
    }
}

void PetWindow::fadeAndRemoveNotice(QFrame* card, const int delayMilliseconds)
{
    if (!card)
    {
        return;
    }
    card->setProperty("fading", true);
    QPointer<QFrame> guarded(card);
    QTimer::singleShot(delayMilliseconds, this, [this, guarded]() {
        if (!guarded)
        {
            return;
        }
        auto* effect = new QGraphicsOpacityEffect(guarded);
        guarded->setGraphicsEffect(effect);
        auto* animation = new QPropertyAnimation(effect, "opacity", guarded);
        animation->setDuration(450);
        animation->setStartValue(1.0);
        animation->setEndValue(0.0);
        connect(animation, &QPropertyAnimation::finished, this, [this, guarded]() {
            if (guarded)
            {
                removeNotice(guarded);
            }
        });
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void PetWindow::removeNotice(QFrame* card)
{
    if (!card)
    {
        return;
    }
    card->hide();
    noticeLayout_->removeWidget(card);
    card->deleteLater();
    noticeLayout_->invalidate();
    noticeHost_->updateGeometry();
    if (layout())
    {
        layout()->activate();
    }
    update();
    QTimer::singleShot(0, this, [this]() {
        if (noticeHost_->findChildren<QFrame*>(
                QString(), Qt::FindDirectChildrenOnly).isEmpty())
        {
            noticeHost_->hide();
        }
        positionNoticeHost();
    });
}

void PetWindow::clearNotices()
{
    const auto cards = noticeHost_->findChildren<QFrame*>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QFrame* card : cards)
    {
        noticeLayout_->removeWidget(card);
        card->deleteLater();
    }
    noticeHost_->hide();
    positionNoticeHost();
}

QPoint PetWindow::clampedTopLeft(const QPoint& requested) const
{
    QRect virtualArea;
    const auto screens = QGuiApplication::screens();
    for (const QScreen* screen : screens)
    {
        virtualArea = virtualArea.united(screen->availableGeometry());
    }

    if (!virtualArea.isValid())
    {
        return requested;
    }

    return WindowPlacement::clampToVisibleArea(
        requested,
        size(),
        virtualArea,
        MinimumVisiblePixels);
}

void PetWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}
