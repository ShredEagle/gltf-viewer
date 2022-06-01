#pragma once


#include <renderer/VertexSpecification.h>


namespace ad {
namespace gltfviewer {


struct Cube
{
    Cube();

    void draw() const;

    graphics::VertexArrayObject mVao;
    graphics::VertexBufferObject mCubeVertices;
    graphics::IndexBufferObject mCubeIndices;
};


struct Quad
{
    Quad();

    void draw() const;

    graphics::VertexSpecification mVertexSpecification;
};





} // namespace gltfviewer
} // namespace ad
