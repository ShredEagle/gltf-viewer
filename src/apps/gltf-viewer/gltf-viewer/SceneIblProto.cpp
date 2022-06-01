#include "SceneIblProto.h"

#include "Ibl.h"
#include "Shaders.h" // For gQuadVertexShader
#include "ShadersIblProto.h"
#include "ShadersSkybox.h"

#include <renderer/Framebuffer.h>

#include <renderer/GL_Loader.h>
#include <renderer/Uniforms.h>

#include <nfd.h>

#include <algorithm>


namespace ad {
namespace gltfviewer {

namespace {


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


    graphics::Texture loadCubemap(const filesystem::path & aPath)
    {
        if(aPath.extension() == ".hdr")
        {
            return loadEquirectangularMapHdr(aPath);
        }
        else
        {
            return loadFolder(aPath);
        }
    }


} // anonymous namespace


IblRenderer::IblRenderer(const filesystem::path & aEnvironmentMap) :
    mCubemapProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader}, // todo use skybox
        {GL_FRAGMENT_SHADER, gIblCubemapFragmentShader},
    })},
    mEquirectangularProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader}, // todo use skybox
        {GL_FRAGMENT_SHADER, gEquirectangularSkyboxFragmentShader},
    })},
    mModelProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gIblProtoPbrFragmentShader.c_str()},
    })},
    mTexture2DProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gQuadVertexShader},
        {GL_FRAGMENT_SHADER, gTexture2DFragmentShader},
    })},
    mCubemap{loadCubemap(aEnvironmentMap)},
    mIrradianceCubemap{prepareIrradiance(mCubemap)},
    mPrefilteredCubemap{prefilterEnvironment(mCubemap)},
    mBrdfLut{prepareBrdfLut()}
{
    setUniformInt(mCubemapProgram, "u_cubemap", gCubemapTextureUnit);
    setUniformInt(mEquirectangularProgram, "u_equirectangularMap", gCubemapTextureUnit);

    setUniform(mModelProgram, "u_baseColorFactor", math::Vec<4, GLfloat>{1.f, 1.f, 1.f, 1.f});
    setUniformFloat(mModelProgram, "u_maxReflectionLod", gltfviewer::Environment::gPrefilterLevels);
    setUniformInt(mModelProgram, "u_irradianceMap", gCubemapTextureUnit);
    setUniformInt(mModelProgram, "u_prefilterMap", gPrefilterMapTextureUnit);
    setUniformInt(mModelProgram, "u_brdfLut", gBrdfLutTextureUnit);

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
            glActiveTexture(GL_TEXTURE0 + gCubemapTextureUnit);
            graphics::bind_guard boundCubemap{mIrradianceCubemap};
            glActiveTexture(GL_TEXTURE0 + gPrefilterMapTextureUnit);
            bind(mPrefilteredCubemap);
            glActiveTexture(GL_TEXTURE0 + gBrdfLutTextureUnit);
            bind(mBrdfLut);

            graphics::bind_guard boundProgram{mModelProgram};

            mSphere.draw();
        }
    }
}


void IblRenderer::update()
{
    setUniformInt(mCubemapProgram, "u_explicitLod", mPrefilteredLod);

    setUniform(mModelProgram, "u_lightColor", mLightColor);

    setUniformFloat(mModelProgram, "u_metallicFactor", mMetallic);
    setUniformFloat(mModelProgram, "u_roughnessFactor", mRoughness);
    setUniformFloat(mModelProgram, "u_ambientFactor", mAmbientFactor);

    setUniformInt(mModelProgram, "u_hdrTonemapping", static_cast<int>(mHdrTonemapping)); 
    setUniformInt(mModelProgram, "u_gammaCorrect", static_cast<int>(mGammaCorrect)); 
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
    setUniform(mRenderer.mModelProgram, "u_cameraPosition_world", mCameraSystem.getWorldPosition()); 
}


void SceneIblProto::setProjection()
{
    setUniform(mRenderer.mCubemapProgram, "u_projection", mCameraSystem.getCubemapProjectionTransform(mAppInterface)); 
    setUniform(mRenderer.mEquirectangularProgram, "u_projection", mCameraSystem.getCubemapProjectionTransform(mAppInterface)); 
    setUniform(mRenderer.mModelProgram, "u_projection", mCameraSystem.getProjectionTransform(mAppInterface)); 
}


void SceneIblProto::showSceneControls()
{
    mCameraSystem.showCameraControls();
    mRenderer.showRendererOptions();
}


void IblRenderer::showRendererOptions()
{
    ImGui::Begin("Rendering options");
    {
        ImGui::ColorPicker3("Light color", mLightColor.data());

        ImGui::Checkbox("Show BRDF LUT", &mShowBrdfLut);

        if(ImGui::Button("Open HDR environment"))
        {
            nfdchar_t *outPath = NULL;
            nfdresult_t result = NFD_OpenDialog("hdr", NULL, &outPath );
                
            if ( result == NFD_OKAY ) 
            {
                mCubemap = loadCubemap(outPath);
                mIrradianceCubemap = prepareIrradiance(mCubemap);
                mPrefilteredCubemap = prefilterEnvironment(mCubemap);

                free(outPath);
            }
            else if ( result == NFD_CANCEL ) 
            {
                ADLOG(gPrepareLogger, trace)("User cancelled selection.");
            }
            else 
            {
                ADLOG(gPrepareLogger, error)("File selection error: {}.", NFD_GetError());
            }
        }

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
            ImGui::SliderInt("Prefiltered level", &mPrefilteredLod, 0, gltfviewer::Environment::gPrefilterLevels - 1);
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

        ImGui::Checkbox("HDR Tonemapping", &mHdrTonemapping);
        ImGui::Checkbox("Gamma Correct", &mGammaCorrect);

        ImGui::SliderFloat("Metallic", &mMetallic, 0.f, 1.0f, "%.3f");
        ImGui::SliderFloat("Roughness", &mRoughness, 0.f, 1.0f, "%.3f");
        ImGui::SliderFloat("Ambient factor", &mAmbientFactor, 0.f, 1.0f, "%.3f");

    }
    ImGui::End();
}

} // namespace gltfviewer
} // namespace ad
