#include "Mii/mii_FixedModelRenderer.hpp"

#include "Mii/mii_RenderResources.hpp"

#include <nn/fs.h>
#include <nn/gfx.h>
#include <nn/mii.h>
#include <nn/nn_Assert.h>
#include <nn/nn_Log.h>
#include <nn/util/util_BinaryFormat.h>
#include <nn/util/util_Matrix.h>

#include <nns/gfx/gfx_GraphicsFramework.h>

#include <cstdlib>
#include <new>

namespace wishbone::mii
{
  namespace
  {
    constexpr size_t TextureResolution = 512;
    constexpr int    TextureMipCount   = 1;
    constexpr int    ExpressionFlags   = nn::mii::ExpressionFlag_Normal;
    constexpr int    TextureSlotCount  = 64;
    constexpr bool   IsVerticalFlip    = true;

    constexpr nn::gfx::ImageFormat TextureFormat = nn::gfx::ImageFormat_R8_G8_B8_A8_Unorm;

    constexpr int CreateFlags = nn::mii::CreateFlag_Normal | nn::mii::CreateFlag_NoseNormal;

    constexpr nn::mii::CreateModelType CreateModelType = nn::mii::CreateModelType_Normal;
    constexpr nn::mii::CreateNoseType  CreateNoseType  = nn::mii::CreateNoseType_Normal;

    constexpr const char * TextureShaderPath = "Contents:/Wishbone/Mii/TextureShader.bnsh";
    constexpr const char * ModelShaderPath   = "Contents:/Wishbone/Mii/MiiModel.bnsh";

    struct ConstantBufferMatrix
    {
      nn::util::FloatColumnMajor4x3 modelView;
      nn::util::FloatColumnMajor4x4 projection;
    };

    struct ConstantBufferModulate
    {
      int32_t          modulateType;
      int32_t          padding[ 3 ];
      nn::util::Float4 constantColor[ nn::mii::DrawParam::ConstantColorNum ];
    };

    nn::gfx::ShaderCodeType FindShaderCodeType( const nn::gfx::ResShaderContainer & container )
    {
      for ( int i = 0; i < nn::gfx::ShaderCodeType_End; ++i )
      {
        const auto type = static_cast<nn::gfx::ShaderCodeType>( i );

        if ( container.GetResShaderVariation( 0 )->GetResShaderProgram( type ) != nullptr )
        {
          return type;
        }
      }

      return nn::gfx::ShaderCodeType_End;
    }
  } // namespace

  struct FixedModelRenderer::Impl
  {
    using Framework = nns::gfx::GraphicsFramework;

    enum DrawMode
    {
      DrawMode_Opaque,
      DrawMode_Translucent,
      DrawMode_Count
    };

    struct ShaderResource
    {
      void *                      memory   = nullptr;
      nn::gfx::ResShaderFile *    file     = nullptr;
      nn::gfx::ShaderCodeType     codeType = nn::gfx::ShaderCodeType_End;
      bool                        initialized = false;
    };

    Framework *     graphics = nullptr;
    nn::gfx::Device * device  = nullptr;

    nn::mii::CharModel          charModel;
    nn::mii::Faceline           faceline;
    nn::mii::Mask               mask;
    nn::mii::FacelineGpuBuffer  facelineGpuBuffer;
    nn::mii::MaskGpuBuffer      maskGpuBuffer;
    nn::mii::TextureShader      textureShader;
    nn::mii::TextureShaderInfo  textureShaderInfo;

    nn::gfx::Sampler           sampler;
    nn::gfx::DescriptorSlot    samplerSlot;
    int                        samplerSlotIndex = -1;
    int                        textureSlotIndex = -1;
    int                        usedTextureSlots = 0;

    ShaderResource textureShaderResource;
    ShaderResource modelShaderResource;

    nn::gfx::RasterizerState  rasterizerStates[ nn::gfx::CullMode_End ];
    nn::gfx::BlendState       blendStates[ DrawMode_Count ];
    nn::gfx::DepthStencilState depthStates[ DrawMode_Count ];
    nn::gfx::VertexState      vertexState;

    void * blendStateMemory[ DrawMode_Count ] = {};
    void * vertexStateMemory                  = nullptr;
    int    rasterizerStateCount               = 0;
    int    blendStateCount                    = 0;
    int    depthStateCount                    = 0;
    bool   vertexStateInitialized             = false;
    bool   samplerInitialized                 = false;

    nn::gfx::Buffer matrixBuffer;
    nn::gfx::Buffer modulateBuffers[ nn::mii::CharModel::DrawType_End ];
    ptrdiff_t       matrixBufferOffset = -1;
    ptrdiff_t       modulateBufferOffsets[ nn::mii::CharModel::DrawType_End ];
    int             modulateBufferCount = 0;
    bool            matrixBufferInitialized = false;

    void * textureShaderMemory = nullptr;
    void * charModelMemory     = nullptr;
    void * facelineMemory      = nullptr;
    void * maskMemory          = nullptr;
    void * facelineGpuMemory   = nullptr;
    void * maskGpuMemory       = nullptr;

    ptrdiff_t charDynamicPoolOffset = -1;
    ptrdiff_t charStaticPoolOffset  = -1;
    ptrdiff_t facelinePoolOffset    = -1;
    ptrdiff_t maskPoolOffset        = -1;
    ptrdiff_t facelineGpuPoolOffset = -1;
    ptrdiff_t maskGpuPoolOffset     = -1;

    int  matrixSlot   = -1;
    int  modulateSlot = -1;
    int  samplerShaderSlot = -1;
    bool ready = false;

    const char * errorOperation   = nullptr;
    int          errorModule      = 0;
    int          errorDescription = 0;
    bool         hasResultCode    = false;

    Impl()
    {
      for ( ptrdiff_t & offset : modulateBufferOffsets )
      {
        offset = -1;
      }
    }

