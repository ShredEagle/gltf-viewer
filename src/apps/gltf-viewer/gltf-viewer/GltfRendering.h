#pragma once


#include "Ibl.h"
#include "Mesh.h"
#include "SkeletalAnimation.h"

#include <arte/gltf/Gltf.h>

#include <renderer/Shading.h>


namespace ad {
namespace gltfviewer {


math::AffineMatrix<4, float> getLocalTransform(const arte::gltf::Node::TRS & aTRS);
math::AffineMatrix<4, float> getLocalTransform(const arte::gltf::Node & aNode);


enum class GpuProgram
{
    InstancedNoAnimation,
    Skinning,
};


enum class ShadingModel
{
    Phong,
    PbrReference,
    PbrLearn,
    PbrLearnIbl,

    // Keep last
    _End,
};

inline std::string to_string(ShadingModel aShading)
{
    switch(aShading)
    {
    case ShadingModel::Phong:
        return "Phong";
    case ShadingModel::PbrReference:
        return "PbrReference";
    case ShadingModel::PbrLearn:
        return "PbrLearn";
    case ShadingModel::PbrLearnIbl:
        return "PbrLearnIbl";
    }
}

enum class DebugColor
{
    Default = 0,
    Metallic,
    Roughness,
    Albedo,
    Occlusion,
    FresnelLight,
    FresnelIbl,
    Normal,
    View,
    Halfway,
    NdotL,
    VdotH,
    NormalDistributionFunction,
    GeometryFunction,
    DiffuseLight,
    SpecularLight,
    DiffuseIbl,
    SpecularIbl,
    Direct,
    Ambient,

    // Keep last
    _End,
};


std::string to_string(DebugColor aColor);


struct Light
{
    math::hdr::Rgb<GLfloat> mColor{1.f, 1.f, 1.f};
    math::Vec<3, GLfloat> mDirection{0.f, 0.f, -1.f};
};


class SkyboxRenderer
{
public:
    SkyboxRenderer();

    void render(const EnvironmentTexture & aSkybox) const;

    void setCameraTransformation(const math::AffineMatrix<4, GLfloat> & aTransformation);
    void setProjectionTransformation(const math::Matrix<4, 4, GLfloat> & aTransformation);

    void setPrefilterLod(GLint aLodLevel);

private:
    graphics::Program mEquirectangularProgram;
    graphics::Program mCubemapProgram;
};


class Renderer
{
    using ShadingPrograms = std::map<GpuProgram, std::shared_ptr<graphics::Program>>;

public:
    Renderer();

    void setCameraTransformation(const math::AffineMatrix<4, GLfloat> & aTransformation);
    void setProjectionTransformation(const math::Matrix<4, 4, GLfloat> & aTransformation);

    void setLight(const Light & aLight);

    void loadEnvironment(const filesystem::path & aEnvironmentMap)
    { mIbl.loadEnvironment(aEnvironmentMap); }

    void togglePolygonMode();

    void bind(const Skeleton & aSkeleton) const;

    void render(const Mesh & aMesh) const;
    void render(const Mesh & aMesh, const Skeleton & aSkeleton) const;

    void showRendererOptions();

    static constexpr GLsizei gColorTextureUnit{0};
    static constexpr GLsizei gMetallicRoughnessTextureUnit{1};
    static constexpr GLsizei gOcclusionTextureUnit{3};
    static constexpr GLuint gPaletteBlockBinding{3};

    static constexpr GLsizei gIrradianceMapTextureUnit{4};
    static constexpr GLsizei gPrefilterMapTextureUnit{5};
    static constexpr GLsizei gBrdfLutTextureUnit{6};

private:
    enum class PolygonMode
    {
        Fill,
        Line,
    };

    void initializePrograms();
    template <class ... VT_extraParams>
    void renderImpl(const Mesh & aMesh, graphics::Program & aProgram, VT_extraParams ... aExtraParams) const;
    const ShadingPrograms & activePrograms() const;
    void renderSkybox() const;

    std::map<ShadingModel, ShadingPrograms> mPrograms;
    ShadingModel mShadingModel{ShadingModel::PbrReference};
    Ibl mIbl;
    SkyboxRenderer mSkyboxRenderer;
    PolygonMode mPolygonMode{PolygonMode::Fill};
    DebugColor mColorOutput{DebugColor::Default};
    Environment::Content mShownSkybox{Environment::Content::Radiance};
    GLint mPrefilteredLod{0};
    bool mEnableOcclusionTexture{true};
};

} // namespace gltfviewer
} // namespace ad
