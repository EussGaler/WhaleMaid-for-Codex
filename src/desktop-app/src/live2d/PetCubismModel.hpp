#pragma once

#include <Model/CubismUserModel.hpp>
#include <Id/CubismId.hpp>

#include <QByteArray>
#include <QString>

#include <vector>

namespace Live2D::Cubism::Framework
{
class CubismModelSettingJson;
}

class PetCubismModel final : public Csm::CubismUserModel
{
public:
    PetCubismModel();
    ~PetCubismModel() override;

    bool load(const QString& assetDirectory, const QString& modelName, int width, int height);
    void update(float deltaSeconds);
    void draw(int width, int height);
    void resize(int width, int height);
    void setPointer(float x, float y);

private:
    QByteArray readAsset(const QString& relativePath) const;
    bool loadTextures();
    void updateIdleAnimation(float deltaSeconds);
    void releaseOwnedResources();

    QString assetDirectory_;
    QString modelName_;
    Csm::CubismModelSettingJson* modelSetting_ = nullptr;
    std::vector<unsigned int> textureIds_;
    Csm::CubismIdHandle breathParameterId_ = nullptr;
    Csm::CubismIdHandle leftEyeOpenParameterId_ = nullptr;
    Csm::CubismIdHandle rightEyeOpenParameterId_ = nullptr;
    float idleAnimationTime_ = 0.0F;
    float nextBlinkTime_ = 1.0F;
};
