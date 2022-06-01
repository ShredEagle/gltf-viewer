#include "GltfRendering.h"

#include "CubeQuad.h"
#include "ImguiUi.h"
#include "Logging.h"
#include "Shaders.h"
#include "ShadersPbr.h"
#include "ShadersPbr_learnopengl.h"
#include "ShadersPbrIbl_learnopengl.h"
#include "ShadersSkybox.h"

#include <renderer/Uniforms.h>

#include <math/Transformations.h>

#include <renderer/GL_Loader.h>


namespace ad {

using namespace arte;

namespace gltfviewer {


inline std::string to_string(DebugColor aColor)
{
    switch(aColor)
    {
    case DebugColor::Default:
        return "Default";
    case DebugColor::Metallic:
        return "Metallic";
    case DebugColor::Roughness:
        return "Roughness";
    case DebugColor::Albedo:
        return "Albedo";
    case DebugColor::FresnelLight:
        return "FresnelLight";
    case DebugColor::FresnelIbl:
        return "FresnelIbl";
    case DebugColor::Normal:
        return "Normal";
    case DebugColor::View:
        return "View";
    case DebugColor::Halfway:
        return "Halfway";
    case DebugColor::NdotL:
        return "NdotL";
    case DebugColor::VdotH:
        return "VdotH";
    case DebugColor::NormalDistributionFunction:
        return "NormalDistributionFunction";
    case DebugColor::GeometryFunction:
        return "GeometryFunction";
    case DebugColor::DiffuseLight:
        return "DiffuseLight";
    case DebugColor::SpecularLight:
        return "SpecularLight";
    case DebugColor::DiffuseIbl:
        return "DiffuseIbl";
    case DebugColor::SpecularIbl:
        return "SpecularIbl";
    case DebugColor::Direct:
        return "Direct";
    case DebugColor::Ambient:
        return "Ambient";
    }
}


math::AffineMatrix<4, float> getLocalTransform(const arte::gltf::Node::TRS & aTRS)
{
    return 
        math::trans3d::scale(aTRS.scale.as<math::Size>())
        * aTRS.rotation.toRotationMatrix()
        * math::trans3d::translate(aTRS.translation)
        ;
}


math::AffineMatrix<4, float> getLocalTransform(const arte::gltf::Node & aNode)
{
    if(auto matrix = std::get_if<math::AffineMatrix<4, float>>(&aNode.transformation))
    {
        return *matrix;
    }
    else
    {
        return getLocalTransform(std::get<gltf::Node::TRS>(aNode.transformation));
    }
}


/// \brief A single (non-instanted) draw of the mesh primitive.
void drawCall(const MeshPrimitive & aMeshPrimitive)
{
    if (aMeshPrimitive.indices)
    {
        ADLOG(gDrawLogger, trace)
             ("Indexed rendering of {} vertices with mode {}.", aMeshPrimitive.count, aMeshPrimitive.drawMode);

        const Indices & indices = *aMeshPrimitive.indices;
        graphics::bind_guard boundIndexBuffer{indices.ibo};
        glDrawElements(aMeshPrimitive.drawMode, 
                       aMeshPrimitive.count,
                       indices.componentType,
                       reinterpret_cast<void *>(indices.byteOffset));
    }
    else
    {
        ADLOG(gDrawLogger, trace)
             ("Array rendering of {} vertices with mode {}.", aMeshPrimitive.count, aMeshPrimitive.drawMode);

        glDrawArrays(aMeshPrimitive.drawMode,  
                     0, // Start at the beginning of enable arrays, all byte offsets are aleady applied.
                     aMeshPrimitive.count);
    }
}


/// \brief Instanced draw of the mesh primitive.
void drawCall(const MeshPrimitive & aMeshPrimitive, GLsizei aInstanceCount)
{
    if (aMeshPrimitive.indices)
    {
        ADLOG(gDrawLogger, trace)
             ("Indexed rendering of {} instance(s) of {} vertices with mode {}.",
              aMeshPrimitive.count, aInstanceCount, aMeshPrimitive.drawMode);

        const Indices & indices = *aMeshPrimitive.indices;
        graphics::bind_guard boundIndexBuffer{indices.ibo};
        glDrawElementsInstanced(
            aMeshPrimitive.drawMode, 
            aMeshPrimitive.count,
            indices.componentType,
            reinterpret_cast<void *>(indices.byteOffset),
            aInstanceCount);
    }
    else
    {
        ADLOG(gDrawLogger, trace)
             ("Instanced array rendering of {} instance(s) of {} vertices with mode {}.",
              aMeshPrimitive.count, aInstanceCount, aMeshPrimitive.drawMode);

        glDrawArraysInstanced(
            aMeshPrimitive.drawMode,  
            0, // Start at the beginning of enable arrays, all byte offsets are aleady applied.
            aMeshPrimitive.count,
            aInstanceCount);
    }
}


template <class ... VT_extraParams>
void render(const MeshPrimitive & aMeshPrimitive, VT_extraParams ... aExtraDrawParams)
{
    graphics::bind_guard boundVao{aMeshPrimitive.vao};

    const auto & material = aMeshPrimitive.material;

    // Culling
    if (material.doubleSided) glDisable(GL_CULL_FACE);
    else glEnable(GL_CULL_FACE);

    // Alpha mode
    switch(material.alphaMode)
    {
    case gltf::Material::AlphaMode::Opaque:
        glDisable(GL_BLEND);
        break;
    case gltf::Material::AlphaMode::Blend:
        glEnable(GL_BLEND);
        break;
    case gltf::Material::AlphaMode::Mask:
        ADLOG(gDrawLogger, critical)("Not supported: mask alpha mode.");
        throw std::logic_error{"Mask alpha mode not implemented."};
    }

    glActiveTexture(GL_TEXTURE0 + Renderer::gColorTextureUnit);
    glBindTexture(GL_TEXTURE_2D, *material.baseColorTexture);

    glActiveTexture(GL_TEXTURE0 + Renderer::gMetallicRoughnessTextureUnit);
    glBindTexture(GL_TEXTURE_2D, *material.metallicRoughnessTexture);

    drawCall(aMeshPrimitive, std::forward<VT_extraParams>(aExtraDrawParams)...);

    glBindTexture(GL_TEXTURE_2D, 0);
}



Renderer::Renderer() :
    mSkyboxProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gSkyboxVertexShader},
        {GL_FRAGMENT_SHADER, gEquirectangularSkyboxFragmentShader},
    })}
{
    setUniformInt(mSkyboxProgram, "u_equirectangularMap", gColorTextureUnit);
    initializePrograms();
}


