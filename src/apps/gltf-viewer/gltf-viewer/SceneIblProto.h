#pragma once


#include "Camera.h"
#include "GltfRendering.h"
#include "ImguiUi.h"
#include "Sphere.h"

#include <graphics/AppInterface.h>
#include <graphics/Timer.h>

#include <math/Homogeneous.h>
#include <math/Matrix.h>

#include <renderer/Shading.h>
#include <renderer/Texture.h>
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




struct IblRenderer
{
    enum class Environment
    {
        Radiance,
        Irradiance,
        Prefiltered,

        // Keep last
        _End//
    };

    IblRenderer(const filesystem::path & aEnvironmentMap);

    void update();

    void render() const;

    void showRendererOptions();

    Cube mCube;
    Sphere mSphere;
    graphics::Program mCubemapProgram;
    graphics::Program mEquirectangularProgram;
    graphics::Program mModelProgram;
    graphics::Program mTexture2DProgram;
    graphics::Texture mCubemap;
    graphics::Texture mIrradianceCubemap;
    graphics::Texture mPrefilteredCubemap;
    graphics::Texture mBrdfLut;

    bool mShowBrdfLut{false};
    Environment mEnvMap{Environment::Radiance};
    int mPrefilteredLod{-1};

    bool mShowObject{true};
    GLfloat mMetallic{0.f};
    GLfloat mRoughness{0.3f};
    GLfloat mAmbientFactor{0.3f};
    DebugColor mColorOutput{DebugColor::Default};

    static constexpr GLsizei gCubemapTextureUnit{3};
    static constexpr GLsizei gPrefilterMapTextureUnit{4};
    static constexpr GLsizei gBrdfLutTextureUnit{5};
};


inline std::string to_string(IblRenderer::Environment aEnvironment)
{
    switch(aEnvironment)
    {
    case IblRenderer::Environment::Radiance:
        return "Radiance";
    case IblRenderer::Environment::Irradiance:
        return "Irradiance";
    case IblRenderer::Environment::Prefiltered:
        return "Prefiltered";
    }
}


struct SceneIblProto
{
    SceneIblProto(std::shared_ptr<graphics::AppInterface> aAppInterface,
                  ImguiUi & aImgui,
                  const filesystem::path & aEnvironmentMap) :
        mAppInterface{std::move(aAppInterface)},
        mCameraSystem{mAppInterface},
        mImgui{aImgui},
        mRenderer{aEnvironmentMap}
    {
        mCameraSystem.setViewedBox(math::Box<GLfloat>::CenterOnOrigin({2.f, 2.f, 2.f}));

        using namespace std::placeholders;
        mAppInterface->registerKeyCallback(
            std::bind(&SceneIblProto::callbackKeyboard, this, _1, _2, _3, _4));
        mAppInterface->registerMouseButtonCallback(
            std::bind(&SceneIblProto::callbackMouseButton, this, _1, _2, _3, _4, _5));
        mAppInterface->registerCursorPositionCallback(
            std::bind(&SceneIblProto::callbackCursorPosition, this, _1, _2));
        mAppInterface->registerScrollCallback(
            std::bind(&SceneIblProto::callbackScroll, this, _1, _2));
    }

    void setView();
    void setProjection();

    void showSceneControls();

    void update(const graphics::Timer &)
    {
        setView();
        // This is a bit greedy, as the projection transform rarely changes compared to the view.
        setProjection();

        mRenderer.update();
    }

    void render() const;

    void callbackKeyboard(int key, int scancode, int action, int mods)
    {
        if (mImgui.isCapturingKeyboard())
        {
            return;
        }

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            mAppInterface->requestCloseApplication();
        }
    }

    void callbackCursorPosition(double xpos, double ypos)
    {
        mCameraSystem.callbackCursorPosition(xpos, ypos);
    }

    void callbackMouseButton(int button, int action, int mods, double xpos, double ypos)
    {
        if (mImgui.isCapturingMouse())
        {
            return;
        }

        mCameraSystem.callbackMouseButton(button, action, mods, xpos, ypos);
    }

    void callbackScroll(double xoffset, double yoffset)
    {
        if (mImgui.isCapturingMouse())
        {
            return;
        }

        mCameraSystem.callbackScroll(xoffset, yoffset);
    }

    std::shared_ptr<graphics::AppInterface> mAppInterface;
    CameraSystem mCameraSystem;
    ImguiUi & mImgui;
    IblRenderer mRenderer;
};


} // namespace gltfviewer
} // namespace ad