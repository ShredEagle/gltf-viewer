#include "CubeQuad.h"

#include <graphics/detail/UnitQuad.h>


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

} // anonymouse namespace


Cube::Cube() :
    mCubeVertices{loadVertexBuffer<const Vertex>(
        mVao,
        gVertexDescription,
        std::span{gVertices})
    },
    mCubeIndices{loadIndexBuffer<const GLushort>(
        mVao, gCubeIndices, graphics::BufferHint::StaticRead)
    }
{}


void Cube::draw() const
{
    graphics::ScopedBind boundVao{mVao};
    graphics::ScopedBind boundIbo{mCubeIndices};
    glDrawElements(GL_TRIANGLES, gCubeIndices.size(), GL_UNSIGNED_SHORT, nullptr);
}


Quad::Quad() :
    mVertexSpecification{graphics::detail::make_UnitQuad()}
{}


void Quad::draw() const
{
    graphics::ScopedBind boundVao{mVertexSpecification};
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


} // namespace gltfviewer
} // namespace ad
