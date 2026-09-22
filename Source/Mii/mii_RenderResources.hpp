#pragma once

#include <nn/mii/mii_Resource.h>

#include <cstddef>

namespace wishbone::mii
{
  class RenderResources
  {
  public:
    RenderResources() = default;
    ~RenderResources();

    RenderResources( const RenderResources & )             = delete;
    RenderResources & operator=( const RenderResources & ) = delete;

    bool Initialize( nn::gfx::Device * device );
    void Finalize();

    bool IsSupported() const;
    bool IsReady() const;
    bool HasError() const;
    bool HasResultCode() const;

    const nn::mii::Resource & GetResource() const;

    size_t GetTextureDataSize() const;
    size_t GetShapeDataSize() const;
    size_t GetWorkingMemorySize() const;

    const char * GetErrorOperation() const;
    int          GetErrorModule() const;
    int          GetErrorDescription() const;

  private:
    void ReleaseStorage();
    void ClearError();
    void SetError( const char * operation );
    void SetError( const char * operation, nn::Result result );

    nn::mii::Resource m_Resource;
    nn::gfx::Device * m_Device            = nullptr;
    void *            m_TextureData       = nullptr;
    void *            m_ShapeData         = nullptr;
    void *            m_WorkingMemory     = nullptr;
    size_t            m_TextureDataSize   = 0;
    size_t            m_ShapeDataSize     = 0;
    size_t            m_WorkingMemorySize = 0;
    const char *      m_ErrorOperation    = nullptr;
    int               m_ErrorModule       = 0;
    int               m_ErrorDescription  = 0;
    bool              m_HasResultCode     = false;
  };
} // namespace wishbone::mii