    void SetError( const char * operation )
    {
      errorOperation   = operation;
      errorModule      = 0;
      errorDescription = 0;
      hasResultCode    = false;

      NN_LOG( "[Wishbone] %s failed.\n", operation );
    }

    void SetError( const char * operation, nn::Result result )
    {
      errorOperation   = operation;
      errorModule      = result.GetModule();
      errorDescription = result.GetDescription();
      hasResultCode    = true;

      NN_LOG( "[Wishbone] %s failed (%d-%d).\n", operation, errorModule, errorDescription );
    }

    void * Allocate( size_t size, size_t alignment, const char * operation )
    {
      void * memory = graphics->AllocateMemory( size, alignment );

      if ( memory == nullptr )
      {
        SetError( operation );
      }

      return memory;
    }

    bool LoadShaderResource( ShaderResource * resource, const char * path )
    {
      nn::fs::FileHandle handle;
      nn::Result result = nn::fs::OpenFile( &handle, path, nn::fs::OpenMode_Read );

      if ( result.IsFailure() )
      {
        SetError( path, result );
        return false;
      }

      int64_t fileSize = 0;
      result = nn::fs::GetFileSize( &fileSize, handle );

      if ( result.IsFailure() )
      {
        nn::fs::CloseFile( handle );
        SetError( "Get shader file size", result );
        return false;
      }

      nn::util::BinaryFileHeader header = {};
      result = nn::fs::ReadFile( handle, 0, &header, sizeof( header ) );

      if ( result.IsFailure() )
      {
        nn::fs::CloseFile( handle );
        SetError( "Read shader header", result );
        return false;
      }

      resource->memory = Allocate( static_cast<size_t>( fileSize ), header.GetAlignment(), "Allocate shader file" );

      if ( resource->memory == nullptr )
      {
        nn::fs::CloseFile( handle );
        return false;
      }

      result = nn::fs::ReadFile( handle, 0, resource->memory, static_cast<size_t>( fileSize ) );
      nn::fs::CloseFile( handle );

      if ( result.IsFailure() )
      {
        SetError( "Read shader file", result );
        return false;
      }

      resource->file = nn::gfx::ResShaderFile::ResCast( resource->memory );

      if ( resource->file == nullptr )
      {
        SetError( "ResShaderFile::ResCast" );
        return false;
      }

      nn::gfx::ResShaderContainer * container = resource->file->GetShaderContainer();
      resource->codeType = FindShaderCodeType( *container );

      if ( resource->codeType == nn::gfx::ShaderCodeType_End )
      {
        SetError( "Find shader code type" );
        return false;
      }

      container->Initialize( device );

      for ( int i = 0; i < container->GetShaderVariationCount(); ++i )
      {
        auto * program = container->GetResShaderVariation( i )->GetResShaderProgram( resource->codeType );

        if ( program->Initialize( device ) != nn::gfx::ShaderInitializeResult_Success )
        {
          SetError( "ResShaderProgram::Initialize" );
          return false;
        }
      }

      resource->initialized = true;
      return true;
    }

    void FinalizeShaderResource( ShaderResource * resource )
    {
      if ( resource->initialized )
      {
        auto * container = resource->file->GetShaderContainer();

        for ( int i = 0; i < container->GetShaderVariationCount(); ++i )
        {
          container->GetResShaderVariation( i )->GetResShaderProgram( resource->codeType )->Finalize( device );
        }

        container->Finalize( device );
        resource->initialized = false;
      }

      if ( resource->memory != nullptr )
      {
        graphics->FreeMemory( resource->memory );
        resource->memory = nullptr;
        resource->file   = nullptr;
      }
    }

    bool LoadShaders()
    {
      size_t cacheSize = 0;
      nn::Result result = nn::fs::QueryMountRomCacheSize( &cacheSize );

      if ( result.IsFailure() )
      {
        SetError( "QueryMountRomCacheSize", result );
        return false;
      }

      void * cache = ::malloc( cacheSize );

      if ( cache == nullptr )
      {
        SetError( "Allocate ROM cache" );
        return false;
      }

      result = nn::fs::MountRom( "Contents", cache, cacheSize );

      if ( result.IsFailure() )
      {
        ::free( cache );
        SetError( "MountRom", result );
        return false;
      }

      const bool loaded = LoadShaderResource( &textureShaderResource, TextureShaderPath )
                       && LoadShaderResource( &modelShaderResource, ModelShaderPath );

      nn::fs::Unmount( "Contents" );
      ::free( cache );

      return loaded;
    }

    bool InitializeTextureShader()
    {
      textureShaderInfo.SetFacelineImageFormat( TextureFormat );
      textureShaderInfo.SetMaskImageFormat( TextureFormat );

      const size_t alignment = nn::mii::TextureShader::CalculateMemoryAlignment(
        *textureShaderResource.file, textureShaderResource.codeType, textureShaderInfo, device );
      const size_t size = nn::mii::TextureShader::CalculateMemorySize(
        *textureShaderResource.file, textureShaderResource.codeType, textureShaderInfo, device );

      textureShaderMemory = Allocate( size, alignment, "Allocate TextureShader memory" );

      if ( textureShaderMemory == nullptr )
      {
        return false;
      }

      const nn::Result result = textureShader.Initialize( textureShaderMemory,
                                                          size,
                                                          device,
                                                          *textureShaderResource.file,
                                                          textureShaderResource.codeType,
                                                          textureShaderInfo );

      if ( result.IsFailure() )
      {
        SetError( "TextureShader::Initialize", result );
        return false;
      }

      return true;
    }

