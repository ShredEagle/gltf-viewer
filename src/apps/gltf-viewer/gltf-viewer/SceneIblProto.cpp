#include "SceneIblProto.h"

#include "ShadersIblProto.h"

#include <renderer/GL_Loader.h>
#include <renderer/Uniforms.h>


namespace ad {
namespace gltfviewer {


namespace {

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


} // anonymous namespace


IblRenderer::IblRenderer(const filesystem::path & aEnvironmentMap) :
    mCubeVertices{loadVertexBuffer<const Vertex>(
        mVao,
        gVertexDescription,
        gVertices)
    },
    mCubeIndices{loadIndexBuffer<const GLushort>(
        mVao, gCubeIndices, graphics::BufferHint::StaticRead)
    },
    mCubemapProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gEquirectangularFragmentShader},
    })},
    mModelProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gEquirectangularFragmentShader},
    })},
    mCubemap{loadCubemap(aEnvironmentMap)}
{
    //setUniformInt(mCubemapProgram, "u_cubemap", gCubemapTextureUnit);
    setUniformInt(mCubemapProgram, "u_equirectangularMap", gCubemapTextureUnit);
    //setUniformInt(mModelProgram, "u_cubemap", gCubemapTextureUnit);
    setUniformInt(mModelProgram, "u_equirectangularMap", gCubemapTextureUnit);
}


void IblRenderer::render() const
{
    graphics::bind_guard boundVao{mVao};
    graphics::bind_guard boundIbo{mCubeIndices};

    glActiveTexture(GL_TEXTURE0 + gCubemapTextureUnit);
    graphics::bind_guard boundCubemap{mCubemap};
   
    // Render skybox
    {
        auto depthMaskGuard = graphics::scopeDepthMask(false);
        graphics::bind_guard boundProgram{mCubemapProgram};
        glDrawElements(GL_TRIANGLES, gCubeIndices.size(), GL_UNSIGNED_SHORT, nullptr);
    }

    // Render model (a cube)
    {
        graphics::bind_guard boundProgram{mModelProgram};
        glDrawElements(GL_TRIANGLES, gCubeIndices.size(), GL_UNSIGNED_SHORT, nullptr);
    }
}


void SceneIblProto::render() const
{
    glEnable(GL_DEPTH_TEST);  
    mRenderer.render(); 
}


void SceneIblProto::setView()
{
    math::AffineMatrix<4, GLfloat> viewOrientation{mCameraSystem.getViewTransform().getLinear()};
    setUniform(mRenderer.mCubemapProgram, "u_camera", viewOrientation); 
    setUniform(mRenderer.mModelProgram, "u_camera", mCameraSystem.getViewTransform()); 
}


void SceneIblProto::setProjection()
{
    setUniform(mRenderer.mCubemapProgram, "u_projection", mCameraSystem.getCubemapProjectionTransform(mAppInterface)); 
    setUniform(mRenderer.mModelProgram, "u_projection", mCameraSystem.getProjectionTransform(mAppInterface)); 
}


void SceneIblProto::showSceneControls()
{
    ImGui::Begin("Scene options");
    mCameraSystem.appendCameraControls();
    ImGui::End();

}


} // namespace gltfviewer
} // namespace ad