const Renderer::ShadingPrograms & Renderer::activePrograms() const
{
    return mPrograms.at(mShadingModel);
}


template <class ... VT_extraParams>
void Renderer::renderImpl(const Mesh & aMesh,
                          graphics::Program & aProgram,
                          VT_extraParams ... aExtraParams) const
{
    // Not enabled by default OpenGL context.
    glEnable(GL_DEPTH_TEST);

    graphics::bind_guard boundProgram{aProgram};

    // IBL parameters
    setUniformFloat(aProgram, "u_ambientFactor", mIbl.mAmbientFactor); 
    setUniformInt(aProgram, "u_maxReflectionLod", Environment::gPrefilterLevels); 

    // Bind IBL textures
    if(auto & environment = mIbl.mEnvironment; environment)
    {
        glActiveTexture(GL_TEXTURE0 + Renderer::gIrradianceMapTextureUnit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, environment->mIrradianceCubemap);

        glActiveTexture(GL_TEXTURE0 + Renderer::gPrefilterMapTextureUnit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, environment->mPrefilteredCubemap);

        // Is present even if the environment is not, but it would not be used
        glActiveTexture(GL_TEXTURE0 + Renderer::gBrdfLutTextureUnit);
        glBindTexture(GL_TEXTURE_2D, mIbl.mBrdfLut);
    }

    setUniformInt(aProgram, "u_debugOutput", static_cast<int>(mColorOutput)); 

    // TODO do only once
    setUniformInt(aProgram, "u_baseColorTex", gColorTextureUnit); 
    setUniformInt(aProgram, "u_metallicRoughnessTex", gMetallicRoughnessTextureUnit); 

    for (const auto & primitive : aMesh.primitives)
    {
        setUniform(aProgram, "u_baseColorFactor", primitive.material.baseColorFactor); 
        setUniformFloat(aProgram, "u_metallicFactor", primitive.material.metallicFactor); 
        setUniformFloat(aProgram, "u_roughnessFactor", primitive.material.roughnessFactor); 

        // If the vertex color are not provided for the primitive, the default value (black)
        // will be used in the shaders. It must be offset to white.
        auto vertexColorOffset = primitive.providesColor() ?
            math::hdr::Rgba_f{0.f, 0.f, 0.f, 0.f} : math::hdr::Rgba_f{1.f, 1.f, 1.f, 0.f};
        setUniform(aProgram, "u_vertexColorOffset", vertexColorOffset); 

        gltfviewer::render(primitive, std::forward<VT_extraParams>(aExtraParams)...);
    }
}


