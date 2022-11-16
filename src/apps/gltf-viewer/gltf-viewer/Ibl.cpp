#include "Ibl.h"

#include "CubeQuad.h"
#include "Shaders.h"
#include "ShadersIblProto.h"

#include <graphics/CameraUtilities.h>

#include <math/Transformations.h>

#include <renderer/FrameBuffer.h>
#include <renderer/Shading.h>
#include <renderer/Uniforms.h>


namespace ad {
namespace gltfviewer {


namespace {


    using graphics::getCameraTransform;
    // TODO had to invert the up direction on the some view matrices, not sure why yet. 
    // (might be related to the cubemap coordinate system being left-handed)
    const std::array<math::AffineMatrix<4, GLfloat>, 6> gCaptureView{
        getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, { 1.f, 0.f, 0.f}, {0.f, -1.f, 0.f}),
        getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, -1.f, 0.f}),
        getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, {0.f,  1.f, 0.f}, {0.f, 0.f,  1.f}),
        getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, -1.f}),
        getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, {0.f, 0.f,  1.f}, {0.f, -1.f, 0.f}),
        getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, -1.f, 0.f}),
    };


} // namespace anonymous


std::string to_string(Environment::Content aEnvironment)
{
    switch(aEnvironment)
    {
    case Environment::Content::Radiance:
        return "Radiance";
    case Environment::Content::Irradiance:
        return "Irradiance";
    case Environment::Content::Prefiltered:
        return "Prefiltered";
    case Environment::Content::PrefilteredAntialiased:
        return "PrefilteredAntialiased";
    }
}


graphics::Texture loadEquirectangularMapHdr(const filesystem::path & aEquirectangular)
{
    graphics::Texture equirect{GL_TEXTURE_2D};

    std::size_t faceId = 0;

    using ImageType = arte::Image<math::hdr::Rgb<GLfloat>>;
    ImageType image{aEquirectangular, arte::ImageOrientation::InvertVerticalAxis};

    allocateStorage(equirect, GL_RGB16F, image.dimensions(),
                    graphics::countCompleteMipmaps(image.dimensions()));

    // Cannot bind earlier, allocate storage scopes the bind...
    graphics::ScopedBind boundTexture{equirect};

    Guard scopedAlignemnt = graphics::detail::scopeUnpackAlignment(image.rowAlignment());
    glTexSubImage2D(GL_TEXTURE_2D,
                    0, // base mipmap level
                    0, 0, // offset
                    image.dimensions().width(), image.dimensions().height(),
                    graphics::MappedPixel_v<ImageType::pixel_format_t>,
                    GL_FLOAT,
                    image.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // TODO Rendering the environment map with linear mipmap filtering ends up with dotted lines in some envs.
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenerateMipmap(GL_TEXTURE_2D);

    return equirect;
}


graphics::Texture prepareIrradiance(const graphics::Texture & aRadianceEquirect)
{
    constexpr math::Size<2, int> gIrradianceSize{32, 32};
    constexpr GLint gTextureUnit = 0;

    graphics::FrameBuffer framebuffer;
    bind(framebuffer);

    graphics::Texture cubemap{GL_TEXTURE_CUBE_MAP};
    allocateStorage(cubemap, GL_RGB16F, gIrradianceSize);

    bind(cubemap);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    auto scopedViewport = 
        graphics::scopeViewport({{0, 0}, {gIrradianceSize.width(), gIrradianceSize.height()}});

    auto convolution = graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gIrradianceFragmentShader},
    });
    bind(convolution);

    glActiveTexture(GL_TEXTURE0 + gTextureUnit);
    bind(aRadianceEquirect);
    setUniformInt(convolution, "u_equirectangularMap", gTextureUnit);

    setUniform(convolution, "u_projection",
        math::trans3d::scale(1.f, 1.f, -1.f) // OpenGL clipping space is left handed.
        * math::trans3d::orthographicProjection(math::Box<GLfloat>{
            {-1.f, -1.f, 0.f}, 
            {2.f, 2.f, 2.f}
        })
    ); 

    static const Cube gCube;
    for(GLint face = 0; face != 6; ++face)
    {
        glFramebufferTexture2D(
            GL_DRAW_FRAMEBUFFER, 
            GL_COLOR_ATTACHMENT0, 
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            cubemap,
            0);

        glClear(GL_COLOR_BUFFER_BIT);

        setUniform(convolution, "u_camera", gCaptureView[face]); 
        gCube.draw();
    }

    return cubemap;
}