    bool InitializeCharModel( const nn::mii::Resource & resources, const nn::mii::CharInfo & charInfo )
    {
      nn::mii::CharModelInfo info;
      info.SetDefault();
      info.SetDynamicTextureResolution( TextureResolution, TextureResolution );
      info.SetCreateFlag( CreateFlags );
      info.SetDynamicTextureFormat( TextureFormat, TextureFormat );
      info.SetMaskCount( nn::mii::GetExpressionCount( ExpressionFlags ) );
      info.SetDynamicTextureMipCount( TextureMipCount, TextureMipCount );

      const size_t memorySize = nn::mii::CharModel::CalculateMemorySize( info );
      charModelMemory = Allocate(
        memorySize, nn::mii::CharModel::CalculateMemoryAlignment( info ), "Allocate CharModel memory" );

      auto * temporary = static_cast<nn::mii::TemporaryBuffer *>(
        Allocate( nn::mii::TemporaryBufferSize, alignof( nn::mii::TemporaryBuffer ), "Allocate CharModel temporary memory" ) );

      if ( charModelMemory == nullptr || temporary == nullptr )
      {
        if ( temporary != nullptr )
        {
          graphics->FreeMemory( temporary );
        }

        return false;
      }

      const size_t dynamicAlignment = nn::mii::CharModel::CalculateDynamicMemoryPoolAlignment( device, info );
      const size_t dynamicSize      = nn::mii::CharModel::CalculateDynamicMemoryPoolSize( device, info );
      const size_t staticAlignment  = nn::mii::CharModel::CalculateStaticMemoryPoolAlignment( resources );
      const size_t staticSize       = nn::mii::CharModel::CalculateStaticMemoryPoolSize( resources );

      charDynamicPoolOffset = graphics->AllocatePoolMemory( Framework::MemoryPoolType_Others,
                                                            dynamicSize,
                                                            dynamicAlignment );
      charStaticPoolOffset = graphics->AllocatePoolMemory( Framework::MemoryPoolType_Data,
                                                           staticSize,
                                                           staticAlignment );

      const nn::Result result = charModel.Initialize( charModelMemory,
                                                      memorySize,
                                                      device,
                                                      graphics->GetMemoryPool( Framework::MemoryPoolType_Others ),
                                                      charDynamicPoolOffset,
                                                      dynamicSize,
                                                      graphics->GetMemoryPool( Framework::MemoryPoolType_Data ),
                                                      charStaticPoolOffset,
                                                      staticSize,
                                                      resources,
                                                      temporary,
                                                      info,
                                                      charInfo );

      graphics->FreeMemory( temporary );

      if ( result.IsFailure() )
      {
        SetError( "CharModel::Initialize", result );
        return false;
      }

      return true;
    }

    bool InitializeFaceline( const nn::mii::Resource & resources, const nn::mii::CharInfo & charInfo )
    {
      const size_t memorySize = nn::mii::Faceline::CalculateMemorySize();
      facelineMemory = Allocate(
        memorySize, nn::mii::Faceline::CalculateMemoryAlignment(), "Allocate Faceline memory" );

      auto * temporary = static_cast<nn::mii::TemporaryBuffer *>(
        Allocate( nn::mii::TemporaryBufferSize, alignof( nn::mii::TemporaryBuffer ), "Allocate Faceline temporary memory" ) );

      if ( facelineMemory == nullptr || temporary == nullptr )
      {
        if ( temporary != nullptr )
        {
          graphics->FreeMemory( temporary );
        }

        return false;
      }

      const size_t poolSize = nn::mii::Faceline::CalculateMemoryPoolSize( device, resources );
      facelinePoolOffset = graphics->AllocatePoolMemory(
        Framework::MemoryPoolType_Data,
        poolSize,
        nn::mii::Faceline::CalculateMemoryPoolAlignment( device, resources ) );

      const nn::Result result = faceline.Initialize( facelineMemory,
                                                     memorySize,
                                                     device,
                                                     graphics->GetMemoryPool( Framework::MemoryPoolType_Data ),
                                                     facelinePoolOffset,
                                                     poolSize,
                                                     temporary,
                                                     resources,
                                                     charInfo,
                                                     IsVerticalFlip );

      graphics->FreeMemory( temporary );

      if ( result.IsFailure() )
      {
        SetError( "Faceline::Initialize", result );
        return false;
      }

      return true;
    }

    bool InitializeMask( const nn::mii::Resource & resources, const nn::mii::CharInfo & charInfo )
    {
      const size_t memorySize = nn::mii::Mask::CalculateMemorySize();
      maskMemory = Allocate( memorySize, nn::mii::Mask::CalculateMemoryAlignment(), "Allocate Mask memory" );

      auto * temporary = static_cast<nn::mii::TemporaryBuffer *>(
        Allocate( nn::mii::TemporaryBufferSize, alignof( nn::mii::TemporaryBuffer ), "Allocate Mask temporary memory" ) );

      if ( maskMemory == nullptr || temporary == nullptr )
      {
        if ( temporary != nullptr )
        {
          graphics->FreeMemory( temporary );
        }

        return false;
      }

      const size_t poolSize = nn::mii::Mask::CalculateMemoryPoolSize( device, resources, ExpressionFlags );
      maskPoolOffset = graphics->AllocatePoolMemory(
        Framework::MemoryPoolType_Data,
        poolSize,
        nn::mii::Mask::CalculateMemoryPoolAlignment( device, resources, ExpressionFlags ) );

      const nn::Result result = mask.Initialize( maskMemory,
                                                 memorySize,
                                                 device,
                                                 graphics->GetMemoryPool( Framework::MemoryPoolType_Data ),
                                                 maskPoolOffset,
                                                 poolSize,
                                                 temporary,
                                                 resources,
                                                 charInfo,
                                                 ExpressionFlags,
                                                 IsVerticalFlip );

      graphics->FreeMemory( temporary );

      if ( result.IsFailure() )
      {
        SetError( "Mask::Initialize", result );
        return false;
      }

      return true;
    }