void Renderer::render(const Mesh & aMesh) const
{
    renderImpl(aMesh, *activePrograms().at(GpuProgram::InstancedNoAnimation), aMesh.gpuInstances.size());
    renderSkybox();
}


void Renderer::render(const Mesh & aMesh, const Skeleton & aSkeleton) const
{
    bind(aSkeleton);
    renderImpl(aMesh, *activePrograms().at(GpuProgram::Skinning));
    renderSkybox();
}


void Renderer::renderSkybox() const
{
    if(auto & environment = mIbl.mEnvironment; environment)
    {
        glActiveTexture(GL_TEXTURE0 + gColorTextureUnit);
        graphics::bind(environment->mEnvironmentEquirectangular);
        graphics::bind(mSkyboxProgram);

        auto depthMaskGuard = graphics::scopeDepthMask(false);
        static const Cube gCube;
        gCube.draw();
    }
}


void Renderer::bind(const Skeleton & aSkeleton) const
{
    glBindBufferBase(GL_UNIFORM_BUFFER,
        gPaletteBlockBinding,
        aSkeleton.matrixPalette.uniformBuffer);
}


void Renderer::initializePrograms()
{
    auto setLights = [](graphics::Program & aProgram)
    {
        setUniform(aProgram, "u_light.position_world", math::Vec<4, GLfloat>{-100.f, 100.f, 1000.f, 1.f}); 
        setUniform(aProgram, "u_light.color", math::hdr::gWhite<GLfloat>);
        setUniformInt(aProgram, "u_light.specularExponent", 100);
        // Ideally, I suspect the sum should be 1
        setUniformFloat(aProgram, "u_light.diffuse", 0.3f);
        setUniformFloat(aProgram, "u_light.specular", 0.35f);
        setUniformFloat(aProgram, "u_light.ambient", 0.45f);
    };

    auto insertPrograms = [&](ShadingModel aShadingModel, const char * aFragmentSource)
    {
        {
            auto phong = std::make_shared<graphics::Program>(
                graphics::makeLinkedProgram({
                    {GL_VERTEX_SHADER,   gltfviewer::gStaticVertexShader},
                    {GL_FRAGMENT_SHADER, aFragmentSource},
            }));
            setLights(*phong);
            mPrograms[aShadingModel].emplace(GpuProgram::InstancedNoAnimation, std::move(phong));
            }

        {
            auto skinning = std::make_shared<graphics::Program>(
                graphics::makeLinkedProgram({
                    {GL_VERTEX_SHADER,   gltfviewer::gSkeletalVertexShader},
                    {GL_FRAGMENT_SHADER, aFragmentSource},
            }));
            if(auto paletteBlockId = glGetUniformBlockIndex(*skinning, "MatrixPalette");
               paletteBlockId != GL_INVALID_INDEX)
            {
                glUniformBlockBinding(*skinning, paletteBlockId, gPaletteBlockBinding);
            }
            else
            {
                throw std::logic_error{"Uniform block name could not be found."};
            }
            setLights(*skinning);
            mPrograms[aShadingModel].emplace(GpuProgram::Skinning, std::move(skinning));
        }
    };

    insertPrograms(ShadingModel::Phong, gltfviewer::gPhongFragmentShader);
    insertPrograms(ShadingModel::PbrReference, gltfviewer::gPbrFragmentShader.c_str());
    insertPrograms(ShadingModel::PbrLearn, gltfviewer::gPbrLearnFragmentShader.c_str());
    insertPrograms(ShadingModel::PbrLearnIbl, gltfviewer::gIblPbrLearnFragmentShader.c_str());

    // The texture units mappings for IBL are permanent
    for(auto & [_key, program] : mPrograms.at(ShadingModel::PbrLearnIbl))
    {
        setUniformInt(*program, "u_irradianceMap", gIrradianceMapTextureUnit); 
        setUniformInt(*program, "u_prefilterMap", gPrefilterMapTextureUnit); 
        setUniformInt(*program, "u_brdfLut", gBrdfLutTextureUnit); 
    }
}


