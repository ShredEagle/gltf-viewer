#pragma once


#include <renderer/Texture.h>

#include <optional>


namespace ad {
namespace gltfviewer {


graphics::Texture loadEquirectangularMapHdr(const filesystem::path & aEquirectangular);
graphics::Texture prepareIrradiance(const graphics::Texture & aRadianceEquirect);
graphics::Texture prefilterEnvironment(const graphics::Texture & aRadianceEquirect, bool aLodAntialiasing);
graphics::Texture prepareBrdfLut();


// Note: Inheritance instead of composition.
// This way, instance can implicitly be converted to GLint (texture ids),
// as expected by OpenGL API.
struct EnvironmentTexture : public graphics::Texture
{
    enum class Type
    {
        Equirectangular,
        Cubemap,
    };

    EnvironmentTexture(graphics::Texture aTexture, Type aType) :
        graphics::Texture{std::move(aTexture)},
        mType{aType}
    {}

    Type mType;
};


/// \brief To be generated form a .hdr equirectangular map.
struct Environment
{
    /// \brief Content of the environment map, used for GUI selection.
    enum class Content
    {
        Radiance,
        Irradiance,
        Prefiltered,
        PrefilteredAntialiased,

        // Keep last
        _End//
    };

    explicit Environment(graphics::Texture aEnvironmentRectangular);

    EnvironmentTexture mEnvironmentEquirectangular;
    EnvironmentTexture mIrradianceCubemap;
    EnvironmentTexture mPrefilteredCubemap;
    EnvironmentTexture mPrefilteredAntialiasedCubemap;
    static constexpr math::Size<2, int> gPrefilterSize{128, 128};
    static constexpr GLint gPrefilterLevels{4};
};


std::string to_string(Environment::Content aEnvironment);


struct Ibl
{
    void loadEnvironment(const filesystem::path & aEnvironmentHdr);

    GLfloat mAmbientFactor{1.0f};
    std::optional<Environment> mEnvironment;
    const graphics::Texture mBrdfLut{prepareBrdfLut()};
};

} // namespace gltfviewer
} // namespace ad
