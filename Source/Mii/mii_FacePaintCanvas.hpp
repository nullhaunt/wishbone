#pragma once

#include <nn/gfx.h>

#include <cstddef>
#include <cstdint>

namespace nns::gfx
{
  class GraphicsFramework;
}

namespace wishbone::mii
{
  struct FacePaintSettings
  {
    bool  enabled       = true;
    float position[ 2 ] = { 0.35f, 0.58f };
    float scale[ 2 ]    = { 0.38f, 0.38f };
    float rotation      = 0.0f;
    float color[ 4 ]    = { 1.0f, 1.0f, 1.0f, 0.9f };
  };

  class FacePaintCanvas
  {
  public:
    static constexpr int Width  = 256;
    static constexpr int Height = 512;

    FacePaintCanvas() = default;
    ~FacePaintCanvas();

    FacePaintCanvas( const FacePaintCanvas & )             = delete;
    FacePaintCanvas & operator=( const FacePaintCanvas & ) = delete;

    bool Initialize( nns::gfx::GraphicsFramework & graphics );
    void Finalize();

    bool                            IsReady() const;
    const nn::gfx::DescriptorSlot & GetTextureDescriptorSlot() const;

  private:
    void BuildDiagnosticPattern( std::uint8_t * pixels ) const;

    nns::gfx::GraphicsFramework * m_Graphics = nullptr;
    nn::gfx::Device *             m_Device   = nullptr;

    nn::gfx::Texture        m_Texture;
    nn::gfx::TextureView    m_TextureView;
    nn::gfx::DescriptorSlot m_TextureDescriptorSlot;

    std::ptrdiff_t m_TexturePoolOffset      = -1;
    int            m_TextureDescriptorIndex = -1;
    bool           m_TextureInitialized     = false;
    bool           m_TextureViewInitialized = false;
  };
} // namespace wishbone::mii