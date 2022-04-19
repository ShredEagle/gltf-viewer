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


struct CameraInstance
{
    math::AffineMatrix<4, float> getViewTransform() const
    { return orientation.inverse(); }

    math::AffineMatrix<4, float> orientation;
    arte::Const_Owned<arte::gltf::Camera> gltfCamera;
};


inline math::Matrix<4, 4, float> getProjection(arte::Const_Owned<arte::gltf::Camera> aCamera,
                                               float aAspectRatio)
{
    switch(aCamera->type)
    {
    case arte::gltf::Camera::Type::Orthographic:
    {
        auto & ortho = std::get<arte::gltf::Camera::Orthographic>(aCamera->projection);
        //float r = aAspectRatio ? *aAspectRatio * ortho.ymag : ortho.xmag;
        float r = aAspectRatio * ortho.ymag;
        float t = ortho.ymag;
        float f = ortho.zfar;
        float n = ortho.znear;
        return {
            1/r,    0.f,    0.f,         0.f,
            0.f,    1/t,    0.f,         0.f,
            0.f,    0.f,    2/(n-f),     0.f,
            0.f,    0.f,    (f+n)/(n-f), 1.f, 
        };
        break;
    }
    case arte::gltf::Camera::Type::Perspective:
    {
        auto & persp = std::get<arte::gltf::Camera::Perspective>(aCamera->projection);
        //float a = aAspectRatio ? *aAspectRatio : persp.aspectRatio;
        float a = aAspectRatio;
        float y = persp.yfov;
        float n = persp.znear;
        float tan = std::tan(0.5f * y);

        if (persp.zfar)
        {
            float f = *persp.zfar;
            return {
                1/(a*tan),  0.f,    0.f,         0.f,
                0.f,        1/tan,  0.f,         0.f,
                0.f,        0.f,    (f+n)/(n-f), -1.f,
                0.f,        0.f,    2*f*n/(n-f), 0.f, 
            };
        }
        else
        {
            return {
                1/(a*tan),  0.f,    0.f,        0.f,
                0.f,        1/tan,  0.f,        0.f,
                0.f,        0.f,    -1.f,       -1.f,
                0.f,        0.f,    -2*n,       0.f, 
            };
        }
        break;
    }
    }
}


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

    void setOrigin(math::Position<3, GLfloat> aNewOrigin)
    { mPolarOrigin = aNewOrigin; }

    /// \return Projection matrix.
    math::Matrix<4, 4, float> setViewedHeight(GLfloat aHeight);
    math::Matrix<4, 4, float> multiplyViewedHeight(GLfloat aFactor);

private:
    enum class ControlMode
    {
        None,
        Orbit,
        Pan,
    };

    std::shared_ptr<graphics::AppInterface> mAppInterface;
    Polar mPosition{3.f};
    math::Position<3, GLfloat> mPolarOrigin{0.f, 0.f, 0.f};
    ControlMode mControlMode;
    math::Position<2, GLfloat> mPreviousDragPosition{0.f, 0.f};
    GLfloat mCurrentProjectionHeight;

    static constexpr math::Position<3, GLfloat> gGazePoint{0.f, 0.f, 0.f};
    static constexpr math::Vec<2, GLfloat> gMouseControlFactor{1/700.f, 1/700.f};
    static constexpr GLfloat gViewedDepth = 10000;
};


inline math::AffineMatrix<4, GLfloat> UserCamera::getViewTransform()
{
    const math::Position<3, GLfloat> cameraCartesian = mPosition.toCartesian();
    ADLOG(gDrawLogger, trace)("Camera position {}.", cameraCartesian);

    math::Vec<3, GLfloat> gazeDirection = gGazePoint - cameraCartesian;
    return graphics::getCameraTransform(mPolarOrigin + cameraCartesian.as<math::Vec>(),
                                        gazeDirection,
                                        mPosition.getUpTangent());
}


inline void UserCamera::callbackCursorPosition(double xpos, double ypos)
{
    using Radian = math::Radian<GLfloat>;
    math::Position<2, GLfloat> cursorPosition{(GLfloat)xpos, (GLfloat)ypos};

    // top-left corner origin
    switch (mControlMode)
    {
    case ControlMode::Orbit:
    {
        auto angularIncrements = (cursorPosition - mPreviousDragPosition).cwMul(gMouseControlFactor);

        // The viewed object should turn in the direction of the mouse,
        // so the camera angles are changed in the opposite direction (hence the substractions).
        mPosition.azimuthal -= Radian{angularIncrements.x()};
        mPosition.polar -= Radian{angularIncrements.y()};
        mPosition.polar = std::max(Radian{0}, std::min(Radian{math::pi<GLfloat>}, mPosition.polar));
        break;
    }
    case ControlMode::Pan:
    {
        auto dragVector{cursorPosition - mPreviousDragPosition};
        dragVector *= mCurrentProjectionHeight / mAppInterface->getWindowSize().height();
        mPolarOrigin -= dragVector.x() * mPosition.getCCWTangent().normalize() 
                        - dragVector.y() * mPosition.getUpTangent().normalize();
        break;
    }
    }

    mPreviousDragPosition = cursorPosition;
}


inline void UserCamera::callbackMouseButton(int button, int action, int mods, double xpos, double ypos)
{
    if (action == GLFW_PRESS)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            mControlMode = ControlMode::Orbit;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            mControlMode = ControlMode::Pan;
            break;
        }
    }
    else if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE)
             && action == GLFW_RELEASE)
    {
        mControlMode = ControlMode::None;
    }
}


inline math::Matrix<4, 4, float> UserCamera::setViewedHeight(GLfloat aHeight)
{
    mCurrentProjectionHeight = aHeight;
    const math::Box<GLfloat> projectedBox =
        graphics::getViewVolumeRightHanded(mAppInterface->getWindowSize(), mCurrentProjectionHeight, gViewedDepth, 2*gViewedDepth);
    math::Matrix<4, 4, float> projectionTransform = 
        math::trans3d::orthographicProjection(projectedBox)
        * math::trans3d::scale(1.f, 1.f, -1.f); // OpenGL clipping space is left handed.

    return projectionTransform;
}


inline math::Matrix<4, 4, float> UserCamera::multiplyViewedHeight(GLfloat aFactor)
{
    return setViewedHeight(mCurrentProjectionHeight * aFactor);
}


} // namespace gltfviewer
} // namespace ad
