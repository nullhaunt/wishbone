#include "Dev/dev_DebugInput.hpp"

#include <imgui.h>

#include <nn/nn_Log.h>
#include <nns/dbgui.h>
#include <nns/dbgui/dbgui_GraphicsFrameworkInteroperability.h>
#include <nns/gfx/gfx_GraphicsFramework.h>

#if !NN_GFX_IS_TARGET_NVN
  #error Wishbone's development UI requires the Nintendo SDK NVN backend.
#endif

namespace
{
  constexpr int DisplayWidth  = 1280;
  constexpr int DisplayHeight = 720;
  constexpr int BufferCount   = 2;

  struct DevelopmentState
  {
    wishbone::dev::DebugInputStatus input;
    bool                            exitRequested   = false;
    bool                            showDemo        = false;
    bool                            showMetrics     = false;
    int                             buttonPresses   = 0;
    int                             testValue       = 50;
    float                           clearColor[ 3 ] = { 0.055f, 0.075f, 0.11f };
    char                            text[ 128 ]     = "Wishbone";
  };

  void InitializeGraphics( nns::gfx::GraphicsFramework & graphics )
  {
    using Framework = nns::gfx::GraphicsFramework;
    using Viewport  = nn::gfx::util::ScalableViewport;

    Framework::InitializeGraphicsSystem( 8 * 1024 * 1024 );

    Framework::FrameworkInfo info;
    info.SetDefault();

    info.SetDisplayWidth( DisplayWidth );
    info.SetDisplayHeight( DisplayHeight );
    info.SetSwapChainFormat( nn::gfx::ImageFormat_R8_G8_B8_A8_UnormSrgb );
    info.SetColorBufferFormat( nn::gfx::ImageFormat_R8_G8_B8_A8_UnormSrgb );
    info.SetDepthStencilBufferEnabled( false );
    info.SetBufferCount( BufferCount );
    info.SetSwapChainBufferCount( BufferCount );

    info.SetRootCommandBufferCommandMemorySize( 1024 * 1024 );
    info.SetMemoryPoolSize( Framework::MemoryPoolType_CommandBuffer, 4 * 1024 * 1024 );
    info.SetMemoryPoolSize( Framework::MemoryPoolType_ConstantBuffer, 1024 * 1024 );

    info.SetDescriptorPoolSlotCount( nn::gfx::DescriptorPoolType_BufferView, 16 );
    info.SetDescriptorPoolSlotCount( nn::gfx::DescriptorPoolType_TextureView, 1024 );
    info.SetDescriptorPoolSlotCount( nn::gfx::DescriptorPoolType_Sampler, 1024 );

    Viewport::WindowCoordinate coordinates;
    coordinates.SetWidth( static_cast<float>( DisplayWidth ) );
    coordinates.SetHeight( static_cast<float>( DisplayHeight ) );
    coordinates.SetOriginMode( Viewport::OriginMode_UpperLeft );

    info.SetPhysicalWindowCoordinate( coordinates );
    info.SetVirtualWindowCoordinate( coordinates );

    graphics.Initialize( info );
    graphics.SetFrameworkMode( Framework::FrameworkMode_Immediate );
  }

  void ConfigureDebugUi( nns::dbgui::InterfaceInfo * info, void * )
  {
    // No filesystem dependency or persistent layout for this initial test interface.
    info->SetIniFilePath( nullptr );
  }

  void Calculate( nns::gfx::GraphicsFramework *, void * userData )
  {
    auto & state = *static_cast<DevelopmentState *>( userData );

    nns::dbgui::NewFrame();

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

      ImGui::Text( "Controller: %s", state.input.controller );
      ImGui::Text( "Keyboard: %s | Mouse: %s | Touch: %s",
                   state.input.keyboard ? "connected" : "disconnected",
                   state.input.mouse ? "connected" : "disconnected",
                   state.input.touch ? "active" : "idle" );
      ImGui::Separator();

      if ( ImGui::Button( "Test button" ) )
      {
        ++state.buttonPresses;
        NN_LOG( "[Wishbone] ImGui button pressed: %d\n", state.buttonPresses );
      }

      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      ImGui::Text( "Count: %d", state.buttonPresses );

      ImGui::SliderInt( "Test value", &state.testValue, 0, 100 );
      ImGui::InputText( "Text input", state.text, sizeof( state.text ) );
      ImGui::ColorEdit3( "Background", state.clearColor );

      ImGui::Checkbox( "ImGui demo", &state.showDemo );
      ImGui::Checkbox( "ImGui metrics", &state.showMetrics );

      const auto & io = ImGui::GetIO();

      ImGui::Separator();
      ImGui::Text( "Frame: %.2f ms | %.1f FPS", io.DeltaTime * 1000.0f, io.Framerate );
      ImGui::Text( "UI captures mouse: %s | keyboard: %s",
                   io.WantCaptureMouse ? "yes" : "no",
                   io.WantCaptureKeyboard ? "yes" : "no" );

      if ( ImGui::Button( "Exit Wishbone" ) )
      {
        state.exitRequested = true;
      }
    }

    ImGui::End();

    if ( state.showDemo )
    {
      ImGui::SetNextWindowPos( ImVec2( 620, 24 ), ImGuiCond_FirstUseEver );
      ImGui::ShowDemoWindow( &state.showDemo );
    }

    if ( state.showMetrics )
    {
      ImGui::ShowMetricsWindow( &state.showMetrics );
    }
  }

  void Render( nns::gfx::GraphicsFramework * graphics, int bufferIndex, void * userData )
  {
    const auto & state = *static_cast<DevelopmentState *>( userData );

    graphics->BeginFrame( bufferIndex );

    auto * commands = graphics->GetRootCommandBuffer( bufferIndex );
    auto * target   = graphics->GetColorTargetView();

    commands->ClearColor( target, state.clearColor[ 0 ], state.clearColor[ 1 ], state.clearColor[ 2 ], 1.0f, nullptr );
    commands->SetRenderTargets( 1, &target, nullptr );

    nns::dbgui::Render( commands );

    commands->SetViewportScissorState( graphics->GetViewportScissorState() );
    graphics->EndFrame( bufferIndex );
  }
} // namespace

extern "C" void nnMain()
{
  NN_LOG( "[Wishbone] Initializing NVN development UI.\n" );

  nns::gfx::GraphicsFramework graphics;
  InitializeGraphics( graphics );

  wishbone::dev::InitializeDebugInput();

  nns::dbgui::GraphicsFrameworkInteroperabilityData debugUi = {};
  nns::dbgui::InitializeWithGraphicsFramework( &debugUi, &graphics, ConfigureDebugUi, nullptr );

  ImGui::StyleColorsDark();
  ImGui::GetIO().FontGlobalScale = 1.4f;
  ImGui::GetStyle().ScaleAllSizes( 1.25f );

  DevelopmentState state;
  graphics.SetCalculateCallback( Calculate, &state );
  graphics.SetMakeCommandCallback( Render, &state );

  NN_LOG( "[Wishbone] ImGui ready. D-pad: navigate; B: activate; A: back.\n" );

  while ( !state.exitRequested )
  {
    state.input = wishbone::dev::UpdateDebugInput( graphics.GetLayer() );
    graphics.ProcessFrame();
  }

  // The final frame still references the UI's GPU buffers and descriptors.
  graphics.QueueFinish();
  nns::dbgui::FinalizeWithGraphicsFramework( &debugUi, &graphics );
  graphics.Finalize();

  NN_LOG( "[Wishbone] Development UI shut down.\n" );
}