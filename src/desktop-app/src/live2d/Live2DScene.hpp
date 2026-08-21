#pragma once

#include <QElapsedTimer>
#include <QString>

#include <memory>

class PetCubismModel;

class Live2DScene final
{
public:
    Live2DScene();
    ~Live2DScene();

    bool initialize(
        const QString& assetDirectory,
        const QString& modelName,
        int width,
        int height);
    void render(int width, int height);
    void resize(int width, int height);
    void setPointer(float x, float y);

private:
    std::unique_ptr<PetCubismModel> model_;
    QElapsedTimer frameTimer_;
    bool runtimeAcquired_ = false;
};
