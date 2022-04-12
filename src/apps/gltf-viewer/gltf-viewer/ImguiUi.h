#pragma once


#include <imgui.h>

#include <graphics/ApplicationGlfw.h>


namespace ad {
namespace gltfviewer {


class ImguiUi
{
public:
    ImguiUi(graphics::ApplicationGlfw & aApplication);

    ~ImguiUi();

    void startFrame() const;
    void render() const;

    bool isCapturingKeyboard() const;
    bool isCapturingMouse() const;

private:
    // Could be pimpled
    ImGuiIO & mIo;
};

} // namespace gltfviewer
} // namespace ad
