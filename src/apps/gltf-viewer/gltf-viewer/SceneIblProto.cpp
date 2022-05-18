#include "SceneIblProto.h"

#include "Shaders.h"

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

graphics::Texture loadCubemap(const filesystem::path & aFolder, filesystem::path aExtension = ".jpg")
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
        // Bind each time, some calls might be using a scoped guard which unbinds at the end...
        graphics::bind_guard boundCubemap{cubemap};

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
    auto image = ImageType{aFolder / filename};
    allocateStorage(cubemap, GL_RGBA8, image.dimensions());
    loadData(image, faceId);

    for(++faceId; faceId != indexToFilename.size(); ++faceId)
    {
        filename = indexToFilename[faceId];
        filename += aExtension;
        image = ImageType{aFolder / filename};
        loadData(image, faceId);
    }

    return cubemap;
}


inline const GLchar* gIblVertexShader = R"#(
    #version 400

    layout(location=0) in vec4 ve_position;

    uniform mat4 u_camera;
    uniform mat4 u_projection;

    out vec4 ex_color;
    out vec4 ex_position_view;
    out vec3 ex_position_local;

    void main(void)
    {
        ex_position_local = ve_position.xyz;

        mat4 modelViewTransform = u_camera;
        ex_position_view = modelViewTransform * ve_position;
        ex_color = vec4(1., 0., 0., 1.);

        gl_Position = u_projection * ex_position_view;
    }
)#";

inline const GLchar* gIblFragmentShader = R"#(
    #version 400

    in vec3 ex_position_local;
    in vec4 ex_color;

    out vec4 out_color;

    uniform samplerCube u_cubemap;

    void main(void)
    {
        out_color = texture(u_cubemap, ex_position_local);
    }
)#";

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
    mProgram{graphics::makeLinkedProgram({
        {GL_VERTEX_SHADER,   gIblVertexShader},
        {GL_FRAGMENT_SHADER, gIblFragmentShader},
    })},
    mCubemap{loadCubemap(aEnvironmentMap)}
{
    setUniformInt(mProgram, "u_cubemap", gCubemapTextureUnit);
}


void IblRenderer::render() const
{
    graphics::bind_guard boundVao{mVao};
    graphics::bind_guard boundIbo{mCubeIndices};
    graphics::bind_guard boundProgram{mProgram};

    glActiveTexture(GL_TEXTURE0 + gCubemapTextureUnit);
    graphics::bind_guard boundCubemap{mCubemap};

    glDrawElements(GL_TRIANGLES, gCubeIndices.size(), GL_UNSIGNED_SHORT, nullptr);
}


void SceneIblProto::render() const
{
    glEnable(GL_DEPTH_TEST);  
    mRenderer.render(); 
}


void SceneIblProto::setView(const math::AffineMatrix<4, float> & aViewTransform)
{
    setUniform(mRenderer.mProgram, "u_camera", aViewTransform); 
}


void SceneIblProto::setProjection(const math::Matrix<4, 4, float> & aProjectionTransform)
{
    setUniform(mRenderer.mProgram, "u_projection", aProjectionTransform); 
}


void SceneIblProto::showSceneControls()
{
    ImGui::Begin("Scene options");
    ImGui::End();
}


} // namespace gltfviewer
} // namespace ad
