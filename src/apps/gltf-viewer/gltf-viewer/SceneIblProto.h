#pragma once


#include "Camera.h"
#include "ImguiUi.h"

#include <graphics/AppInterface.h>
#include <graphics/Timer.h>

#include <math/Homogeneous.h>
#include <math/Matrix.h>

#include <renderer/Shading.h>
#include <renderer/Texture.h>
#include <renderer/VertexSpecification.h>


namespace ad {
namespace gltfviewer {


struct IblRenderer
{
    IblRenderer(const filesystem::path & aEnvironmentMap);

    void render() const;

    graphics::VertexArrayObject mVao;
    graphics::VertexBufferObject mCubeVertices;
    graphics::IndexBufferObject mCubeIndices;
    graphics::Program mCubemapProgram;
    graphics::Program mModelProgram;
    graphics::Texture mCubemap;

    static constexpr GLsizei gCubemapTextureUnit{3};
};

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
