#include "Mii/mii_RenderResources.hpp"

#include <nn/nn_Assert.h>
#include <nn/nn_Log.h>

#include <cstdlib>

namespace wishbone::mii
{
  namespace
  {
    class ResourceLoaderScope
    {
    public:
      void Activate()
      {
        m_IsActive = true;
      }

      void Finalize()
      {
        if ( m_IsActive )
        {
          nn::mii::FinalizeResourceLoader();
          m_IsActive = false;
        }
      }

      ~ResourceLoaderScope()
      {
        Finalize();
      }

    private:
      bool m_IsActive = false;
    };

#if defined( NN_BUILD_CONFIG_OS_SUPPORTS_HORIZON )
    size_t AlignUp( size_t value, size_t alignment )
    {
      return ( value + alignment - 1 ) / alignment * alignment;
    }

    void * AllocateAligned( size_t size, size_t alignment )
    {
      return ::aligned_alloc( alignment, AlignUp( size, alignment ) );
    }
#endif
  } // namespace

  RenderResources::~RenderResources()
  {
    NN_SDK_ASSERT( !m_Resource.IsInitialized() );
    NN_SDK_ASSERT( m_TextureData == nullptr );
    NN_SDK_ASSERT( m_ShapeData == nullptr );
    NN_SDK_ASSERT( m_WorkingMemory == nullptr );
  }

  bool RenderResources::Initialize( nn::gfx::Device * device )
  {
    NN_SDK_ASSERT_NOT_NULL( device );

    if ( IsReady() )
    {
      return true;
    }

    ClearError();

#if !defined( NN_BUILD_CONFIG_OS_SUPPORTS_HORIZON )
    SetError( "Mii system resources require NX hardware" );
    return false;
#else
    nn::mii::ResourceInfo info;
    info.SetDefault();
    info.SetShapeQuality( nn::mii::ShapeQuality_Middle );
    info.SetTextureQuality( nn::mii::TextureQuality_Low );
    info.SetGammaType( nn::mii::GammaType_Srgb );

    ResourceLoaderScope loader;
    const nn::Result    loaderResult = nn::mii::InitializeResourceLoader();

    if ( loaderResult.IsFailure() )
    {
      SetError( "InitializeResourceLoader", loaderResult );
      return false;
    }

    loader.Activate();

    m_TextureDataSize = nn::mii::GetResourceTextureSize( info );
    m_ShapeDataSize   = nn::mii::GetResourceShapeSize( info );

    if ( m_TextureDataSize == 0 || m_ShapeDataSize == 0 )
    {
      SetError( "GetResourceTextureSize/GetResourceShapeSize" );
      loader.Finalize();
      ReleaseStorage();
      return false;
    }

    m_TextureData = AllocateAligned( m_TextureDataSize, nn::mii::ResourceMemoryAlignment );
    m_ShapeData   = AllocateAligned( m_ShapeDataSize, nn::mii::ResourceMemoryAlignment );

    if ( m_TextureData == nullptr || m_ShapeData == nullptr )
    {
      SetError( "Allocate Mii texture/shape data" );
      loader.Finalize();
      ReleaseStorage();
      return false;
    }

    const nn::Result textureResult = nn::mii::LoadResourceTexture( m_TextureData, m_TextureDataSize, info );

    if ( textureResult.IsFailure() )
    {
      SetError( "LoadResourceTexture", textureResult );
      loader.Finalize();
      ReleaseStorage();
      return false;
    }

    const nn::Result shapeResult = nn::mii::LoadResourceShape( m_ShapeData, m_ShapeDataSize, info );

    if ( shapeResult.IsFailure() )
    {
      SetError( "LoadResourceShape", shapeResult );
      loader.Finalize();
      ReleaseStorage();
      return false;
    }

    loader.Finalize();

    const size_t workingMemoryAlignment = nn::mii::Resource::CalculateMemoryAlignment(
      m_TextureData, m_TextureDataSize, m_ShapeData, m_ShapeDataSize, info );

    m_WorkingMemorySize =
      nn::mii::Resource::CalculateMemorySize( m_TextureData, m_TextureDataSize, m_ShapeData, m_ShapeDataSize, info );

    if ( workingMemoryAlignment == 0 || m_WorkingMemorySize == 0 )
    {
      SetError( "Resource::CalculateMemorySize/CalculateMemoryAlignment" );
      ReleaseStorage();
      return false;
    }

    m_WorkingMemory = AllocateAligned( m_WorkingMemorySize, workingMemoryAlignment );

    if ( m_WorkingMemory == nullptr )
    {
      SetError( "Allocate Mii resource working memory" );
      ReleaseStorage();
      return false;
    }

    const nn::Result resourceResult = m_Resource.Initialize( m_WorkingMemory,
                                                             m_WorkingMemorySize,
                                                             m_TextureData,
                                                             m_TextureDataSize,
                                                             m_ShapeData,
                                                             m_ShapeDataSize,
                                                             info,
                                                             device );

    if ( resourceResult.IsFailure() )
    {
      SetError( "Resource::Initialize", resourceResult );
      ReleaseStorage();
      return false;
    }

    m_Device = device;
    ClearError();

    NN_LOG( "[Wishbone] Mii rendering resources ready (texture: %zu, shape: %zu, working: %zu bytes).\n",
            m_TextureDataSize,
            m_ShapeDataSize,
            m_WorkingMemorySize );

    return true;
#endif
  }

