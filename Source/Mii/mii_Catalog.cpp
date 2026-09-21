#include "Mii/mii_Catalog.hpp"

#include <nn/mii/mii_CharInfoAccessor.h>
#include <nn/nn_Assert.h>
#include <nn/nn_Log.h>
#include <nn/util/util_CharacterEncoding.h>

namespace wishbone::mii
{
  namespace
  {
    void CopyNickname( char * destination, int destinationLength, const nn::mii::CharInfoAccessor & accessor )
    {
      nn::mii::Nickname nickname = {};
      accessor.GetNickname( &nickname, nn::mii::FontRegionFlag_All );

      const auto result = nn::util::ConvertStringUtf16NativeToUtf8( destination, destinationLength, nickname.name );

      if ( result != nn::util::CharacterEncodingResult_Success )
      {
        destination[ 0 ] = '?';
        destination[ 1 ] = '\0';
      }
    }
  } // namespace

  Catalog::~Catalog()
  {
    Finalize();
  }

  bool Catalog::Initialize()
  {
    if ( m_IsInitialized )
    {
      return Refresh();
    }

    const nn::Result result = m_Database.Initialize();

    if ( result.IsFailure() )
    {
      SetError( "Database::Initialize", result );
      return false;
    }

    m_IsInitialized = true;
    return Refresh();
  }

  void Catalog::Finalize()
  {
    if ( m_IsInitialized )
    {
      m_Database.Finalize();
      m_IsInitialized = false;
    }

    m_Count         = 0;
    m_DatabaseCount = 0;
    m_DefaultCount  = 0;
    m_InvalidCount  = 0;
  }

  bool Catalog::Refresh()
  {
    if ( !m_IsInitialized )
    {
      return false;
    }

    nn::mii::CharInfoElement elements[ Capacity ] = {};
    int                      fetchedCount         = 0;

    const nn::Result result = m_Database.Get( &fetchedCount, elements, Capacity, nn::mii::SourceFlag_All );

    if ( result.IsFailure() )
    {
      SetError( "Database::Get", result );
      return false;
    }

    int validCount    = 0;
    int databaseCount = 0;
    int defaultCount  = 0;
    int invalidCount  = 0;

    for ( int i = 0; i < fetchedCount; ++i )
    {
      const nn::mii::CharInfoAccessor accessor( elements[ i ].info );

      if ( !accessor.IsValid() )
      {
        ++invalidCount;
        continue;
      }

      CatalogEntry & entry = m_Entries[ validCount++ ];
      entry.m_CharInfo     = elements[ i ].info;
      entry.m_Source       = elements[ i ].source;
      CopyNickname( entry.m_Nickname, CatalogEntry::NicknameCapacity, accessor );

      if ( entry.m_Source == nn::mii::Source_Database )
      {
        ++databaseCount;
      }
      else if ( entry.m_Source == nn::mii::Source_Default )
      {
        ++defaultCount;
      }
    }

    m_Count         = validCount;
    m_DatabaseCount = databaseCount;
    m_DefaultCount  = defaultCount;
    m_InvalidCount  = invalidCount;

    ClearError();

    NN_LOG( "[Wishbone] Loaded %d Mii entries (%d system, %d default, %d invalid).\n",
            m_Count,
            m_DatabaseCount,
            m_DefaultCount,
            m_InvalidCount );

    return true;
  }

  bool Catalog::IsAvailable() const
  {
    return m_IsInitialized && m_Database.IsAvailable();
  }

  bool Catalog::HasError() const
  {
    return m_ErrorOperation != nullptr;
  }

  int Catalog::GetCount() const
  {
    return m_Count;
  }

  int Catalog::GetDatabaseCount() const
  {
    return m_DatabaseCount;
  }

  int Catalog::GetDefaultCount() const
  {
    return m_DefaultCount;
  }

  int Catalog::GetInvalidCount() const
  {
    return m_InvalidCount;
  }

  const CatalogEntry & Catalog::GetEntry( int index ) const
  {
    NN_SDK_ASSERT_RANGE( index, 0, m_Count );
    return m_Entries[ index ];
  }

  const char * Catalog::GetErrorOperation() const
  {
    return m_ErrorOperation;
  }

  int Catalog::GetErrorModule() const
  {
    return m_ErrorModule;
  }

  int Catalog::GetErrorDescription() const
  {
    return m_ErrorDescription;
  }

  void Catalog::ClearError()
  {
    m_ErrorOperation   = nullptr;
    m_ErrorModule      = 0;
    m_ErrorDescription = 0;
  }

  void Catalog::SetError( const char * operation, nn::Result result )
  {
    m_ErrorOperation   = operation;
    m_ErrorModule      = result.GetModule();
    m_ErrorDescription = result.GetDescription();

    NN_LOG( "[Wishbone] %s failed (%d-%d).\n", operation, m_ErrorModule, m_ErrorDescription );
  }
} // namespace wishbone::mii