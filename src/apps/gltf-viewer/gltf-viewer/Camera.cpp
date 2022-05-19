#include "Camera.h"

#include <imgui.h>

#include <algorithm>


namespace ad {
namespace gltfviewer {


math::Matrix<4, 4, float> getProjection(arte::Const_Owned<arte::gltf::Camera> aCamera,
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


//
// UserCamera
//
math::AffineMatrix<4, GLfloat> UserCamera::getViewTransform()
{
    const math::Position<3, GLfloat> cameraCartesian = mPosition.toCartesian();
    ADLOG(gDrawLogger, trace)("Camera position {}.", cameraCartesian);

    math::Vec<3, GLfloat> gazeDirection = gGazePoint - cameraCartesian;
    return graphics::getCameraTransform(mPolarOrigin + cameraCartesian.as<math::Vec>(),
                                        gazeDirection,
                                        mPosition.getUpTangent());
}


math::Radian<GLfloat> getVerticalFov(GLfloat aScreenHeight, GLfloat aScreenDistance)
{
    return 2 * math::atan<math::Radian_tag>((aScreenHeight / 2) / aScreenDistance); 
}


math::Radian<GLfloat> defaultVerticalFov()
{
    //float projectionHeight = mCurrentProjectionHeight;
    constexpr GLfloat screenHeight = 0.36347f; // Height of an ideal 16/10 27" screen.
    constexpr GLfloat screenDistance = 0.6f; // Screen at 60cm from the eyes.
    return getVerticalFov(screenHeight, screenDistance);
}


math::Matrix<4, 4, float> UserCamera::getProjectionTransform(GLfloat aOrthographicPerspectiveEquivalence)
{
    float nearPlaneZ = -mNearPlaneDistance;

    // For perspective, get the height of the near plane.
    //   (to use same near plane in perspective matrix and projection matrix).
    // For orthographic, get the height at the polar origin.
    // This implements "apparent size consistency" for dimensions in the plane containing polar origin
    // and perpendicalur to view vector.
    GLfloat projectionHeight = 2 * tan(mVerticalFov / 2)
                               * std::abs(mPerspectiveProjection ? nearPlaneZ : aOrthographicPerspectiveEquivalence);

    const math::Box<GLfloat> projectedBox = graphics::getViewVolumeRightHanded(
        mAppInterface->getWindowSize(),
        projectionHeight,
        nearPlaneZ,
        mViewedDepth);

    math::Matrix<4, 4, float> projectionTransform = 
        math::trans3d::orthographicProjection(projectedBox)
        * math::trans3d::scale(1.f, 1.f, -1.f); // OpenGL clipping space is left handed.

    if(mPerspectiveProjection)
    {
        auto perspective = math::trans3d::perspective(nearPlaneZ, (nearPlaneZ - mViewedDepth));
        projectionTransform = perspective * projectionTransform;
    }

    return projectionTransform;
}


math::Matrix<4, 4, float> UserCamera::getProjectionTransform()
{
    return getProjectionTransform(mPosition.r);
}


void UserCamera::callbackCursorPosition(double xpos, double ypos)
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

        // View height in the plane of the polar origin (plane perpendicular to the view vector)
        // (the view height depends on the "plane distance" in the perspective case).
        GLfloat viewHeight = 2 * tan(mVerticalFov / 2) * std::abs(mPosition.r);
        dragVector *= viewHeight / mAppInterface->getWindowSize().height();

        mPolarOrigin -= dragVector.x() * mPosition.getCCWTangent().normalize() 
                        - dragVector.y() * mPosition.getUpTangent().normalize();
        break;
    }
    }

    mPreviousDragPosition = cursorPosition;
}


void UserCamera::callbackMouseButton(int button, int action, int mods, double xpos, double ypos)
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


void UserCamera::callbackScroll(double xoffset, double yoffset)
{
    auto factor = (1 - yoffset * gScrollFactor);
    mPosition.r *= factor;
    mPosition.r = std::clamp(mPosition.r, gMinCameraDistance, gMaxCameraDistance);
}


void UserCamera::setViewedBox(math::Box<GLfloat> aSceneBoundingBox)
{
    setOrigin(aSceneBoundingBox.center());
    setDistance(
        (aSceneBoundingBox.height() / 2 * gViewportHeightFactor) / tan(mVerticalFov/2) 
        + aSceneBoundingBox.depth() / 2);

    //
    // Approximate a "round" view depth, with candidates being powers of 10
    //

    // The distance of the far plane from the camera
    GLfloat farPlane = mPosition.r + aSceneBoundingBox.depth() / 2;
    for(int power = 0; ; ++power)
    {
        // 8 as an arbitrary factor.
        if(GLfloat depth = std::pow(10, power); depth >= 8 * farPlane)
        {
            mViewedDepth = depth;
            break;
        }
    }
}


