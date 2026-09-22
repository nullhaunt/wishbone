#pragma once

#include "Dev/dev_DebugInput.hpp"
#include "Dev/dev_MiiLab.hpp"

#include <nns/dbgui/dbgui_GraphicsFrameworkInteroperability.h>

namespace nn::vi
{
  class Layer;
}

namespace nns::gfx
{
  class GraphicsFramework;
}

namespace wishbone::mii
{
  class Catalog;
  class RenderResources;
} // namespace wishbone::mii

namespace wishbone::dev
{
  class DebugUi
  {
  public:
    void Initialize( nns::gfx::GraphicsFramework & graphics );
    void Finalize( nns::gfx::GraphicsFramework & graphics );

    void UpdateInput( nn::vi::Layer * layer );
    void BuildFrame( wishbone::mii::Catalog & catalog, const wishbone::mii::RenderResources & renderResources );
    void Render( nn::gfx::CommandBuffer * commands );

    const float * GetClearColor() const;
    bool          IsExitRequested() const;

  private:
    void BuildDevelopmentPanel();

    nns::dbgui::GraphicsFrameworkInteroperabilityData m_Interoperability = {};
    DebugInputStatus                                  m_InputStatus;
    MiiLab                                            m_MiiLab;
    bool                                              m_ExitRequested   = false;
    bool                                              m_ShowDemo        = false;
    bool                                              m_ShowMetrics     = false;
    int                                               m_ButtonPresses   = 0;
    int                                               m_TestValue       = 50;
    float                                             m_ClearColor[ 3 ] = { 0.055f, 0.075f, 0.11f };
    char                                              m_Text[ 128 ]     = "Wishbone";
  };
} // namespace wishbone::dev