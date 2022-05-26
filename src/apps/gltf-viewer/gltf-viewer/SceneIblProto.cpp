#include "SceneIblProto.h"

#include "ShadersIblProto.h"

#include <graphics/CameraUtilities.h>
#include <graphics/detail/UnitQuad.h>

#include <renderer/Framebuffer.h>
#include <renderer/GL_Loader.h>
#include <renderer/Uniforms.h>

#include <math/Transformations.h>

#include <algorithm>


namespace ad {
namespace gltfviewer {

namespace {

    GLsizei countCompleteMipmaps(math::Size<2, int> aResolution)
    {
        return std::floor(
            std::log2(
                std::max(aResolution.width(), aResolution.height())
        )) + 1;
    }

    constexpr math::Size<2, int> gPrefilterSize{128, 128};
    const GLsizei gPrefilterLevels = countCompleteMipmaps(gPrefilterSize);


    math::Size<2, GLsizei> getMipmapSize(math::Size<2, int> aFullResolution, unsigned int aLevel)
    {
        return {
            std::max(1, aFullResolution.width() / (int)std::pow(2, aLevel)),
            std::max(1, aFullResolution.height() / (int)std::pow(2, aLevel)),
        };
    }

    struct Vertex
    {
        math::Position<3, GLfloat> position;
    };

    constexpr std::initializer_list<graphics::AttributeDescription> gVertexDescription = {
        { 0, 3, offsetof(Vertex, position), graphics::MappedGL<GLfloat>::enumerator},
    };

    const std::array<Vertex, 8> gVertices{
        Vertex{{-1.0f, -1.0f, -1.0f}},
        Vertex{{-1.0f, -1.0f,  1.0f}},
        Vertex{{-1.0f,  1.0f, -1.0f}},
        Vertex{{-1.0f,  1.0f,  1.0f}},

        Vertex{{ 1.0f, -1.0f, -1.0f}},
        Vertex{{ 1.0f, -1.0f,  1.0f}},
        Vertex{{ 1.0f,  1.0f, -1.0f}},
        Vertex{{ 1.0f,  1.0f,  1.0f}},
    };

    const std::array<GLushort, 6*6> gCubeIndices
    {
        // Left
        0, 2, 1,
        1, 2, 3,
        // Back
        1, 3, 5,
        5, 3, 7,
        // Right,
        5, 7, 4,
        4, 7, 6,
        // Front
        4, 6, 0,
        0, 6, 2,
        // Top
        6, 7, 2,
        2, 7, 3,
        // Bottom,
        0, 1, 4,
        4, 1, 5,
    };

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

graphics::Texture loadFolder(const filesystem::path & aFolder, filesystem::path aExtension = ".jpg")
{
    const std::vector<filesystem::path> indexToFilename{
        "posx",
        "negx",
        "posy",
        "negy",
        "posz",
        "negz",
    };

    graphics::Texture cubemap{GL_TEXTURE_CUBE_MAP};

    std::size_t faceId = 0;

    using ImageType = arte::ImageRgb;

    auto loadData = [&cubemap](const ImageType & aImage, std::size_t aFaceId)
    {
        Guard scopedAlignemnt = graphics::detail::scopeUnpackAlignment(aImage.rowAlignment());
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + aFaceId,
                        0, // base mipmap level
                        0, 0, // offset
                        aImage.dimensions().width(), aImage.dimensions().height(),
                        graphics::MappedPixel_v<ImageType::pixel_format_t>,
                        GL_UNSIGNED_BYTE,
                        aImage.data());
    };

    // No operator+()
    // see: https://stackoverflow.com/questions/2396548/appending-to-boostfilesystempath#comment42716010_21144232
    auto filename = indexToFilename[faceId];
    filename += aExtension;
    ImageType image{aFolder / filename};
    allocateStorage(cubemap, GL_RGB8, image.dimensions());

    // Cannot bind earlier, allocate storage scopes the bind...
    graphics::bind_guard boundCubemap{cubemap};

    loadData(image, faceId);

    for(++faceId; faceId != indexToFilename.size(); ++faceId)
    {
        filename = indexToFilename[faceId];
        filename += aExtension;
        image = ImageType{aFolder / filename};
        loadData(image, faceId);
    }

