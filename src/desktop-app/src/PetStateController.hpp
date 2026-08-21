#pragma once

#include <QObject>
#include <QString>

class PetStateController final : public QObject
{
    Q_OBJECT

public:
    enum class PrimaryState
    {
        Idle,
        Thinking,
        Working,
        AwaitingApproval
    };
    Q_ENUM(PrimaryState)

    explicit PetStateController(QObject* parent = nullptr);

    [[nodiscard]] PrimaryState primaryState() const noexcept;
    void setPrimaryState(PrimaryState state);
    void postCompletion(const QString& message);
    void postFailure(const QString& message);

Q_SIGNALS:
    void primaryStateChanged(PrimaryState state);
    void completionPosted(const QString& message);
    void failurePosted(const QString& message);

private:
    PrimaryState primaryState_ = PrimaryState::Idle;
};
