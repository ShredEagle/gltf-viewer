#pragma once


#include <renderer/Texture.h>

#include <optional>


namespace ad {
namespace gltfviewer {


graphics::Texture loadEquirectangularMapHdr(const filesystem::path & aEquirectangular);
graphics::Texture prepareIrradiance(const graphics::Texture & aRadianceEquirect);
graphics::Texture prefilterEnvironment(const graphics::Texture & aRadianceEquirect);
graphics::Texture prepareBrdfLut();


/// \brief To be generated form a .hdr equirectangular map.
struct Environment
{
    explicit Environment(graphics::Texture aEnvironmentRectangular);

    graphics::Texture mEnvironmentEquirectangular;
    graphics::Texture mIrradianceCubemap;
    graphics::Texture mPrefilteredCubemap;
    static constexpr math::Size<2, int> gPrefilterSize{128, 128};
    static constexpr GLint gPrefilterLevels{4};
};


struct Ibl
{
    void loadEnvironment(const filesystem::path & aEnvironmentHdr);

    GLfloat mAmbientFactor{1.0f};
    std::optional<Environment> mEnvironment;
    const graphics::Texture mBrdfLut{prepareBrdfLut()};
};

} // namespace gltfviewer
} // namespace ad