    // No mipmap atm though
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return cubemap;
}


graphics::Texture loadHdr(const filesystem::path & aEquirectangular)
{
    graphics::Texture equirect{GL_TEXTURE_2D};

    std::size_t faceId = 0;

    using ImageType = arte::Image<math::hdr::Rgb<GLfloat>>;
    ImageType image{aEquirectangular, arte::ImageOrientation::InvertVerticalAxis};

    allocateStorage(equirect, GL_RGB16F, image.dimensions());

    // Cannot bind earlier, allocate storage scopes the bind...
    graphics::bind_guard boundTexture{equirect};

    Guard scopedAlignemnt = graphics::detail::scopeUnpackAlignment(image.rowAlignment());
    glTexSubImage2D(GL_TEXTURE_2D,
                    0, // base mipmap level
                    0, 0, // offset
                    image.dimensions().width(), image.dimensions().height(),
                    graphics::MappedPixel_v<ImageType::pixel_format_t>,
                    GL_FLOAT,
                    image.data());

    // No mipmap atm though
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return equirect;
}


graphics::Texture loadCubemap(const filesystem::path & aPath)
{
    if(aPath.extension() == ".hdr")
    {
        return loadHdr(aPath);
    }
    else
    {
        return loadFolder(aPath);
    }
}


graphics::Texture prepareIrradiance(const graphics::Texture & aRadianceEquirect, const Cube & aCube)
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
        aCube.draw();
    }

    return cubemap;
}


graphics::Texture prefilterEnvironment(const graphics::Texture & aRadianceEquirect, const Cube & aCube)
{
    constexpr GLint gTextureUnit = 0;
    assert(gPrefilterLevels > 1);

    graphics::FrameBuffer framebuffer;
    bind(framebuffer);

    graphics::Texture cubemap{GL_TEXTURE_CUBE_MAP};
    allocateStorage(cubemap, GL_RGB16F, gPrefilterSize, gPrefilterLevels);

    bind(cubemap);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    auto scopedViewport = 
        graphics::scopeViewport({{0, 0}, {gPrefilterSize.width(), gPrefilterSize.height()}});

    auto convolution = graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gPrefilterFragmentShader.c_str()},
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

    for(int mip = 0; mip != gPrefilterLevels; ++mip)
    {
        math::Size<2, GLsizei> levelSize = getMipmapSize(gPrefilterSize, mip);
        glViewport(0, 0, levelSize.width(), levelSize.height());

        GLfloat roughness = (GLfloat)mip / (gPrefilterLevels - 1);
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
            aCube.draw();
        }
    }
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

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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

} // anonymous namespace


Cube::Cube() :
    mCubeVertices{loadVertexBuffer<const Vertex>(
        mVao,
        gVertexDescription,
        gVertices)
    },
    mCubeIndices{loadIndexBuffer<const GLushort>(
        mVao, gCubeIndices, graphics::BufferHint::StaticRead)
    }
{}


void Cube::draw() const
{
    graphics::bind_guard boundVao{mVao};
    graphics::bind_guard boundIbo{mCubeIndices};
    glDrawElements(GL_TRIANGLES, gCubeIndices.size(), GL_UNSIGNED_SHORT, nullptr);
}


Quad::Quad() :
    mVertexSpecification{graphics::detail::make_UnitQuad()}
{}


void Quad::draw() const
{
    graphics::bind_guard boundVao{mVertexSpecification};
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


IblRenderer::IblRenderer(const filesystem::path & aEnvironmentMap) :
    mCubemapProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gIblCubemapFragmentShader},
    })},
    mEquirectangularProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gIblEquirectangularFragmentShader},
    })},
    mModelProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gIblPbrFragmentShader.c_str()},
    })},
    mTexture2DProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gQuadVertexShader},
        {GL_FRAGMENT_SHADER, gTexture2DFragmentShader},
    })},
    mCubemap{loadCubemap(aEnvironmentMap)},
    mIrradianceCubemap{prepareIrradiance(mCubemap, mCube)},
    mPrefilteredCubemap{prefilterEnvironment(mCubemap, mCube)},
    mBrdfLut{prepareBrdfLut()}
{
    setUniformInt(mCubemapProgram, "u_cubemap", gCubemapTextureUnit);
    setUniformInt(mEquirectangularProgram, "u_equirectangularMap", gCubemapTextureUnit);

    setUniform(mModelProgram, "u_baseColorFactor", math::Vec<4, GLfloat>{1.f, 1.f, 1.f, 1.f});
    setUniformInt(mModelProgram, "u_irrandianceMap", gCubemapTextureUnit);

    setUniformInt(mTexture2DProgram, "u_texture", gCubemapTextureUnit);
}


