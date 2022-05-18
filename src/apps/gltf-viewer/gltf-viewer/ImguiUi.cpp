#include "ImguiUi.h"

#include "UserOptions.h"

#include <GLFW/glfw3.h>

#include <imgui_backends/imgui_impl_glfw.h>
#include <imgui_backends/imgui_impl_opengl3.h>


namespace {

    ImGuiIO & initializeImgui(GLFWwindow * aWindow)
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(aWindow, true);
        ImGui_ImplOpenGL3_Init();

        return io;
    }


    void terminateImgui()
    {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

} // anonymous namespace


namespace ad {
namespace gltfviewer {


ImguiUi::ImguiUi(graphics::ApplicationGlfw & aApplication) :
    mIo{initializeImgui(aApplication.getGlfwWindow())}
{}


ImguiUi::~ImguiUi()
{
    terminateImgui();
}


void ImguiUi::startFrame() const
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}


void ImguiUi::render() const
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


bool ImguiUi::isCapturingKeyboard() const
{
    return mIo.WantCaptureKeyboard;
}


bool ImguiUi::isCapturingMouse() const
{
    return mIo.WantCaptureMouse;
}


void showUserOptionsWindow(UserOptions & aOptions)
{
    ImGui::Begin("Rendering options");
    ImGui::Checkbox("Show skeletons", &aOptions.showSkeletons);
    ImGui::Checkbox("Show Imgui demo window", &aOptions.showImguiDemo);
    ImGui::End();

    if(aOptions.showImguiDemo)
    {
        ImGui::ShowDemoWindow(&aOptions.showImguiDemo);
    }
}


} // namespace gltfviewer
} // namespace ad