  void RenderResources::Finalize()
  {
    if ( m_Resource.IsInitialized() )
    {
      NN_SDK_ASSERT_NOT_NULL( m_Device );
      m_Resource.Finalize( m_Device );
    }

    m_Device = nullptr;
    ReleaseStorage();
  }

  bool RenderResources::IsSupported() const
  {
#if defined( NN_BUILD_CONFIG_OS_SUPPORTS_HORIZON )
    return true;
#else
    return false;
#endif
  }

  bool RenderResources::IsReady() const
  {
    return m_Resource.IsInitialized();
  }

  bool RenderResources::HasError() const
  {
    return m_ErrorOperation != nullptr;
  }

  bool RenderResources::HasResultCode() const
  {
    return m_HasResultCode;
  }

  const nn::mii::Resource & RenderResources::GetResource() const
  {
    NN_SDK_ASSERT( IsReady() );
    return m_Resource;
  }

  size_t RenderResources::GetTextureDataSize() const
  {
    return m_TextureDataSize;
  }

  size_t RenderResources::GetShapeDataSize() const
  {
    return m_ShapeDataSize;
  }

  size_t RenderResources::GetWorkingMemorySize() const
  {
    return m_WorkingMemorySize;
  }

  const char * RenderResources::GetErrorOperation() const
  {
    return m_ErrorOperation;
  }

  int RenderResources::GetErrorModule() const
  {
    return m_ErrorModule;
  }

  int RenderResources::GetErrorDescription() const
  {
    return m_ErrorDescription;
  }

  void RenderResources::ReleaseStorage()
  {
    ::free( m_WorkingMemory );
    ::free( m_ShapeData );
    ::free( m_TextureData );

    m_WorkingMemory     = nullptr;
    m_ShapeData         = nullptr;
    m_TextureData       = nullptr;
    m_WorkingMemorySize = 0;
    m_ShapeDataSize     = 0;
    m_TextureDataSize   = 0;
  }

  void RenderResources::ClearError()
  {
    m_ErrorOperation   = nullptr;
    m_ErrorModule      = 0;
    m_ErrorDescription = 0;
    m_HasResultCode    = false;
  }

  void RenderResources::SetError( const char * operation )
  {
    m_ErrorOperation   = operation;
    m_ErrorModule      = 0;
    m_ErrorDescription = 0;
    m_HasResultCode    = false;

    NN_LOG( "[Wishbone] %s failed.\n", operation );
  }

  void RenderResources::SetError( const char * operation, nn::Result result )
  {
    m_ErrorOperation   = operation;
    m_ErrorModule      = result.GetModule();
    m_ErrorDescription = result.GetDescription();
    m_HasResultCode    = true;

    NN_LOG( "[Wishbone] %s failed (%d-%d).\n", operation, m_ErrorModule, m_ErrorDescription );
  }
} // namespace wishbone::mii