graphics::Texture prefilterEnvironment(const graphics::Texture & aRadianceEquirect, bool aLodAntialiasing)
{
    constexpr GLint gTextureUnit = 0;
    assert(Environment::gPrefilterLevels > 1);

    graphics::FrameBuffer framebuffer;
    bind(framebuffer);

    graphics::Texture cubemap{GL_TEXTURE_CUBE_MAP};
    allocateStorage(cubemap, GL_RGB16F, Environment::gPrefilterSize, Environment::gPrefilterLevels);

    bind(cubemap);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    auto scopedViewport = 
        graphics::scopeViewport({{0, 0}, {Environment::gPrefilterSize.width(), Environment::gPrefilterSize.height()}});

    auto convolution = graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, aLodAntialiasing ? 
                             gAntialiasPrefilterFragmentShader.c_str() : gPrefilterFragmentShader.c_str()},
    });
    bind(convolution);

    glActiveTexture(GL_TEXTURE0 + gTextureUnit);
    bind(aRadianceEquirect);

    GLint previousMinFilter;
    glGetTexParameteriv(aRadianceEquirect.mTarget, GL_TEXTURE_MIN_FILTER, &previousMinFilter);

    // Mipmap linear filtering must be enabled for lod based sampling in prefilter shader
    if(aLodAntialiasing)
    {
        glTexParameteri(aRadianceEquirect.mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    setUniformInt(convolution, "u_equirectangularMap", gTextureUnit);

    setUniform(convolution, "u_projection",
        math::trans3d::scale(1.f, 1.f, -1.f) // OpenGL clipping space is left handed.
        * math::trans3d::orthographicProjection(math::Box<GLfloat>{
            {-1.f, -1.f, 0.f}, 
            {2.f, 2.f, 2.f}
        })
    ); 

    static const Cube gCube;
    for(int mip = 0; mip != Environment::gPrefilterLevels; ++mip)
    {
        math::Size<2, GLsizei> levelSize = graphics::getMipmapSize(Environment::gPrefilterSize, mip);
        glViewport(0, 0, levelSize.width(), levelSize.height());

        GLfloat roughness = (GLfloat)mip / (Environment::gPrefilterLevels - 1);
        setUniformFloat(convolution, "u_roughness", roughness);
        for(GLint face = 0; face != 6; ++face)
        {
            glFramebufferTexture2D(
                GL_DRAW_FRAMEBUFFER, 
                GL_COLOR_ATTACHMENT0, 
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                cubemap,
                mip);

            glClear(GL_COLOR_BUFFER_BIT);

            setUniform(convolution, "u_camera", gCaptureView[face]); 
            gCube.draw();
        }
    }

    glTexParameteri(aRadianceEquirect.mTarget, GL_TEXTURE_MIN_FILTER, previousMinFilter);

    return cubemap;
}


graphics::Texture prepareBrdfLut()
{
    constexpr math::Size<2, int> gSize{512, 512};

    graphics::FrameBuffer framebuffer;
    bind(framebuffer);

    graphics::Texture lut{GL_TEXTURE_2D};
    allocateStorage(lut, GL_RG16F, gSize);

    bind(lut);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    auto scopedViewport = 
        graphics::scopeViewport({{0, 0}, {gSize.width(), gSize.height()}});

    auto program = graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gQuadVertexShader},
        {GL_FRAGMENT_SHADER, gBrdfLutFragmentShader.c_str()},
    });
    bind(program);

    glFramebufferTexture2D(
        GL_DRAW_FRAMEBUFFER, 
        GL_COLOR_ATTACHMENT0, 
        GL_TEXTURE_2D,
        lut,
        0);

    glClear(GL_COLOR_BUFFER_BIT);

    static const Quad gQuad;
    gQuad.draw();

    return lut;
}


Environment::Environment(graphics::Texture aEnvironmentRectangular) :
    mEnvironmentEquirectangular{std::move(aEnvironmentRectangular), EnvironmentTexture::Type::Equirectangular},
    mIrradianceCubemap{prepareIrradiance(mEnvironmentEquirectangular), EnvironmentTexture::Type::Cubemap},
    mPrefilteredCubemap{prefilterEnvironment(mEnvironmentEquirectangular, false), EnvironmentTexture::Type::Cubemap},
    mPrefilteredAntialiasedCubemap{prefilterEnvironment(mEnvironmentEquirectangular, true), EnvironmentTexture::Type::Cubemap}
{}


void Ibl::loadEnvironment(const filesystem::path & aEnvironmentHdr)
{
    mEnvironment = Environment{loadEquirectangularMapHdr(aEnvironmentHdr)};
}


} // namespace gltfviewer
} // namespace ad
