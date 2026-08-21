#include "PetWindow.hpp"

#include "CharacterWidget.hpp"
#include "CodexActivityWatcher.hpp"
#include "CodexStatusBridge.hpp"
#include "StartupSettings.hpp"
#include "WindowPlacement.hpp"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFrame>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QToolButton>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr int BaseWindowWidth = 520;
constexpr int BaseWindowHeight = 720;
constexpr int DefaultWindowScalePercent = 75;
constexpr int MinimumVisiblePixels = 96;
constexpr int BaseNoticeWidth = 330;
constexpr int BaseOuterMargin = 12;

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

    if (const QScreen* screen = QGuiApplication::primaryScreen())
    {
        const QRect area = screen->availableGeometry();
        move(area.right() - width() - 20, area.bottom() - height() - 20);
    }
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

    noticeLayout_->setSpacing(scaledPixels(7, windowScalePercent_));
    const int noticeWidth = std::min(
        scaledPixels(BaseNoticeWidth, windowScalePercent_),
        std::max(1, scaledWindowSize(windowScalePercent_).width() - margin * 2));
    noticeHost_->setFixedWidth(noticeWidth);

    const auto cards = noticeHost_->findChildren<QFrame*>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QFrame* card : cards)
    {
        card->setFixedWidth(noticeWidth);
        if (auto* row = qobject_cast<QHBoxLayout*>(card->layout()))
        {
            row->setContentsMargins(
                scaledPixels(14, windowScalePercent_),
                scaledPixels(10, windowScalePercent_),
                scaledPixels(9, windowScalePercent_),
                scaledPixels(10, windowScalePercent_));
            row->setSpacing(scaledPixels(7, windowScalePercent_));
        }
    }

    const QString noticeStyle = QStringLiteral(R"(
        QFrame#noticeCard {
            background: rgba(15, 32, 66, 220);
            border: 1px solid rgba(125, 190, 255, 180);
            border-radius: %1px;
        }
        QFrame#noticeCard[kind="approval"] {
            background: rgba(92, 64, 18, 235);
            border-color: rgba(255, 205, 92, 210);
        }
        QFrame#noticeCard[kind="completed"] {
            background: rgba(32, 86, 96, 235);
            border-color: rgba(128, 242, 218, 205);
        }
        QFrame#noticeCard[kind="failed"] {
            background: rgba(102, 36, 45, 238);
            border-color: rgba(255, 128, 145, 210);
        }
        QLabel#noticeTitle {
            color: #f4f8ff;
            font-size: %2px;
            font-weight: 700;
        }
        QLabel#noticeDetail {
            color: #bcd9ff;
            font-size: %3px;
        }
        QToolButton#noticeClose {
            color: #f4f8ff;
            background: transparent;
            border: none;
            font-size: %4px;
            font-weight: 700;
            padding: 0px %5px;
        }
        QToolButton#noticeClose:hover {
            background: rgba(255, 255, 255, 35);
            border-radius: 8px;
        }
    )")
        .arg(scaledPixels(13, windowScalePercent_))
        .arg(scaledPixels(15, windowScalePercent_, 7))
        .arg(scaledPixels(13, windowScalePercent_, 7))
        .arg(scaledPixels(17, windowScalePercent_, 8))
        .arg(scaledPixels(3, windowScalePercent_));
    setStyleSheet(noticeStyle);
    noticeHost_->setStyleSheet(noticeStyle);

    positionNoticeHost();
}

