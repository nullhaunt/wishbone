#include "App/app_Application.hpp"

#include "Dev/dev_DebugUi.hpp"
#include "Mii/mii_Catalog.hpp"
#include "Mii/mii_FixedModelRenderer.hpp"
#include "Mii/mii_RenderResources.hpp"

#include <nn/nn_Log.h>
#include <nns/gfx/gfx_GraphicsFramework.h>

#if !NN_GFX_IS_TARGET_NVN
  #error Wishbone's development UI requires the Nintendo SDK NVN backend.
#endif

namespace wishbone::app
{
  namespace
  {
    constexpr int DisplayWidth  = 1280;
    constexpr int DisplayHeight = 720;
    constexpr int BufferCount   = 2;

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
      info.SetDepthStencilBufferEnabled( true );
      info.SetDepthStencilBufferFormat( nn::gfx::ImageFormat_D24_Unorm_S8_Uint );
      info.SetBufferCount( BufferCount );
      info.SetSwapChainBufferCount( BufferCount );

      info.SetRootCommandBufferCommandMemorySize( 1024 * 1024 );
      info.SetMemoryPoolSize( Framework::MemoryPoolType_CommandBuffer, 6 * 1024 * 1024 );
      info.SetMemoryPoolSize( Framework::MemoryPoolType_RenderTarget, 32 * 1024 * 1024 );
      info.SetMemoryPoolSize( Framework::MemoryPoolType_ConstantBuffer, 32 * 1024 * 1024 );
      info.SetMemoryPoolSize( Framework::MemoryPoolType_Data, 32 * 1024 * 1024 );
      info.SetMemoryPoolSize( Framework::MemoryPoolType_Others, 32 * 1024 * 1024 );

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

    class Application
    {
    public:
      void Run()
      {
        NN_LOG( "[Wishbone] Initializing NVN development UI.\n" );

        InitializeGraphics( m_Graphics );
        m_MiiRenderResources.Initialize( m_Graphics.GetDevice() );
        m_MiiCatalog.Initialize();

        if ( m_MiiCatalog.GetCount() > 0 )
        {
          m_FixedModelRenderer.Initialize(
            m_Graphics, m_MiiRenderResources, m_MiiCatalog.GetEntry( 0 ).m_CharInfo );
        }

        m_DebugUi.Initialize( m_Graphics );

        m_Graphics.SetCalculateCallback( Calculate, this );
        m_Graphics.SetMakeCommandCallback( Render, this );

        NN_LOG( "[Wishbone] ImGui ready. D-pad: navigate; B: activate; A: back.\n" );

        while ( !m_DebugUi.IsExitRequested() )
        {
          m_DebugUi.UpdateInput( m_Graphics.GetLayer() );
          m_Graphics.ProcessFrame();
        }

        // The final frame still references the UI's GPU buffers and descriptors.
        m_Graphics.QueueFinish();
        m_DebugUi.Finalize( m_Graphics );
        m_FixedModelRenderer.Finalize();
        m_MiiCatalog.Finalize();
        m_MiiRenderResources.Finalize();
        m_Graphics.Finalize();

        NN_LOG( "[Wishbone] Development UI shut down.\n" );
      }

    private:
      static void Calculate( nns::gfx::GraphicsFramework *, void * userData )
      {
        auto & application = *static_cast<Application *>( userData );
        application.m_DebugUi.BuildFrame(
          application.m_MiiCatalog, application.m_MiiRenderResources, application.m_FixedModelRenderer );
      }

      static void Render( nns::gfx::GraphicsFramework * graphics, int bufferIndex, void * userData )
      {
        auto & application = *static_cast<Application *>( userData );

        graphics->BeginFrame( bufferIndex );

        auto *       commands   = graphics->GetRootCommandBuffer( bufferIndex );
        auto *       target     = graphics->GetColorTargetView();
        auto *       depth      = graphics->GetDepthStencilView();
        const auto * clearColor = application.m_DebugUi.GetClearColor();

        commands->ClearColor( target, clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], 1.0f, nullptr );
        commands->ClearDepthStencil(
          depth, 1.0f, 0, nn::gfx::DepthStencilClearMode_DepthStencil, nullptr );
        commands->SetRenderTargets( 1, &target, depth );

        application.m_FixedModelRenderer.Draw( commands );

        application.m_DebugUi.Render( commands );

        commands->SetViewportScissorState( graphics->GetViewportScissorState() );
        graphics->EndFrame( bufferIndex );
      }

      nns::gfx::GraphicsFramework m_Graphics;
      mii::RenderResources        m_MiiRenderResources;
      mii::Catalog                m_MiiCatalog;
      mii::FixedModelRenderer     m_FixedModelRenderer;
      dev::DebugUi                m_DebugUi;
    };
  } // namespace

  void Run()
  {
    Application application;
    application.Run();
  }
} // namespace wishbone::app