    bool BindDescriptors()
    {
      nn::gfx::Sampler::InfoType samplerInfo;
      nn::mii::GetSamplerInfo( &samplerInfo );
      sampler.Initialize( device, samplerInfo );
      samplerInitialized = true;

      samplerSlotIndex = graphics->AllocateDescriptorSlot( nn::gfx::DescriptorPoolType_Sampler, 1 );
      textureSlotIndex = graphics->AllocateDescriptorSlot( nn::gfx::DescriptorPoolType_TextureView, TextureSlotCount );

      if ( samplerSlotIndex < 0 || textureSlotIndex < 0 )
      {
        SetError( "Allocate Mii descriptor slots" );
        return false;
      }

      graphics->SetSamplerToDescriptorPool( samplerSlotIndex, &sampler );
      graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_Sampler )
        ->GetDescriptorSlot( &samplerSlot, samplerSlotIndex );

      for ( int i = 0; i < nn::mii::CharModel::TextureType_End; ++i )
      {
        const auto type = static_cast<nn::mii::CharModel::TextureType>( i );

        if ( type == nn::mii::CharModel::TextureType_Mask )
        {
          continue;
        }

        const nn::gfx::TextureView * view = charModel.GetTextureView( type, 0 );

        if ( view != nullptr )
        {
          nn::gfx::DescriptorSlot slot;
          const int index = textureSlotIndex + usedTextureSlots++;
          graphics->SetTextureViewToDescriptorPool( index, view );
          graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_TextureView )->GetDescriptorSlot( &slot, index );
          charModel.SetTextureDescriptorSlot( type, slot, 0 );
        }
      }

