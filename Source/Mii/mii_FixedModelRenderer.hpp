#pragma once

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
                     const RenderResources &        resources,
                     const nn::mii::CharInfo &      charInfo );
    void Finalize();
    void Draw( nn::gfx::CommandBuffer * commands );

    bool IsSupported() const;
    bool IsReady() const;
    bool HasError() const;
    bool HasResultCode() const;

    const char * GetErrorOperation() const;
    int          GetErrorModule() const;
    int          GetErrorDescription() const;

  private:
    struct Impl;

    Impl * m_Impl = nullptr;
  };
} // namespace wishbone::mii
