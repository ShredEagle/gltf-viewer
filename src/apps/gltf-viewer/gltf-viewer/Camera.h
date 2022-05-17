#pragma once


#include "Logging.h"
#include "Polar.h"

#include <arte/gltf/Gltf.h>

#include <graphics/AppInterface.h>
#include <graphics/CameraUtilities.h>

#include <renderer/GL_Loader.h>

// Dirty include, to get the GLFW input definitions
#include <GLFW/glfw3.h>


namespace ad {
namespace gltfviewer {


/// \brief Models a glTF camera, as loaded from the asset file.
struct CameraInstance
{
    math::AffineMatrix<4, float> getViewTransform() const
    { return orientation.inverse(); }

    math::AffineMatrix<4, float> orientation;
    arte::Const_Owned<arte::gltf::Camera> gltfCamera;
};


math::Matrix<4, 4, float> getProjection(arte::Const_Owned<arte::gltf::Camera> aCamera,
                                        float aAspectRatio);


/// \brief Models the custom camera, allowing user control of the point of view like modeling tools.
class UserCamera
{
public:
    UserCamera(std::shared_ptr<graphics::AppInterface> aAppInterface) :
        mAppInterface{std::move(aAppInterface)}
    {}

    /// \return View matrix.
    math::AffineMatrix<4, GLfloat> getViewTransform();

    void callbackCursorPosition(double xpos, double ypos);
    void callbackMouseButton(int button, int action, int mods, double xpos, double ypos);
    void callbackScroll(double xoffset, double yoffset);

    void setOrigin(math::Position<3, GLfloat> aNewOrigin)
    { mPolarOrigin = aNewOrigin; }

    /// \return Projection matrix.
    math::Matrix<4, 4, float> getProjectionTransform();

    void setViewedHeight(GLfloat aHeight);

    void appendProjectionControls();

private:
    enum class ControlMode
    {
        None,
        Orbit,
        Pan,
    };

    std::shared_ptr<graphics::AppInterface> mAppInterface;
    Polar mPosition{1.f};
    math::Position<3, GLfloat> mPolarOrigin{0.f, 0.f, 0.f};
    ControlMode mControlMode;
    math::Position<2, GLfloat> mPreviousDragPosition{0.f, 0.f};
    GLfloat mCurrentProjectionHeight;
    bool mPerspectiveProjection{false};

    static constexpr math::Position<3, GLfloat> gGazePoint{0.f, 0.f, 0.f};
    static constexpr math::Vec<2, GLfloat> gMouseControlFactor{1/700.f, 1/700.f};
    static constexpr GLfloat gViewedDepth = 100;
    static constexpr GLfloat gScrollFactor = 0.05;
};


class CameraSystem
{
public:
    CameraSystem(std::shared_ptr<graphics::AppInterface> aAppInterface);

    void setViewedBox(math::Box<GLfloat> aSceneBoundingBox);

    math::AffineMatrix<4, GLfloat> getViewTransform();
    math::Matrix<4, 4, float> getProjectionTransform(std::shared_ptr<graphics::AppInterface> aAppInterface);

    void appendCameraControls();

    void clearGltfCameras()
    { mCameraInstances.clear(); }

    void push(CameraInstance aInstance)
    { mCameraInstances.push_back(aInstance); }

    void callbackCursorPosition(double xpos, double ypos)
    { if (!mSelectedCamera) mCustomCamera.callbackCursorPosition(xpos, ypos); }

    void callbackMouseButton(int button, int action, int mods, double xpos, double ypos)
    { if (!mSelectedCamera) mCustomCamera.callbackMouseButton(button, action, mods, xpos, ypos); }

    void callbackScroll(double xoffset, double yoffset)
    { if (!mSelectedCamera) mCustomCamera.callbackScroll(xoffset, yoffset); }


private:
    // TODO prefix m
    std::optional<std::size_t> mSelectedCamera;
    std::vector<CameraInstance> mCameraInstances;
    UserCamera mCustomCamera;

    static constexpr GLfloat gViewportHeightFactor = 1.6f;
};

} // namespace gltfviewer
} // namespace ad
