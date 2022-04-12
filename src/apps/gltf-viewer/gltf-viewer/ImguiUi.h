#pragma once


#include <imgui.h>

#include <graphics/ApplicationGlfw.h>


namespace ad {
namespace gltfviewer {


struct UserOptions;


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


void showUserOptionsWindow(UserOptions & aOptions);


} // namespace gltfviewer
} // namespace ad