void IblRenderer::render() const
{
    glActiveTexture(GL_TEXTURE0 + gCubemapTextureUnit);
   
    if (mShowBrdfLut)
    {
        static const Quad gQuad;
        bind(mBrdfLut);
        bind(mTexture2DProgram);
        gQuad.draw();
    }
    else
    {
        // Render skybox
        {
            switch(mEnvMap)
            {
            case Environment::Radiance:
                bind(mCubemap);
                bind(mEquirectangularProgram);
                break;
            case Environment::Irradiance:
                bind(mIrradianceCubemap);
                bind(mCubemapProgram);
                break;
            case Environment::Prefiltered:
                bind(mPrefilteredCubemap);
                bind(mCubemapProgram);
                break;
            }

            auto depthMaskGuard = graphics::scopeDepthMask(false);
            mCube.draw();
        }

        // Render model
        if(mShowObject)
        {
            graphics::bind_guard boundCubemap{mIrradianceCubemap};
            graphics::bind_guard boundProgram{mModelProgram};

            mSphere.draw();
        }
    }
}


void IblRenderer::update()
{
    setUniformInt(mCubemapProgram, "u_explicitLod", mPrefilteredLod);

    setUniformFloat(mModelProgram, "u_metallicFactor", mMetallic);
    setUniformFloat(mModelProgram, "u_roughnessFactor", mRoughness);
    setUniformFloat(mModelProgram, "u_ambientFactor", mAmbientFactor);
    setUniformInt(mModelProgram, "u_debugOutput", static_cast<int>(mColorOutput)); 
}


void SceneIblProto::render() const
{
    glEnable(GL_DEPTH_TEST);  
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);  

    mRenderer.render(); 
}


void SceneIblProto::setView()
{
    math::AffineMatrix<4, GLfloat> viewOrientation{mCameraSystem.getViewTransform().getLinear()};
    setUniform(mRenderer.mCubemapProgram, "u_camera", viewOrientation); 
    setUniform(mRenderer.mEquirectangularProgram, "u_camera", viewOrientation); 
    setUniform(mRenderer.mModelProgram, "u_camera", mCameraSystem.getViewTransform()); 
}


void SceneIblProto::setProjection()
{
    setUniform(mRenderer.mCubemapProgram, "u_projection", mCameraSystem.getCubemapProjectionTransform(mAppInterface)); 
    setUniform(mRenderer.mEquirectangularProgram, "u_projection", mCameraSystem.getCubemapProjectionTransform(mAppInterface)); 
    setUniform(mRenderer.mModelProgram, "u_projection", mCameraSystem.getProjectionTransform(mAppInterface)); 
}


void SceneIblProto::showSceneControls()
{
    ImGui::Begin("Scene options");
    mCameraSystem.appendCameraControls();
    ImGui::End();

    mRenderer.showRendererOptions();
}


void IblRenderer::showRendererOptions()
{
    ImGui::Begin("Rendering options");
    {
        ImGui::Checkbox("Show BRDF LUT", &mShowBrdfLut);

        if (ImGui::BeginCombo("Environment map", to_string(mEnvMap).c_str()))
        {
            for (int id = 0; id < static_cast<int>(Environment::_End); ++id)
            {
                Environment entry = static_cast<Environment>(id);
                const bool isSelected = (mEnvMap == entry);
                if (ImGui::Selectable(to_string(entry).c_str(), isSelected))
                {
                    mEnvMap = entry;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if(mEnvMap == Environment::Prefiltered)
        {
            if(mPrefilteredLod == -1)
            {
                mPrefilteredLod = 0;
            }
            ImGui::SliderInt("Prefiltered level", &mPrefilteredLod, 0, gPrefilterLevels - 1);
        }
        else
        {
            mPrefilteredLod = -1;
        }

        ImGui::Checkbox("Show object", &mShowObject);

        if (ImGui::BeginCombo("Color output", to_string(mColorOutput).c_str()))
        {
            for (int colorId = 0; colorId < static_cast<int>(DebugColor::_End); ++colorId)
            {
                DebugColor output = static_cast<DebugColor>(colorId);
                const bool isSelected = (mColorOutput == output);
                if (ImGui::Selectable(to_string(output).c_str(), isSelected))
                {
                    mColorOutput = output;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SliderFloat("Metallic", &mMetallic, 0.f, 1.0f, "%.3f");
        ImGui::SliderFloat("Roughness", &mRoughness, 0.f, 1.0f, "%.3f");
        ImGui::SliderFloat("Ambient factor", &mAmbientFactor, 0.f, 1.0f, "%.3f");

    }
    ImGui::End();
}

} // namespace gltfviewer
} // namespace ad
