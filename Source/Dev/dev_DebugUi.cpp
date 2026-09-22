#include "Dev/dev_DebugUi.hpp"

#include "Mii/mii_Catalog.hpp"
#include "Mii/mii_FixedModelRenderer.hpp"
#include "Mii/mii_RenderResources.hpp"

#include <imgui.h>

#include <nn/nn_Log.h>
#include <nns/dbgui.h>
#include <nns/gfx/gfx_GraphicsFramework.h>

namespace wishbone::dev
{
  namespace
  {
    void Configure( nns::dbgui::InterfaceInfo * info, void * )
    {
      // No filesystem dependency or persistent layout for this initial test interface.
      info->SetIniFilePath( nullptr );
    }
  } // namespace

  void DebugUi::Initialize( nns::gfx::GraphicsFramework & graphics )
  {
    InitializeDebugInput();
    nns::dbgui::InitializeWithGraphicsFramework( &m_Interoperability, &graphics, Configure, nullptr );

    ImGui::StyleColorsDark();
    ImGui::GetIO().FontGlobalScale = 1.4f;
    ImGui::GetStyle().ScaleAllSizes( 1.25f );
  }

  void DebugUi::Finalize( nns::gfx::GraphicsFramework & graphics )
  {
    nns::dbgui::FinalizeWithGraphicsFramework( &m_Interoperability, &graphics );
  }

  void DebugUi::UpdateInput( nn::vi::Layer * layer )
  {
    m_InputStatus = UpdateDebugInput( layer );
  }

  void DebugUi::BuildFrame( wishbone::mii::Catalog &                  catalog,
                            const wishbone::mii::RenderResources &    renderResources,
                            const wishbone::mii::FixedModelRenderer & fixedModelRenderer )
  {
    nns::dbgui::NewFrame();
    BuildDevelopmentPanel();
    m_MiiLab.Build( catalog, renderResources, fixedModelRenderer );

    if ( m_ShowDemo )
    {
      ImGui::SetNextWindowPos( ImVec2( 620, 24 ), ImGuiCond_FirstUseEver );
      ImGui::ShowDemoWindow( &m_ShowDemo );
    }

    if ( m_ShowMetrics )
    {
      ImGui::ShowMetricsWindow( &m_ShowMetrics );
    }
  }

  void DebugUi::BuildDevelopmentPanel()
  {
    ImGui::SetNextWindowPos( ImVec2( 24, 24 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 570, 600 ), ImGuiCond_FirstUseEver );

    if ( ImGui::Begin( "Wishbone | Development" ) )
    {
      ImGui::Text( "Dear ImGui %s | Nintendo SDK | NVN", IMGUI_VERSION );
      ImGui::TextUnformatted( "Input and rendering check" );
      ImGui::Separator();

      ImGui::TextWrapped( "D-pad: navigate. B: activate. A: back. X: text input. "
                          "Hold Y to move/switch windows. L/R: slow/fast adjustment." );
      ImGui::TextWrapped( "USB mouse/keyboard and handheld touch are also supported." );
      ImGui::Spacing();

      ImGui::Text( "Controller: %s", m_InputStatus.controller );
      ImGui::Text( "Keyboard: %s | Mouse: %s | Touch: %s",
                   m_InputStatus.keyboard ? "connected" : "disconnected",
                   m_InputStatus.mouse ? "connected" : "disconnected",
                   m_InputStatus.touch ? "active" : "idle" );
      ImGui::Separator();

      if ( ImGui::Button( "Test button" ) )
      {
        ++m_ButtonPresses;
        NN_LOG( "[Wishbone] ImGui button pressed: %d\n", m_ButtonPresses );
      }

      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      ImGui::Text( "Count: %d", m_ButtonPresses );

      ImGui::SliderInt( "Test value", &m_TestValue, 0, 100 );
      ImGui::InputText( "Text input", m_Text, sizeof( m_Text ) );
      ImGui::ColorEdit3( "Background", m_ClearColor );

      bool showMiiLab = m_MiiLab.IsVisible();

      if ( ImGui::Checkbox( "Mii Lab", &showMiiLab ) )
      {
        m_MiiLab.SetVisible( showMiiLab );
      }

      ImGui::Checkbox( "ImGui demo", &m_ShowDemo );
      ImGui::Checkbox( "ImGui metrics", &m_ShowMetrics );

      const auto & io = ImGui::GetIO();

      ImGui::Separator();
      ImGui::Text( "Frame: %.2f ms | %.1f FPS", io.DeltaTime * 1000.0f, io.Framerate );
      ImGui::Text( "UI captures mouse: %s | keyboard: %s",
                   io.WantCaptureMouse ? "yes" : "no",
                   io.WantCaptureKeyboard ? "yes" : "no" );

      if ( ImGui::Button( "Exit Wishbone" ) )
      {
        m_ExitRequested = true;
      }
    }

    ImGui::End();
  }

  void DebugUi::Render( nn::gfx::CommandBuffer * commands )
  {
    nns::dbgui::Render( commands );
  }

  const float * DebugUi::GetClearColor() const
  {
    return m_ClearColor;
  }

  bool DebugUi::IsExitRequested() const
  {
    return m_ExitRequested;
  }

  int DebugUi::GetSelectedMii() const
  {
    return m_MiiLab.GetSelectedMii();
  }
} // namespace wishbone::dev
