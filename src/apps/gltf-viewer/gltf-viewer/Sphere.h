#pragma once


#include <renderer/VertexSpecification.h>


namespace ad {
namespace gltfviewer {


struct Sphere
{
    Sphere(unsigned int aSegments = 64);

    void draw() const;

    graphics::VertexArrayObject mVao;
    graphics::VertexBufferObject mVertices;
    graphics::IndexBufferObject mIndices;
    GLuint mIndicesCount;

    struct Vertex
    {
        math::Position<3, GLfloat> position;
        math::Position<2, GLfloat> uv;
        math::Vec<3, GLfloat> normal;
    };

    static constexpr std::initializer_list<graphics::AttributeDescription> gVertexDescription = {
        { 0, 3, offsetof(Vertex, position), graphics::MappedGL<GLfloat>::enumerator},
        { 1, 2, offsetof(Vertex, uv),       graphics::MappedGL<GLfloat>::enumerator},
        { 2, 3, offsetof(Vertex, normal),   graphics::MappedGL<GLfloat>::enumerator},
    };
};


// From learnopengl
inline Sphere::Sphere(unsigned int aSegments)
{
        std::vector<Vertex> vertices;
        std::vector<GLushort> indices;

        const float PI = 3.14159265359f;

        for (unsigned int x = 0; x <= aSegments; ++x)
        {
            for (unsigned int y = 0; y <= aSegments; ++y)
            {
                float xSegment = (float)x / (float)aSegments;
                float ySegment = (float)y / (float)aSegments;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                vertices.push_back(
                    Vertex{
                        {xPos, yPos, zPos},
                        {xSegment, ySegment},
                        {xPos, yPos, zPos}
                    }
                );
            }
        }

        mVertices = loadVertexBuffer<Vertex>(
            mVao,
            gVertexDescription,
            vertices);

        bool oddRow = false;
        for (unsigned int y = 0; y < aSegments; ++y)
        {
            if (!oddRow) // even rows: y == 0, y == 2; and so on
            {
                for (unsigned int x = 0; x <= aSegments; ++x)
                {
                    indices.push_back(y * (aSegments + 1) + x);
                    indices.push_back((y + 1) * (aSegments + 1) + x);
                }
            }
            else
            {
                for (int x = aSegments; x >= 0; --x)
                {
                    indices.push_back((y + 1) * (aSegments + 1) + x);
                    indices.push_back(y * (aSegments + 1) + x);
                }
            }
            oddRow = !oddRow;
        }
        mIndices = loadIndexBuffer<GLushort>(mVao, indices, graphics::BufferHint::StaticRead);
        mIndicesCount = static_cast<unsigned int>(indices.size());
}


inline void Sphere::draw() const
{
    graphics::bind_guard boundVao{mVao};
    graphics::bind_guard boundIbo{mIndices};
    glDrawElements(GL_TRIANGLE_STRIP, mIndicesCount, GL_UNSIGNED_SHORT, nullptr);
}


} // namespace gltfviewer
} // namespace ad