      {
        nn::gfx::DescriptorSlot slot;
        const int index = textureSlotIndex + usedTextureSlots++;
        graphics->SetTextureViewToDescriptorPool( index, charModel.GetTextureView( nn::mii::CharModel::TextureType_Mask, 0 ) );
        graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_TextureView )->GetDescriptorSlot( &slot, index );
        charModel.SetTextureDescriptorSlot( nn::mii::CharModel::TextureType_Mask, slot, 0 );
      }

      for ( int i = 0; i < nn::mii::Faceline::TextureType_End; ++i )
      {
        const auto type = static_cast<nn::mii::Faceline::TextureType>( i );
        const nn::gfx::TextureView * view = faceline.GetTextureView( type );

        if ( view != nullptr )
        {
          nn::gfx::DescriptorSlot slot;
          const int index = textureSlotIndex + usedTextureSlots++;
          graphics->SetTextureViewToDescriptorPool( index, view );
          graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_TextureView )->GetDescriptorSlot( &slot, index );
          faceline.SetTextureDescriptorSlot( type, slot );
        }
      }

      for ( int i = 0; i < nn::mii::Mask::TextureType_End; ++i )
      {
        const auto type = static_cast<nn::mii::Mask::TextureType>( i );
        const nn::gfx::TextureView * view = mask.GetTextureView( type );

        if ( view != nullptr )
        {
          nn::gfx::DescriptorSlot slot;
          const int index = textureSlotIndex + usedTextureSlots++;
          graphics->SetTextureViewToDescriptorPool( index, view );
          graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_TextureView )->GetDescriptorSlot( &slot, index );
          mask.SetTextureDescriptorSlot( type, slot );
        }
      }

      NN_SDK_ASSERT_LESS_EQUAL( usedTextureSlots, TextureSlotCount );
      return true;
    }

    bool InitializeGenerationBuffers()
    {
      size_t memorySize = nn::mii::FacelineGpuBuffer::CalculateMemorySize();
      facelineGpuMemory = Allocate( memorySize,
                                    nn::mii::FacelineGpuBuffer::CalculateMemoryAlignment(),
                                    "Allocate FacelineGpuBuffer memory" );

      size_t poolSize = nn::mii::FacelineGpuBuffer::CalculateMemoryPoolSize( device );
      facelineGpuPoolOffset = graphics->AllocatePoolMemory(
        Framework::MemoryPoolType_ConstantBuffer,
        poolSize,
        nn::mii::FacelineGpuBuffer::CalculateMemoryPoolAlignment( device ) );

      if ( facelineGpuMemory == nullptr )
      {
        return false;
      }

      nn::Result result = facelineGpuBuffer.Initialize(
        facelineGpuMemory,
        memorySize,
        device,
        graphics->GetMemoryPool( Framework::MemoryPoolType_ConstantBuffer ),
        facelineGpuPoolOffset,
        poolSize );

      if ( result.IsFailure() )
      {
        SetError( "FacelineGpuBuffer::Initialize", result );
        return false;
      }

      memorySize = nn::mii::MaskGpuBuffer::CalculateMemorySize();
      maskGpuMemory = Allocate(
        memorySize, nn::mii::MaskGpuBuffer::CalculateMemoryAlignment(), "Allocate MaskGpuBuffer memory" );

      poolSize = nn::mii::MaskGpuBuffer::CalculateMemoryPoolSize( device );
      maskGpuPoolOffset = graphics->AllocatePoolMemory(
        Framework::MemoryPoolType_ConstantBuffer,
        poolSize,
        nn::mii::MaskGpuBuffer::CalculateMemoryPoolAlignment( device ) );

      if ( maskGpuMemory == nullptr )
      {
        return false;
      }

      result = maskGpuBuffer.Initialize( maskGpuMemory,
                                         memorySize,
                                         device,
                                         graphics->GetMemoryPool( Framework::MemoryPoolType_ConstantBuffer ),
                                         maskGpuPoolOffset,
                                         poolSize );

      if ( result.IsFailure() )
      {
        SetError( "MaskGpuBuffer::Initialize", result );
        return false;
      }

      return true;
    }

    bool GenerateFaceTextures()
    {
      Framework::CommandBuffer commandBuffer = {};
      nn::gfx::CommandBuffer::InfoType info;
      info.SetDefault();
      info.SetQueueCapability( nn::gfx::QueueCapability_Graphics );
      info.SetCommandBufferType( nn::gfx::CommandBufferType_Direct );

      graphics->InitializeCommandBuffer( &commandBuffer, info, 1024 * 1024, 1024 );

      facelineGpuBuffer.SetColorTarget( device,
                                        charModel.GetFacelineTexture(),
                                        textureShaderInfo,
                                        TextureResolution / 2,
                                        TextureResolution,
                                        TextureMipCount );
      facelineGpuBuffer.SetFaceline( faceline );

      graphics->ResetCommandBuffer( &commandBuffer );
      commandBuffer.object.Begin();
      commandBuffer.object.SetDescriptorPool( graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_TextureView ) );
      commandBuffer.object.SetDescriptorPool( graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_Sampler ) );
      commandBuffer.object.InvalidateMemory( nn::gfx::GpuAccess_Texture
                                           | nn::gfx::GpuAccess_IndexBuffer
                                           | nn::gfx::GpuAccess_ConstantBuffer
                                           | nn::gfx::GpuAccess_VertexBuffer );
      textureShader.DrawFaceline( &commandBuffer.object, &facelineGpuBuffer, samplerSlot );
      commandBuffer.object.End();
      graphics->GetQueue()->ExecuteCommand( &commandBuffer.object, nullptr );
      graphics->GetQueue()->Sync();

      maskGpuBuffer.SetColorTarget( device,
                                    charModel.GetMaskTexture( 0 ),
                                    textureShaderInfo,
                                    TextureResolution,
                                    TextureMipCount );
      maskGpuBuffer.SetMask( mask, nn::mii::Expression_Normal );

      graphics->ResetCommandBuffer( &commandBuffer );
      commandBuffer.object.Begin();
      commandBuffer.object.SetDescriptorPool( graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_TextureView ) );
      commandBuffer.object.SetDescriptorPool( graphics->GetDescriptorPool( nn::gfx::DescriptorPoolType_Sampler ) );
      commandBuffer.object.InvalidateMemory( nn::gfx::GpuAccess_Texture
                                           | nn::gfx::GpuAccess_IndexBuffer
                                           | nn::gfx::GpuAccess_ConstantBuffer
                                           | nn::gfx::GpuAccess_VertexBuffer );
      textureShader.DrawMask( &commandBuffer.object, &maskGpuBuffer, samplerSlot );
      commandBuffer.object.End();
      graphics->GetQueue()->ExecuteCommand( &commandBuffer.object, nullptr );
      graphics->GetQueue()->Sync();

      graphics->FinalizeCommandBuffer( &commandBuffer );
      return true;
    }

    bool InitializeRenderStates()
    {
      for ( int i = 0; i < nn::gfx::CullMode_End; ++i )
      {
        nn::gfx::RasterizerStateInfo info;
        info.SetDefault();
        info.SetPrimitiveTopologyType( nn::gfx::PrimitiveTopologyType_Triangle );
        info.SetScissorEnabled( true );
        info.SetDepthClipEnabled( false );
        info.SetCullMode( static_cast<nn::gfx::CullMode>( i ) );
        rasterizerStates[ i ].Initialize( device, info );
        ++rasterizerStateCount;
      }

      for ( int i = 0; i < DrawMode_Count; ++i )
      {
        nn::gfx::BlendTargetStateInfo targetInfo;
        targetInfo.SetDefault();

        if ( i == DrawMode_Translucent )
        {
          targetInfo.SetBlendEnabled( true );
          targetInfo.SetColorBlendFunction( nn::gfx::BlendFunction_Add );
          targetInfo.SetSourceColorBlendFactor( nn::gfx::BlendFactor_SourceAlpha );
          targetInfo.SetDestinationColorBlendFactor( nn::gfx::BlendFactor_OneMinusSourceAlpha );
        }

        nn::gfx::BlendState::InfoType blendInfo;
        blendInfo.SetDefault();
        blendInfo.SetBlendTargetStateInfoArray( &targetInfo, 1 );

        const size_t blendMemorySize = nn::gfx::BlendState::GetRequiredMemorySize( blendInfo );
        blendStateMemory[ i ] = Allocate(
          blendMemorySize, nn::gfx::BlendState::RequiredMemoryInfo_Alignment, "Allocate BlendState memory" );

        if ( blendStateMemory[ i ] == nullptr )
        {
          return false;
        }

        blendStates[ i ].SetMemory( blendStateMemory[ i ], blendMemorySize );
        blendStates[ i ].Initialize( device, blendInfo );
        ++blendStateCount;

        nn::gfx::DepthStencilStateInfo depthInfo;
        depthInfo.SetDefault();
        depthInfo.SetDepthTestEnabled( true );
        depthInfo.SetDepthWriteEnabled( i == DrawMode_Opaque );
        depthStates[ i ].Initialize( device, depthInfo );
        ++depthStateCount;
      }

      nn::gfx::VertexAttributeStateInfo attributes[ 2 ];
      attributes[ 0 ].SetDefault();
      attributes[ 0 ].SetBufferIndex( 0 );
      attributes[ 0 ].SetFormat( nn::mii::DrawParam::PositionFormat );
      attributes[ 0 ].SetOffset( nn::mii::DrawParam::PositionOffset );
      attributes[ 0 ].SetShaderSlot( 0 );

      attributes[ 1 ].SetDefault();
      attributes[ 1 ].SetBufferIndex( 1 );
      attributes[ 1 ].SetFormat( nn::mii::DrawParam::UvFormat );
      attributes[ 1 ].SetOffset( nn::mii::DrawParam::UvOffset );
      attributes[ 1 ].SetShaderSlot( 1 );

      nn::gfx::VertexBufferStateInfo buffers[ 2 ];
      buffers[ 0 ].SetDefault();
      buffers[ 0 ].SetStride( nn::mii::DrawParam::PositionStride );
      buffers[ 1 ].SetDefault();
      buffers[ 1 ].SetStride( nn::mii::DrawParam::UvStride );

      nn::gfx::VertexState::InfoType vertexInfo;
      vertexInfo.SetDefault();
      vertexInfo.SetVertexAttributeStateInfoArray( attributes, 2 );
      vertexInfo.SetVertexBufferStateInfoArray( buffers, 2 );

      const size_t vertexMemorySize = nn::gfx::VertexState::GetRequiredMemorySize( vertexInfo );
      vertexStateMemory = Allocate(
        vertexMemorySize, nn::gfx::VertexState::RequiredMemoryInfo_Alignment, "Allocate VertexState memory" );

      if ( vertexStateMemory == nullptr )
      {
        return false;
      }

      vertexState.SetMemory( vertexStateMemory, vertexMemorySize );
      vertexState.Initialize( device, vertexInfo, nullptr );
      vertexStateInitialized = true;

      const auto * program = modelShaderResource.file->GetShaderContainer()
                               ->GetResShaderVariation( 0 )
                               ->GetResShaderProgram( modelShaderResource.codeType );
      const nn::gfx::Shader * shader = program->GetShader();

      matrixSlot = shader->GetInterfaceSlot(
        nn::gfx::ShaderStage_Vertex, nn::gfx::ShaderInterfaceType_ConstantBuffer, "u_Matrix" );
      modulateSlot = shader->GetInterfaceSlot(
        nn::gfx::ShaderStage_Pixel, nn::gfx::ShaderInterfaceType_ConstantBuffer, "u_Modulate" );
      samplerShaderSlot = shader->GetInterfaceSlot(
        nn::gfx::ShaderStage_Pixel, nn::gfx::ShaderInterfaceType_Sampler, "s_Texture" );

      if ( matrixSlot < 0 || modulateSlot < 0 || samplerShaderSlot < 0 )
      {
        SetError( "Find Mii model shader interfaces" );
        return false;
      }

      return true;
    }

    bool InitializeConstantBuffers()
    {
      nn::gfx::BufferInfo matrixInfo;
      matrixInfo.SetDefault();
      matrixInfo.SetGpuAccessFlags( nn::gfx::GpuAccess_ConstantBuffer );
      matrixInfo.SetSize( sizeof( ConstantBufferMatrix ) );

      matrixBufferOffset = graphics->AllocatePoolMemory(
        Framework::MemoryPoolType_ConstantBuffer,
        matrixInfo.GetSize(),
        nn::gfx::Buffer::GetBufferAlignment( device, matrixInfo ) );
      matrixBuffer.Initialize( device,
                               matrixInfo,
                               graphics->GetMemoryPool( Framework::MemoryPoolType_ConstantBuffer ),
                               matrixBufferOffset,
                               matrixInfo.GetSize() );
      matrixBufferInitialized = true;

      auto * matrix = matrixBuffer.Map<ConstantBufferMatrix>();

      nn::util::Matrix4x4fType projection;
      nn::util::MatrixPerspectiveFieldOfViewRightHanded(
        &projection, 3.14159265f / 4.0f, 1280.0f / 720.0f, 0.01f, 10000.0f );
      nn::util::MatrixStore( &matrix->projection, projection );

      const nn::util::Vector3fType cameraPosition = NN_UTIL_VECTOR_3F_INITIALIZER( -35.0f, 30.0f, 115.0f );
      const nn::util::Vector3fType cameraTarget   = NN_UTIL_VECTOR_3F_INITIALIZER( -35.0f, 30.0f, 0.0f );
      nn::util::Matrix4x3fType modelView;
      nn::util::MatrixLookAtRightHanded( &modelView, cameraPosition, cameraTarget, 0.0f );
      nn::util::MatrixStore( &matrix->modelView, modelView );

      matrixBuffer.Unmap();

      nn::gfx::BufferInfo modulateInfo;
      modulateInfo.SetDefault();
      modulateInfo.SetGpuAccessFlags( nn::gfx::GpuAccess_ConstantBuffer );
      modulateInfo.SetSize( sizeof( ConstantBufferModulate ) );

      const size_t alignment = nn::gfx::Buffer::GetBufferAlignment( device, modulateInfo );

      for ( int i = 0; i < nn::mii::CharModel::DrawType_End; ++i )
      {
        modulateBufferOffsets[ i ] = graphics->AllocatePoolMemory(
          Framework::MemoryPoolType_ConstantBuffer, modulateInfo.GetSize(), alignment );
        modulateBuffers[ i ].Initialize( device,
                                         modulateInfo,
                                         graphics->GetMemoryPool( Framework::MemoryPoolType_ConstantBuffer ),
                                         modulateBufferOffsets[ i ],
                                         modulateInfo.GetSize() );
        ++modulateBufferCount;

        const auto drawType = static_cast<nn::mii::CharModel::DrawType>( i );
        const nn::mii::DrawParam * drawParam =
          charModel.GetDrawParam( drawType, CreateModelType, CreateNoseType, 0 );

        auto * constants = modulateBuffers[ i ].Map<ConstantBufferModulate>();
        *constants = {};

        if ( drawParam != nullptr )
        {
          constants->modulateType = drawParam->GetModulateType();

          for ( int colorIndex = 0; colorIndex < nn::mii::DrawParam::ConstantColorNum; ++colorIndex )
          {
            const nn::mii::Color3 * color = drawParam->GetConstantColor( colorIndex );

            if ( color != nullptr )
            {
              constants->constantColor[ colorIndex ].x = color->r;
              constants->constantColor[ colorIndex ].y = color->g;
              constants->constantColor[ colorIndex ].z = color->b;
            }
          }
        }

        modulateBuffers[ i ].Unmap();
      }

      return true;
    }

    void DrawByParam( nn::gfx::CommandBuffer * commands, const nn::mii::DrawParam & drawParam )
    {
      if ( drawParam.IsValidTexture() )
      {
        commands->SetTextureAndSampler( samplerShaderSlot,
                                        nn::gfx::ShaderStage_Pixel,
                                        *drawParam.GetTextureDescriptorSlot(),
                                        samplerSlot );
      }

      nn::gfx::GpuAddress address;
      const nn::gfx::Buffer * vertexBuffer =
        drawParam.GetAttribute( nn::mii::DrawParam::AttributeType_Position );
      size_t vertexBufferSize = drawParam.GetBufferSize( nn::mii::DrawParam::AttributeType_Position );

      vertexBuffer->GetGpuAddress( &address );
      commands->SetVertexBuffer( 0, address, nn::mii::DrawParam::PositionStride, vertexBufferSize );

      if ( drawParam.IsValidAttribute( nn::mii::DrawParam::AttributeType_Uv ) )
      {
        vertexBuffer = drawParam.GetAttribute( nn::mii::DrawParam::AttributeType_Uv );
        vertexBufferSize = drawParam.GetBufferSize( nn::mii::DrawParam::AttributeType_Uv );
      }

      vertexBuffer->GetGpuAddress( &address );
      commands->SetVertexBuffer( 1, address, nn::mii::DrawParam::UvStride, vertexBufferSize );

      drawParam.GetIndexBuffer()->GetGpuAddress( &address );
      commands->DrawIndexed( nn::mii::DrawParam::PrimitiveTopology,
                             nn::mii::DrawParam::IndexFormat,
                             address,
                             drawParam.GetIndexCount(),
                             0 );
    }

    void Draw( nn::gfx::CommandBuffer * commands )
    {
      if ( !ready )
      {
        return;
      }

      commands->InvalidateMemory( nn::gfx::GpuAccess_Texture
                                | nn::gfx::GpuAccess_IndexBuffer
                                | nn::gfx::GpuAccess_ConstantBuffer
                                | nn::gfx::GpuAccess_VertexBuffer );

      const nn::gfx::Shader * shader = modelShaderResource.file->GetShaderContainer()
                                         ->GetResShaderVariation( 0 )
                                         ->GetResShaderProgram( modelShaderResource.codeType )
                                         ->GetShader();

      for ( int i = 0; i < nn::mii::CharModel::DrawType_End; ++i )
      {
        const auto drawType = static_cast<nn::mii::CharModel::DrawType>( i );
        const nn::mii::DrawParam * drawParam =
          charModel.GetDrawParam( drawType, CreateModelType, CreateNoseType, 0 );

        if ( drawParam == nullptr )
        {
          continue;
        }

        const DrawMode mode = drawType >= nn::mii::CharModel::DrawType_XluMin
                                ? DrawMode_Translucent
                                : DrawMode_Opaque;

        commands->SetRasterizerState( &rasterizerStates[ drawParam->GetCullMode() ] );
        commands->SetBlendState( &blendStates[ mode ] );
        commands->SetDepthStencilState( &depthStates[ mode ] );
        commands->SetVertexState( &vertexState );
        commands->SetShader( shader, nn::gfx::ShaderStageBit_All );

        nn::gfx::GpuAddress address;
        matrixBuffer.GetGpuAddress( &address );
        commands->SetConstantBuffer(
          matrixSlot, nn::gfx::ShaderStage_Vertex, address, sizeof( ConstantBufferMatrix ) );

        modulateBuffers[ i ].GetGpuAddress( &address );
        commands->SetConstantBuffer(
          modulateSlot, nn::gfx::ShaderStage_Pixel, address, sizeof( ConstantBufferModulate ) );

        DrawByParam( commands, *drawParam );
      }
    }

    void Finalize()
    {
      if ( graphics == nullptr )
      {
        return;
      }

      ready = false;

      for ( int i = modulateBufferCount - 1; i >= 0; --i )
      {
        modulateBuffers[ i ].Finalize( device );
        graphics->FreePoolMemory( Framework::MemoryPoolType_ConstantBuffer, modulateBufferOffsets[ i ] );
      }

      modulateBufferCount = 0;

      if ( matrixBufferInitialized )
      {
        matrixBuffer.Finalize( device );
        graphics->FreePoolMemory( Framework::MemoryPoolType_ConstantBuffer, matrixBufferOffset );
        matrixBufferInitialized = false;
      }

      if ( vertexStateInitialized )
      {
        vertexState.Finalize( device );
        vertexStateInitialized = false;
      }

      if ( vertexStateMemory != nullptr )
      {
        graphics->FreeMemory( vertexStateMemory );
        vertexStateMemory = nullptr;
      }

      for ( int i = depthStateCount - 1; i >= 0; --i )
      {
        depthStates[ i ].Finalize( device );
      }

      depthStateCount = 0;

      for ( int i = blendStateCount - 1; i >= 0; --i )
      {
        blendStates[ i ].Finalize( device );
        graphics->FreeMemory( blendStateMemory[ i ] );
        blendStateMemory[ i ] = nullptr;
      }

      blendStateCount = 0;

      for ( int i = rasterizerStateCount - 1; i >= 0; --i )
      {
        rasterizerStates[ i ].Finalize( device );
      }

      rasterizerStateCount = 0;

      if ( maskGpuBuffer.IsInitialized() )
      {
        maskGpuBuffer.Finalize( device );
      }

      if ( maskGpuPoolOffset >= 0 )
      {
        graphics->FreePoolMemory( Framework::MemoryPoolType_ConstantBuffer, maskGpuPoolOffset );
        maskGpuPoolOffset = -1;
      }

      if ( facelineGpuBuffer.IsInitialized() )
      {
        facelineGpuBuffer.Finalize( device );
      }

      if ( facelineGpuPoolOffset >= 0 )
      {
        graphics->FreePoolMemory( Framework::MemoryPoolType_ConstantBuffer, facelineGpuPoolOffset );
        facelineGpuPoolOffset = -1;
      }

      if ( maskGpuMemory != nullptr )
      {
        graphics->FreeMemory( maskGpuMemory );
        maskGpuMemory = nullptr;
      }

      if ( facelineGpuMemory != nullptr )
      {
        graphics->FreeMemory( facelineGpuMemory );
        facelineGpuMemory = nullptr;
      }

      if ( textureSlotIndex >= 0 )
      {
        graphics->FreeDescriptorSlot( nn::gfx::DescriptorPoolType_TextureView, textureSlotIndex );
        textureSlotIndex = -1;
      }

      if ( samplerSlotIndex >= 0 )
      {
        graphics->FreeDescriptorSlot( nn::gfx::DescriptorPoolType_Sampler, samplerSlotIndex );
        samplerSlotIndex = -1;
      }

      if ( samplerInitialized )
      {
        sampler.Finalize( device );
        samplerInitialized = false;
      }

      if ( mask.IsInitialized() )
      {
        mask.Finalize( device );
      }

      if ( maskPoolOffset >= 0 )
      {
        graphics->FreePoolMemory( Framework::MemoryPoolType_Data, maskPoolOffset );
        maskPoolOffset = -1;
      }

      if ( faceline.IsInitialized() )
      {
        faceline.Finalize( device );
      }

      if ( facelinePoolOffset >= 0 )
      {
        graphics->FreePoolMemory( Framework::MemoryPoolType_Data, facelinePoolOffset );
        facelinePoolOffset = -1;
      }

      if ( charModel.IsInitialized() )
      {
        charModel.Finalize( device );
      }

      if ( charStaticPoolOffset >= 0 )
      {
        graphics->FreePoolMemory( Framework::MemoryPoolType_Data, charStaticPoolOffset );
        charStaticPoolOffset = -1;
      }

      if ( charDynamicPoolOffset >= 0 )
      {
        graphics->FreePoolMemory( Framework::MemoryPoolType_Others, charDynamicPoolOffset );
        charDynamicPoolOffset = -1;
      }

      if ( maskMemory != nullptr )
      {
        graphics->FreeMemory( maskMemory );
        maskMemory = nullptr;
      }

      if ( facelineMemory != nullptr )
      {
        graphics->FreeMemory( facelineMemory );
        facelineMemory = nullptr;
      }

      if ( charModelMemory != nullptr )
      {
        graphics->FreeMemory( charModelMemory );
        charModelMemory = nullptr;
      }

      if ( textureShader.IsInitialized() )
      {
        textureShader.Finalize( device );
      }

      if ( textureShaderMemory != nullptr )
      {
        graphics->FreeMemory( textureShaderMemory );
        textureShaderMemory = nullptr;
      }

      FinalizeShaderResource( &modelShaderResource );
      FinalizeShaderResource( &textureShaderResource );

      device   = nullptr;
      graphics = nullptr;
    }
  };

  FixedModelRenderer::~FixedModelRenderer()
  {
    Finalize();
  }

  bool FixedModelRenderer::Initialize( nns::gfx::GraphicsFramework & graphics,
                                       const RenderResources &        resources,
                                       const nn::mii::CharInfo &      charInfo )
  {
    Finalize();

#if !defined( NN_BUILD_CONFIG_OS_SUPPORTS_HORIZON )
    NN_UNUSED( graphics );
    NN_UNUSED( resources );
    NN_UNUSED( charInfo );
    return false;
#else
    m_Impl = new ( std::nothrow ) Impl();

    if ( m_Impl == nullptr )
    {
      return false;
    }

    m_Impl->graphics = &graphics;
    m_Impl->device   = graphics.GetDevice();

    if ( !resources.IsReady() )
    {
      m_Impl->SetError( "Mii rendering resources are not ready" );
      return false;
    }

    const nn::mii::Resource & resource = resources.GetResource();

    const bool initialized = m_Impl->LoadShaders()
                          && m_Impl->InitializeTextureShader()
                          && m_Impl->InitializeCharModel( resource, charInfo )
                          && m_Impl->InitializeFaceline( resource, charInfo )
                          && m_Impl->InitializeMask( resource, charInfo )
                          && m_Impl->BindDescriptors()
                          && m_Impl->InitializeGenerationBuffers()
                          && m_Impl->GenerateFaceTextures()
                          && m_Impl->InitializeRenderStates()
                          && m_Impl->InitializeConstantBuffers();

    if ( !initialized )
    {
      return false;
    }

    m_Impl->ready = true;
    NN_LOG( "[Wishbone] Fixed Mii renderer ready.\n" );
    return true;
#endif
  }

  void FixedModelRenderer::Finalize()
  {
    if ( m_Impl != nullptr )
    {
      m_Impl->Finalize();
      delete m_Impl;
      m_Impl = nullptr;
    }
  }

  void FixedModelRenderer::Draw( nn::gfx::CommandBuffer * commands )
  {
    if ( m_Impl != nullptr )
    {
      m_Impl->Draw( commands );
    }
  }

  bool FixedModelRenderer::IsSupported() const
  {
#if defined( NN_BUILD_CONFIG_OS_SUPPORTS_HORIZON )
    return true;
#else
    return false;
#endif
  }

  bool FixedModelRenderer::IsReady() const
  {
    return m_Impl != nullptr && m_Impl->ready;
  }

  bool FixedModelRenderer::HasError() const
  {
    return m_Impl != nullptr && m_Impl->errorOperation != nullptr;
  }

  bool FixedModelRenderer::HasResultCode() const
  {
    return m_Impl != nullptr && m_Impl->hasResultCode;
  }

  const char * FixedModelRenderer::GetErrorOperation() const
  {
    return m_Impl != nullptr ? m_Impl->errorOperation : nullptr;
  }

  int FixedModelRenderer::GetErrorModule() const
  {
    return m_Impl != nullptr ? m_Impl->errorModule : 0;
  }

  int FixedModelRenderer::GetErrorDescription() const
  {
    return m_Impl != nullptr ? m_Impl->errorDescription : 0;
  }
} // namespace wishbone::mii