void Renderer::setCameraTransformation(const math::AffineMatrix<4, GLfloat> & aTransformation)
{
    for (auto & [_key, program] : activePrograms())
    {
        setUniform(*program, "u_camera", aTransformation); 
        // Do the inverse computation, instead of requiring a separate call taking the camera position direclty.
        setUniform(*program, "u_cameraPosition_world", 
                   (math::Vec<4, GLfloat>{0.f, 0.f, 0.f, 1.f} * aTransformation.inverse()).xyz()); 
    }
    math::AffineMatrix<4, GLfloat> viewOrientation{aTransformation.getLinear()};
    setUniform(mSkyboxProgram,  "u_cameraOrientation", viewOrientation); 
}


void Renderer::setProjectionTransformation(const math::Matrix<4, 4, GLfloat> & aTransformation)
{
    for (auto & [_key, program] : activePrograms())
    {
        setUniform(*program, "u_projection", aTransformation); 
    }
    setUniform(mSkyboxProgram, "u_projection", aTransformation); 
}


void Renderer::setLight(const Light & aLight)
{
    for (auto & [_key, program] : activePrograms())
    {
        setUniform(*program, "u_lightColor", aLight.mColor); 
        setUniform(*program, "u_lightDirection_world", aLight.mDirection); 
    }
}



void Renderer::togglePolygonMode()
{
    switch(mPolygonMode)
    {
    case PolygonMode::Fill:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        mPolygonMode = PolygonMode::Line;
        break;
    case PolygonMode::Line:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        mPolygonMode = PolygonMode::Fill;
        break;
    }
}


void Renderer::showRendererOptions()
{
    ImGui::Begin("Rendering options");
    if (ImGui::Button("Toggle wireframe"))
    {
        togglePolygonMode();
    }

    if (ImGui::BeginCombo("Shading", to_string(mShadingModel).c_str()))
    {
        for (int shadingId = 0; shadingId < static_cast<int>(ShadingModel::_End); ++shadingId)
        {
            ShadingModel model = static_cast<ShadingModel>(shadingId);
            const bool isSelected = (mShadingModel == model);
            if (ImGui::Selectable(to_string(model).c_str(), isSelected))
            {
                mShadingModel = model;
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (mShadingModel == ShadingModel::PbrLearn || mShadingModel == ShadingModel::PbrLearnIbl)
    {
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
    }

    ImGui::SliderFloat("IBL factor", &mIbl.mAmbientFactor, 0.f, 3.0f, "%.3f");

    ImGui::End();
}


} // namespace gltfviewer
} // namespace ad 
