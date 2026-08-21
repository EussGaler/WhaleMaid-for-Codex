#include "PetStateController.hpp"

PetStateController::PetStateController(QObject* parent)
    : QObject(parent)
{
}

PetStateController::PrimaryState PetStateController::primaryState() const noexcept
{
    return primaryState_;
}

void PetStateController::setPrimaryState(const PrimaryState state)
{
    if (primaryState_ == state)
    {
        return;
    }

    primaryState_ = state;

    Q_EMIT primaryStateChanged(primaryState_);
}

void PetStateController::postCompletion(const QString& message)
{
    Q_EMIT completionPosted(message);
}

void PetStateController::postFailure(const QString& message)
{
    Q_EMIT failurePosted(message);
}
