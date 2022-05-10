#pragma once


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
    Pbr,

    // Keep last
    _End,
};

inline std::string to_string(ShadingModel aShading)
{
    switch(aShading)
    {
    case ShadingModel::Phong:
        return "Phong";
    case ShadingModel::Pbr:
        return "Pbr";
    }
}


class Renderer
{
    using ShadingPrograms = std::map<GpuProgram, std::shared_ptr<graphics::Program>>;

public:
    Renderer();

    void setCameraTransformation(const math::AffineMatrix<4, GLfloat> & aTransformation);
    void setProjectionTransformation(const math::Matrix<4, 4, GLfloat> & aTransformation);

    void togglePolygonMode();

    void bind(const Skeleton & aSkeleton) const;

    void render(const Mesh & aMesh) const;
    void render(const Mesh & aMesh, const Skeleton & aSkeleton) const;

    void showRendererOptions();

    static constexpr GLsizei gColorTextureUnit{0};
    static constexpr GLsizei gMetallicRoughnessTextureUnit{1};
    static constexpr GLuint gPaletteBlockBinding{3};

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

    std::map<ShadingModel, ShadingPrograms> mPrograms;
    ShadingModel mShadingModel{ShadingModel::Pbr};
    PolygonMode mPolygonMode{PolygonMode::Fill};
};

} // namespace gltfviewer
} // namespace ad
