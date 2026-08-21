#include <GL/glew.h>

#include "live2d/Live2DScene.hpp"

#include "live2d/CubismRuntime.hpp"
#include "live2d/PetCubismModel.hpp"

#include <Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.hpp>

#include <algorithm>

Live2DScene::Live2DScene() = default;

Live2DScene::~Live2DScene()
{
    model_.reset();
    if (runtimeAcquired_)
    {
        Csm::Rendering::CubismOffscreenManager_OpenGLES2::ReleaseInstance();
        CubismRuntime::release();
    }
}

bool Live2DScene::initialize(
    const QString& assetDirectory,
    const QString& modelName,
    const int width,
    const int height)
{
    runtimeAcquired_ = CubismRuntime::acquire();
    if (!runtimeAcquired_)
    {
        return false;
    }

    model_ = std::make_unique<PetCubismModel>();
    if (!model_->load(assetDirectory, modelName, width, height))
    {
        model_.reset();
        CubismRuntime::release();
        runtimeAcquired_ = false;
        return false;
    }

    frameTimer_.start();
    return true;
}

void Live2DScene::render(const int width, const int height)
{
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (!model_ || width <= 0 || height <= 0)
    {
        return;
    }

    const qint64 elapsedNanoseconds = frameTimer_.nsecsElapsed();
    frameTimer_.restart();
    const float deltaSeconds = static_cast<float>(elapsedNanoseconds) / 1'000'000'000.0F;

    auto* offscreenManager = Csm::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance();
    offscreenManager->BeginFrameProcess();
    model_->update(deltaSeconds);
    model_->draw(width, height);
    offscreenManager->EndFrameProcess();
    offscreenManager->ReleaseStaleRenderTextures();
}

void Live2DScene::resize(const int width, const int height)
{
    if (model_)
    {
        model_->resize(width, height);
    }
}

void Live2DScene::setPointer(const float x, const float y)
{
    if (model_)
    {
        model_->setPointer(x, y);
    }
}
