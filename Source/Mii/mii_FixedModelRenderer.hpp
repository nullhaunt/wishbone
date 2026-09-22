#pragma once

#include "Mii/mii_FacePaintCanvas.hpp"

#include <nn/gfx.h>
#include <nn/mii/mii_Common.h>

namespace nns::gfx
{
  class GraphicsFramework;
}

namespace wishbone::mii
{
  class RenderResources;

  class FixedModelRenderer
  {
  public:
    FixedModelRenderer() = default;
    ~FixedModelRenderer();

    FixedModelRenderer( const FixedModelRenderer & )             = delete;
    FixedModelRenderer & operator=( const FixedModelRenderer & ) = delete;

    bool Initialize( nns::gfx::GraphicsFramework & graphics,
                     const RenderResources &       resources,
                     const nn::mii::CharInfo &     charInfo,
                     int                           frameCount );
    void Finalize();
    void Draw( nn::gfx::CommandBuffer * commands, int frameIndex );

    bool SetExpression( nn::mii::Expression expression );
    void SetFacePaintSettings( const FacePaintSettings & settings );

    bool IsSupported() const;
    bool IsReady() const;
    bool HasError() const;
    bool HasResultCode() const;

    nn::mii::Expression       GetExpression() const;
    int                       GetExpressionMaskSlot() const;
    const FacePaintSettings & GetFacePaintSettings() const;

    const char * GetErrorOperation() const;
    int          GetErrorModule() const;
    int          GetErrorDescription() const;

  private:
    struct Impl;

    Impl *            m_Impl = nullptr;
    FacePaintSettings m_FacePaintSettings;
  };
} // namespace wishbone::mii
