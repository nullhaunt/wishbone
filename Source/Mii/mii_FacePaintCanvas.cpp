#include "Mii/mii_FacePaintCanvas.hpp"

#include <nns/gfx/gfx_GraphicsFramework.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wishbone::mii
{
  namespace
  {
    using Framework = nns::gfx::GraphicsFramework;

    constexpr nn::gfx::ImageFormat CanvasFormat = nn::gfx::ImageFormat_R8_G8_B8_A8_Unorm;
    constexpr size_t               PixelSize    = 4;
    constexpr size_t               CanvasSize   = FacePaintCanvas::Width * FacePaintCanvas::Height * PixelSize;
  } // namespace

  FacePaintCanvas::~FacePaintCanvas()
  {
    Finalize();
  }

  bool FacePaintCanvas::Initialize( nns::gfx::GraphicsFramework & graphics )
  {
    Finalize();

    m_Graphics = &graphics;
    m_Device   = graphics.GetDevice();

    nn::gfx::Texture::InfoType textureInfo;
    textureInfo.SetDefault();
    textureInfo.SetGpuAccessFlags( nn::gfx::GpuAccess_Texture );
    textureInfo.SetImageStorageDimension( nn::gfx::ImageStorageDimension_2d );
    textureInfo.SetTileMode( nn::gfx::TileMode_Linear );
    textureInfo.SetWidth( Width );
    textureInfo.SetHeight( Height );
    textureInfo.SetMipCount( 1 );
    textureInfo.SetImageFormat( CanvasFormat );

    const size_t textureSize      = nn::gfx::Texture::CalculateMipDataSize( m_Device, textureInfo );
    const size_t textureAlignment = nn::gfx::Texture::CalculateMipDataAlignment( m_Device, textureInfo );

    m_TexturePoolOffset = graphics.AllocatePoolMemory(
      Framework::MemoryPoolType_Others, textureSize, textureAlignment );

    if ( m_TexturePoolOffset < 0 )
    {
      Finalize();
      return false;
    }

    m_Texture.Initialize( m_Device,
                          textureInfo,
                          graphics.GetMemoryPool( Framework::MemoryPoolType_Others ),
                          m_TexturePoolOffset,
                          textureSize );
    m_TextureInitialized = true;

    nn::gfx::TextureView::InfoType viewInfo;
    viewInfo.SetDefault();
    viewInfo.SetImageDimension( nn::gfx::ImageDimension_2d );
    viewInfo.SetImageFormat( CanvasFormat );
    viewInfo.SetTexturePtr( &m_Texture );
    m_TextureView.Initialize( m_Device, viewInfo );
    m_TextureViewInitialized = true;

    m_TextureDescriptorIndex = graphics.AllocateDescriptorSlot( nn::gfx::DescriptorPoolType_TextureView, 1 );

    if ( m_TextureDescriptorIndex < 0 )
    {
      Finalize();
      return false;
    }

    graphics.SetTextureViewToDescriptorPool( m_TextureDescriptorIndex, &m_TextureView );
    graphics.GetDescriptorPool( nn::gfx::DescriptorPoolType_TextureView )
      ->GetDescriptorSlot( &m_TextureDescriptorSlot, m_TextureDescriptorIndex );

    nn::gfx::BufferInfo uploadInfo;
    uploadInfo.SetDefault();
    uploadInfo.SetGpuAccessFlags( nn::gfx::GpuAccess_Read );
    uploadInfo.SetSize( CanvasSize );

    const ptrdiff_t uploadPoolOffset = graphics.AllocatePoolMemory(
      Framework::MemoryPoolType_ConstantBuffer,
      uploadInfo.GetSize(),
      nn::gfx::Buffer::GetBufferAlignment( m_Device, uploadInfo ) );

    if ( uploadPoolOffset < 0 )
    {
      Finalize();
      return false;
    }

    nn::gfx::Buffer uploadBuffer;
    uploadBuffer.Initialize( m_Device,
                             uploadInfo,
                             graphics.GetMemoryPool( Framework::MemoryPoolType_ConstantBuffer ),
                             uploadPoolOffset,
                             uploadInfo.GetSize() );

    std::uint8_t * pixels = uploadBuffer.Map<std::uint8_t>();
    BuildDiagnosticPattern( pixels );
    uploadBuffer.FlushMappedRange( 0, CanvasSize );
    uploadBuffer.Unmap();

    Framework::CommandBuffer commandBuffer = {};
    nn::gfx::CommandBuffer::InfoType commandInfo;
    commandInfo.SetDefault();
    commandInfo.SetQueueCapability( nn::gfx::QueueCapability_Graphics );
    commandInfo.SetCommandBufferType( nn::gfx::CommandBufferType_Direct );

    graphics.InitializeCommandBuffer( &commandBuffer, commandInfo, 256 * 1024, 1024 );
    graphics.ResetCommandBuffer( &commandBuffer );
    commandBuffer.object.Begin();

    nn::gfx::TextureCopyRegion destination;
    destination.SetDefault();
    destination.SetWidth( Width );
    destination.SetHeight( Height );
    destination.SetDepth( 1 );

    commandBuffer.object.CopyBufferToImage( &m_Texture, destination, &uploadBuffer, 0 );
    commandBuffer.object.End();

    graphics.GetQueue()->ExecuteCommand( &commandBuffer.object, nullptr );
    graphics.GetQueue()->Sync();

    graphics.FinalizeCommandBuffer( &commandBuffer );
    uploadBuffer.Finalize( m_Device );
    graphics.FreePoolMemory( Framework::MemoryPoolType_ConstantBuffer, uploadPoolOffset );

    return true;
  }

  void FacePaintCanvas::Finalize()
  {
    if ( m_Graphics == nullptr )
    {
      return;
    }

    if ( m_TextureDescriptorIndex >= 0 )
    {
      m_Graphics->FreeDescriptorSlot( nn::gfx::DescriptorPoolType_TextureView, m_TextureDescriptorIndex );
      m_TextureDescriptorIndex = -1;
    }

    if ( m_TextureViewInitialized )
    {
      m_TextureView.Finalize( m_Device );
      m_TextureViewInitialized = false;
    }

    if ( m_TextureInitialized )
    {
      m_Texture.Finalize( m_Device );
      m_TextureInitialized = false;
    }

    if ( m_TexturePoolOffset >= 0 )
    {
      m_Graphics->FreePoolMemory( Framework::MemoryPoolType_Others, m_TexturePoolOffset );
      m_TexturePoolOffset = -1;
    }

    m_Device   = nullptr;
    m_Graphics = nullptr;
  }

  bool FacePaintCanvas::IsReady() const
  {
    return m_TextureInitialized && m_TextureViewInitialized && m_TextureDescriptorIndex >= 0;
  }

  const nn::gfx::DescriptorSlot & FacePaintCanvas::GetTextureDescriptorSlot() const
  {
    return m_TextureDescriptorSlot;
  }

  void FacePaintCanvas::BuildDiagnosticPattern( std::uint8_t * pixels ) const
  {
    std::memset( pixels, 0, CanvasSize );

    for ( int y = 0; y < Height; ++y )
    {
      const float v = ( static_cast<float>( y ) + 0.5f ) / static_cast<float>( Height );

      for ( int x = 0; x < Width; ++x )
      {
        const float u = ( static_cast<float>( x ) + 0.5f ) / static_cast<float>( Width );

        const float dotX = u - 0.32f;
        const float dotY = v - 0.38f;
        const bool  dot  = dotX * dotX + dotY * dotY < 0.0064f;

        const bool shaft = u >= 0.30f && u <= 0.62f && v >= 0.47f && v <= 0.55f;
        const bool arrow = u >= 0.56f && u <= 0.78f &&
                           ( v >= 0.51f - ( 0.78f - u ) * 0.7f ) &&
                           ( v <= 0.51f + ( 0.78f - u ) * 0.7f );

        if ( !dot && !shaft && !arrow )
        {
          continue;
        }

        const size_t pixel = ( static_cast<size_t>( y ) * Width + x ) * PixelSize;

        if ( dot )
        {
          pixels[ pixel + 0 ] = 30;
          pixels[ pixel + 1 ] = 210;
          pixels[ pixel + 2 ] = 255;
          pixels[ pixel + 3 ] = 235;
        }
        else
        {
          pixels[ pixel + 0 ] = 255;
          pixels[ pixel + 1 ] = 70;
          pixels[ pixel + 2 ] = 145;
          pixels[ pixel + 3 ] = 235;
        }
      }
    }
  }
} // namespace wishbone::mii