void PetWindow::positionNoticeHost()
{
    if (!noticeHost_)
    {
        return;
    }
    noticeLayout_->activate();
    const int desiredHeight = noticeHost_->isVisible()
        ? noticeLayout_->sizeHint().height()
        : 0;
    noticeHost_->setFixedHeight(desiredHeight);

    if (desiredHeight <= 0)
    {
        return;
    }

    const QRect petGeometry = frameGeometry();
    QPoint requested(
        petGeometry.right() - noticeHost_->width() + 1,
        petGeometry.top() - scaledPixels(8, windowScalePercent_) - desiredHeight);
    if (const QScreen* screen = QGuiApplication::screenAt(petGeometry.center()))
    {
        const QRect area = screen->availableGeometry();
        requested.setX(std::clamp(
            requested.x(), area.left(), area.right() - noticeHost_->width() + 1));
        requested.setY(std::max(area.top(), requested.y()));
    }
    noticeHost_->move(requested);
    noticeHost_->raise();
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
                    addNotice(
                        QStringLiteral("thinking"),
                        QStringLiteral("Codex 思考中"),
                        QStringLiteral("正在理解任务并规划下一步"),
                        false);
                    break;
                case PetStateController::PrimaryState::Working:
                    addNotice(
                        QStringLiteral("working"),
                        QStringLiteral("Codex 工作中"),
                        QStringLiteral("正在调用工具并处理任务"),
                        false);
                    break;
                case PetStateController::PrimaryState::AwaitingApproval:
                    addNotice(
                        QStringLiteral("approval"),
                        QStringLiteral("等待你的确认"),
                        QStringLiteral("请返回 Codex 查看并确认操作"),
                        true);
                    break;
                }
            });
    connect(&state_, &PetStateController::completionPosted,
            this, [this](const QString& message) {
                addNotice(
                    QStringLiteral("completed"),
                    QStringLiteral("Codex 已完成"),
                    message.isEmpty() ? QStringLiteral("本轮任务已经处理完成") : message,
                    true);
            });
    connect(&state_, &PetStateController::failurePosted,
            this, [this](const QString& message) {
                addNotice(
                    QStringLiteral("failed"),
                    QStringLiteral("Codex 遇到问题"),
                    message.isEmpty() ? QStringLiteral("任务未能完成，请返回 Codex 查看详情") : message,
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
}

void PetWindow::showContextMenu(const QPoint& globalPosition)
{
    QMenu menu;

    QMenu* scaleMenu = menu.addMenu(QStringLiteral("调整窗口大小"));
    QActionGroup scaleActions(&menu);
    scaleActions.setExclusive(true);
    constexpr std::array scaleOptions{50, 60, 75, 80, 100, 125, 150, 175, 200};
    for (const int percent : scaleOptions)
    {
        QAction* action = scaleMenu->addAction(QStringLiteral("%1%").arg(percent));
        action->setCheckable(true);
        action->setChecked(percent == windowScalePercent_);
        action->setData(percent);
        scaleActions.addAction(action);
    }

    QAction* about = menu.addAction(QStringLiteral("关于"));
    menu.addSeparator();
    QAction* lockPetAction = menu.addAction(QStringLiteral("锁定桌宠"));
    lockPetAction->setCheckable(true);
    lockPetAction->setChecked(petLocked_);
    QAction* clearNoticesAction = menu.addAction(QStringLiteral("清除提示"));
    clearNoticesAction->setEnabled(!noticeHost_->findChildren<QFrame*>(
        QString(), Qt::FindDirectChildrenOnly).isEmpty());
    QAction* quit = menu.addAction(QStringLiteral("退出"));

    QAction* selected = menu.exec(globalPosition);
    if (!selected)
    {
        return;
    }

    if (scaleActions.actions().contains(selected))
    {
        setWindowScale(selected->data().toInt());
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
    if (percent == windowScalePercent_)
    {
        return;
    }

    const QPoint bottomRightAnchor = frameGeometry().bottomRight();
    windowScalePercent_ = percent;
    setFixedSize(scaledWindowSize(windowScalePercent_));
    updateScaledUiMetrics();

    const QPoint requested(
        bottomRightAnchor.x() - width() + 1,
        bottomRightAnchor.y() - height() + 1);
    move(clampedTopLeft(requested));
}

void PetWindow::showAboutDialog()
{
    QDialog about(this);
    about.setWindowTitle(QStringLiteral("关于 WhaleMaid 桌宠"));
    about.setModal(true);
    about.setMinimumWidth(380);

    auto* layout = new QVBoxLayout(&about);
    auto* information = new QLabel(&about);
    information->setTextFormat(Qt::RichText);
    information->setTextInteractionFlags(Qt::TextBrowserInteraction);
    information->setOpenExternalLinks(true);
    information->setText(QStringLiteral(
        "<p><b>WhaleMaid 桌宠</b></p>"
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
}

void PetWindow::continueWindowDrag(const QPoint& globalPosition)
{
    if (!windowDragActive_)
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
        state_.postCompletion(snapshot.message);
        state_.setPrimaryState(PetStateController::PrimaryState::Idle);
    }
    else if (snapshot.status == QStringLiteral("failed"))
    {
        state_.postFailure(snapshot.message);
        state_.setPrimaryState(PetStateController::PrimaryState::Idle);
    }
    else if (snapshot.status == QStringLiteral("clear"))
    {
        clearNotices();
    }
}

void PetWindow::closeEvent(QCloseEvent* event)
{
    noticeHost_->hide();
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty())
    {
        const QString directory = QDir(localAppData).filePath(QStringLiteral("WhaleMaid"));
        QDir().mkpath(directory);
        QFile flag(QDir(directory).filePath(QStringLiteral("manual-exit.flag")));
        if (flag.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            flag.write("User closed WhaleMaid.\n");
        }
    }
    event->accept();
    QApplication::quit();
}

void PetWindow::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    positionNoticeHost();
}

void PetWindow::addNotice(const QString& kind, const QString& title,
                          const QString& message, const bool persistent)
{
    fadeTransientNotices(2500);

    auto* card = new QFrame(noticeHost_);
    card->setObjectName(QStringLiteral("noticeCard"));
    card->setProperty("kind", kind);
    card->setProperty("persistent", persistent);
    card->setProperty("fading", false);
    card->setFixedWidth(noticeHost_->width());

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(14, 10, 9, 10);
    row->setSpacing(7);
    auto* textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("noticeTitle"));
    auto* detailLabel = new QLabel(message, card);
    detailLabel->setObjectName(QStringLiteral("noticeDetail"));
    detailLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(detailLabel);
    row->addLayout(textLayout, 1);

    if (persistent)
    {
        auto* close = new QToolButton(card);
        close->setObjectName(QStringLiteral("noticeClose"));
        close->setText(QStringLiteral("×"));
        close->setToolTip(QStringLiteral("关闭提示"));
        close->setCursor(Qt::PointingHandCursor);
        connect(close, &QToolButton::clicked, this, [this, card]() {
            removeNotice(card);
        });
        row->addWidget(close, 0, Qt::AlignTop);
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