//
// CameraSystem
//
CameraSystem::CameraSystem(std::shared_ptr<graphics::AppInterface> aAppInterface) :
    mCustomCamera{std::move(aAppInterface)}
{}


void CameraSystem::setViewedBox(math::Box<GLfloat> aSceneBoundingBox)
{
    ADLOG(gPrepareLogger, info)
         ("Centering camera on {}, scene bounding box is {}.",
          aSceneBoundingBox.center(), aSceneBoundingBox);
    mCustomCamera.setViewedBox(aSceneBoundingBox);
}


math::AffineMatrix<4, GLfloat> CameraSystem::getViewTransform()
{
    if (!mSelectedCamera)
    {
        return mCustomCamera.getViewTransform();
    }
    else
    {
        return mCameraInstances[*mSelectedCamera].getViewTransform();
    }
}


math::Matrix<4, 4, float> CameraSystem::getProjectionTransform(std::shared_ptr<graphics::AppInterface> aAppInterface)
{
    if (!mSelectedCamera)
    {
        return mCustomCamera.getProjectionTransform();
    }
    else
    {
        return getProjection(mCameraInstances[*mSelectedCamera].gltfCamera,
                             getRatio<GLfloat>(aAppInterface->getFramebufferSize()));
    }
}


math::Matrix<4, 4, float> CameraSystem::getCubemapProjectionTransform(std::shared_ptr<graphics::AppInterface> aAppInterface)
{
    if (!mSelectedCamera)
    {
        constexpr GLfloat gCubemapDepth = 1.f;
        return mCustomCamera.getProjectionTransform(gCubemapDepth);
    }
    else
    {
        return getProjection(mCameraInstances[*mSelectedCamera].gltfCamera,
                             getRatio<GLfloat>(aAppInterface->getFramebufferSize()));
    }
}


//
// GUI
//
void CameraSystem::appendCameraControls()
{
    // Camera selection
    auto nameCamera = [&](std::size_t aId)
    { 
        std::ostringstream oss;
        oss << "#" << aId << " (" << arte::to_string(mCameraInstances[aId].gltfCamera->type);
        if (!mCameraInstances[aId].gltfCamera->name.empty())
        {
            oss << " '" << mCameraInstances[aId].gltfCamera->name << "'";
        }
        oss << ")";
        return oss.str();
    };

    const std::string customCameraName = "User camera";
    std::string preview = mSelectedCamera ? nameCamera(*mSelectedCamera) : customCameraName;
    if (ImGui::BeginCombo("Camera", preview.c_str()))
    {
        if (ImGui::Selectable(customCameraName.c_str(), !mSelectedCamera))
        {
            mSelectedCamera = std::nullopt;
        }
        if (!mSelectedCamera)
        {
            ImGui::SetItemDefaultFocus();
        }

        for (int id = 0; id < mCameraInstances.size(); ++id)
        {
            const bool isSelected = (mSelectedCamera && *mSelectedCamera == id);
            if (ImGui::Selectable(nameCamera(id).c_str(), isSelected))
            {
                mSelectedCamera = id;
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (!mSelectedCamera)
    {
        mCustomCamera.appendProjectionControls();
    }
}


void UserCamera::appendProjectionControls()
{
    std::string distance{"Camera distance: "};
    distance += std::to_string(mPosition.r);
    ImGui::Text(distance.c_str());

    auto nameProjection = [&]()
    {
        if(mPerspectiveProjection) return "Perspective";
        else return "Orthographic";
    };

    if (ImGui::BeginCombo("Projection", nameProjection()))
    {
        if (ImGui::Selectable("Perspective", mPerspectiveProjection))
        {
            mPerspectiveProjection = true;
        }

        if (mPerspectiveProjection)
        {
            ImGui::SetItemDefaultFocus();
        }

        if (ImGui::Selectable("Orhtographic", !mPerspectiveProjection))
        {
            mPerspectiveProjection = false;
        }

        if (!mPerspectiveProjection)
        {
            ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::SliderAngle("Vertical FOV", &mVerticalFov.data(), 1.f, 180.f, "%.2f");
    if(ImGui::InputFloat("Near plane", &mNearPlaneDistance, 0.1f, 1.0f, "%.2f"))
    {
        mNearPlaneDistance = std::clamp(mNearPlaneDistance, 0.01f, 100.f);
    }
    if(ImGui::InputFloat("Viewed depth", &mViewedDepth, 1.f, 10.f, "%.1f"))
    {
        mViewedDepth = std::clamp(mViewedDepth, 1.f, 1000000.f);
    }
}


} // namespace gltfviewer
} // namespace ad
