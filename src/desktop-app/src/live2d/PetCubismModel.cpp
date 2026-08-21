#include <GL/glew.h>

#include "live2d/PetCubismModel.hpp"

#include <CubismDefaultParameterId.hpp>
#include <CubismFramework.hpp>
#include <CubismModelSettingJson.hpp>
#include <Effect/CubismLook.hpp>
#include <Id/CubismIdManager.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Motion/CubismLookUpdater.hpp>
#include <Motion/CubismPhysicsUpdater.hpp>
#include <Motion/CubismPoseUpdater.hpp>
#include <Physics/CubismPhysics.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace
{
constexpr Csm::csmFloat32 MaximumFrameDelta = 0.1F;
constexpr float Pi = 3.14159265358979323846F;
constexpr float BreathCycleSeconds = 2.7F;
constexpr float BlinkClosingSeconds = 0.10F;
constexpr float BlinkClosedSeconds = 0.06F;
constexpr float BlinkOpeningSeconds = 0.16F;
}

PetCubismModel::PetCubismModel() = default;

PetCubismModel::~PetCubismModel()
{
    releaseOwnedResources();
}

QByteArray PetCubismModel::readAsset(const QString& relativePath) const
{
    if (relativePath.isEmpty())
    {
        return {};
    }

    QFile file(QDir(assetDirectory_).filePath(relativePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning().noquote() << "Live2D asset could not be read:" << file.fileName();
        return {};
    }
    return file.readAll();
}

bool PetCubismModel::load(
    const QString& assetDirectory,
    const QString& modelName,
    const int width,
    const int height)
{
    assetDirectory_ = QDir(assetDirectory).absolutePath();
    modelName_ = modelName;
    _updating = true;
    _initialized = false;

    const QByteArray settingBytes = readAsset(modelName_ + QStringLiteral(".model3.json"));
    if (settingBytes.isEmpty())
    {
        return false;
    }

    modelSetting_ = new Csm::CubismModelSettingJson(
        reinterpret_cast<const Csm::csmByte*>(settingBytes.constData()),
        static_cast<Csm::csmSizeInt>(settingBytes.size()));

    const QByteArray mocBytes = readAsset(QString::fromUtf8(modelSetting_->GetModelFileName()));
    if (mocBytes.isEmpty())
    {
        return false;
    }
    LoadModel(
        reinterpret_cast<const Csm::csmByte*>(mocBytes.constData()),
        static_cast<Csm::csmSizeInt>(mocBytes.size()));
    if (!_model)
    {
        return false;
    }

    const QByteArray poseBytes = readAsset(QString::fromUtf8(modelSetting_->GetPoseFileName()));
    if (!poseBytes.isEmpty())
    {
        LoadPose(
            reinterpret_cast<const Csm::csmByte*>(poseBytes.constData()),
            static_cast<Csm::csmSizeInt>(poseBytes.size()));
        if (_pose)
        {
            _updateScheduler.AddUpdatableList(CSM_NEW Csm::CubismPoseUpdater(*_pose));
        }
    }

    const QByteArray physicsBytes = readAsset(QString::fromUtf8(modelSetting_->GetPhysicsFileName()));
    if (!physicsBytes.isEmpty())
    {
        LoadPhysics(
            reinterpret_cast<const Csm::csmByte*>(physicsBytes.constData()),
            static_cast<Csm::csmSizeInt>(physicsBytes.size()));
        if (_physics)
        {
            _updateScheduler.AddUpdatableList(CSM_NEW Csm::CubismPhysicsUpdater(*_physics));
        }
    }

    const QByteArray userDataBytes = readAsset(QString::fromUtf8(modelSetting_->GetUserDataFile()));
    if (!userDataBytes.isEmpty())
    {
        LoadUserData(
            reinterpret_cast<const Csm::csmByte*>(userDataBytes.constData()),
            static_cast<Csm::csmSizeInt>(userDataBytes.size()));
    }

    auto* idManager = Csm::CubismFramework::GetIdManager();
    breathParameterId_ = idManager->GetId(Csm::DefaultParameterId::ParamBreath);
    leftEyeOpenParameterId_ = idManager->GetId(Csm::DefaultParameterId::ParamEyeLOpen);
    rightEyeOpenParameterId_ = idManager->GetId(Csm::DefaultParameterId::ParamEyeROpen);

    _look = Csm::CubismLook::Create();
    Csm::csmVector<Csm::CubismLook::LookParameterData> lookParameters;
    lookParameters.PushBack({idManager->GetId(Csm::DefaultParameterId::ParamAngleX), 30.0F});
    lookParameters.PushBack({idManager->GetId(Csm::DefaultParameterId::ParamAngleY), 0.0F, 30.0F});
    lookParameters.PushBack({idManager->GetId(Csm::DefaultParameterId::ParamAngleZ), 0.0F, 0.0F, -20.0F});
    lookParameters.PushBack({idManager->GetId(Csm::DefaultParameterId::ParamBodyAngleX), 10.0F});
    lookParameters.PushBack({idManager->GetId(Csm::DefaultParameterId::ParamEyeBallX), 1.0F});
    lookParameters.PushBack({idManager->GetId(Csm::DefaultParameterId::ParamEyeBallY), 0.0F, 1.0F});
    _look->SetParameters(lookParameters);
    _updateScheduler.AddUpdatableList(CSM_NEW Csm::CubismLookUpdater(*_look, *_dragManager));

    _updateScheduler.SortUpdatableList();

    Csm::csmMap<Csm::csmString, Csm::csmFloat32> layout;
    modelSetting_->GetLayoutMap(layout);
    _modelMatrix->SetupFromLayout(layout);
    _model->SaveParameters();

    CreateRenderer(
        static_cast<Csm::csmUint32>(std::max(width, 1)),
        static_cast<Csm::csmUint32>(std::max(height, 1)));
    if (!loadTextures())
    {
        return false;
    }

    _updating = false;
    _initialized = true;
    return true;
}

bool PetCubismModel::loadTextures()
{
    auto* renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
    if (!renderer)
    {
        return false;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (Csm::csmInt32 index = 0; index < modelSetting_->GetTextureCount(); ++index)
    {
        const QString relativePath = QString::fromUtf8(modelSetting_->GetTextureFileName(index));
        if (relativePath.isEmpty())
        {
            continue;
        }

        QImage image(QDir(assetDirectory_).filePath(relativePath));
        if (image.isNull())
        {
            qWarning().noquote() << "Live2D texture could not be read:" << relativePath;
            return false;
        }
        image = image.convertToFormat(QImage::Format_RGBA8888);

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            image.width(),
            image.height(),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            image.constBits());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        textureIds_.push_back(texture);
        renderer->BindTexture(index, texture);
    }
    renderer->IsPremultipliedAlpha(false);
    return !textureIds_.empty();
}

void PetCubismModel::update(const float deltaSeconds)
{
    if (!_model || !modelSetting_)
    {
        return;
    }

    const float frameDelta = std::clamp(deltaSeconds, 0.0F, MaximumFrameDelta);

    _model->LoadParameters();
    _updateScheduler.OnLateUpdate(_model, frameDelta);
    updateIdleAnimation(frameDelta);
    _model->Update();
}

void PetCubismModel::updateIdleAnimation(const float deltaSeconds)
{
    idleAnimationTime_ += deltaSeconds;

    const float breath = std::clamp(
        0.5F + 0.5F * std::sin((2.0F * Pi * idleAnimationTime_) / BreathCycleSeconds),
        0.0F,
        1.0F);
    _model->SetParameterValue(breathParameterId_, breath);

    float eyeOpen = 1.0F;
    if (idleAnimationTime_ >= nextBlinkTime_)
    {
        const float elapsed = idleAnimationTime_ - nextBlinkTime_;
        if (elapsed < BlinkClosingSeconds)
        {
            eyeOpen = 1.0F - elapsed / BlinkClosingSeconds;
        }
        else if (elapsed < BlinkClosingSeconds + BlinkClosedSeconds)
        {
            eyeOpen = 0.0F;
        }
        else if (elapsed < BlinkClosingSeconds + BlinkClosedSeconds + BlinkOpeningSeconds)
        {
            eyeOpen = (elapsed - BlinkClosingSeconds - BlinkClosedSeconds)
                / BlinkOpeningSeconds;
        }
        else
        {
            const float variation = 0.5F + 0.5F * std::sin(idleAnimationTime_ * 1.731F);
            nextBlinkTime_ = idleAnimationTime_ + 3.0F + 2.0F * variation;
            eyeOpen = 1.0F;
        }
    }
    eyeOpen = std::clamp(eyeOpen, 0.0F, 1.0F);
    _model->SetParameterValue(leftEyeOpenParameterId_, eyeOpen);
    _model->SetParameterValue(rightEyeOpenParameterId_, eyeOpen);
}

void PetCubismModel::draw(const int width, const int height)
{
    if (!_model || width <= 0 || height <= 0)
    {
        return;
    }

    Csm::CubismMatrix44 projection;
    projection.LoadIdentity();
    if (_model->GetCanvasWidth() > 1.0F && width < height)
    {
        _modelMatrix->SetWidth(2.0F);
        projection.Scale(1.0F, static_cast<float>(width) / static_cast<float>(height));
    }
    else
    {
        projection.Scale(static_cast<float>(height) / static_cast<float>(width), 1.0F);
    }

    projection.MultiplyByMatrix(_modelMatrix);
    auto* renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
    renderer->SetMvpMatrix(&projection);
    renderer->DrawModel();
}

void PetCubismModel::resize(const int width, const int height)
{
    if (_model && width > 0 && height > 0)
    {
        SetRenderTargetSize(
            static_cast<Csm::csmUint32>(width),
            static_cast<Csm::csmUint32>(height));
    }
}

void PetCubismModel::setPointer(const float x, const float y)
{
    SetDragging(
        std::clamp(x, -1.0F, 1.0F),
        std::clamp(y, -1.0F, 1.0F));
}

void PetCubismModel::releaseOwnedResources()
{
    DeleteRenderer();
    if (!textureIds_.empty())
    {
        glDeleteTextures(
            static_cast<GLsizei>(textureIds_.size()),
            reinterpret_cast<const GLuint*>(textureIds_.data()));
        textureIds_.clear();
    }

    delete modelSetting_;
    modelSetting_ = nullptr;
}
