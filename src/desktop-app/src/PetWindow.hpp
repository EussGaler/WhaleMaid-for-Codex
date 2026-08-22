#pragma once

#include "PetStateController.hpp"

#include <QHash>
#include <QPoint>
#include <QPixmap>
#include <QPointer>
#include <QString>
#include <QWidget>

class CharacterWidget;
class QCloseEvent;
class QFrame;
class QLabel;
class QMoveEvent;
class QVBoxLayout;

class PetWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit PetWindow(QWidget* parent = nullptr);
    ~PetWindow() override;
    void handleCodexEvent(
        const QString& status,
        const QString& message,
        const QString& sessionId = {},
        const QString& turnId = {});
    void setWindowScale(int percent);
    void showAboutForTesting();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

private:
    void buildUi();
    void connectInteractions();
    void loadAssets();
    void restorePreferences();
    void savePreferences() const;
    void showContextMenu(const QPoint& globalPosition);
    void updateScaledUiMetrics();
    void positionNoticeHost();
    void showAboutDialog();
    void beginWindowDrag(const QPoint& globalPosition);
    void continueWindowDrag(const QPoint& globalPosition);
    void endWindowDrag();
    void addNotice(const QString& kind, const QString& title,
                   const QString& message, bool persistent);
    void fadeTransientNotices(int delayMilliseconds);
    void fadeAndRemoveNotice(QFrame* card, int delayMilliseconds);
    void removeNotice(QFrame* card);
    void clearNotices();
    void applyLatestSessionState();

    struct SessionState
    {
        QString status;
        QString message;
        QString turnId;
        qint64 sequence = 0;
    };

    [[nodiscard]] QPoint clampedTopLeft(const QPoint& requested) const;
    PetStateController state_;
    CharacterWidget* character_ = nullptr;
    QWidget* noticeHost_ = nullptr;
    QVBoxLayout* noticeLayout_ = nullptr;

    QPoint dragOffset_;
    bool windowDragActive_ = false;
    bool petLocked_ = false;
    int windowScalePercent_ = 75;
    QHash<QString, QPixmap> pixmaps_;
    QHash<QString, SessionState> sessionStates_;
    QString latestSessionId_;
    qint64 sessionSequence_ = 0;
};
