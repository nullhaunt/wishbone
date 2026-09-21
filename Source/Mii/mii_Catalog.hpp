#pragma once

#include <nn/mii/mii_Common.h>
#include <nn/mii/mii_Database.h>

namespace wishbone::mii
{
  struct CatalogEntry
  {
    static constexpr int NicknameCapacity = nn::mii::Nickname::Length * 4 + 1;

    nn::mii::CharInfo m_CharInfo                     = {};
    nn::mii::Source   m_Source                       = nn::mii::Source_End;
    char              m_Nickname[ NicknameCapacity ] = {};
  };

  class Catalog
  {
  public:
    static constexpr int Capacity = nn::mii::DatabaseMiiCount + nn::mii::DefaultMiiCount;

    Catalog() = default;
    ~Catalog();

    Catalog( const Catalog & )             = delete;
    Catalog & operator=( const Catalog & ) = delete;

    bool Initialize();
    void Finalize();
    bool Refresh();

    bool IsAvailable() const;
    bool HasError() const;

    int GetCount() const;
    int GetDatabaseCount() const;
    int GetDefaultCount() const;
    int GetInvalidCount() const;

    const CatalogEntry & GetEntry( int index ) const;

    const char * GetErrorOperation() const;
    int          GetErrorModule() const;
    int          GetErrorDescription() const;

  private:
    void ClearError();
    void SetError( const char * operation, nn::Result result );

    nn::mii::Database m_Database;
    CatalogEntry      m_Entries[ Capacity ];
    int               m_Count            = 0;
    int               m_DatabaseCount    = 0;
    int               m_DefaultCount     = 0;
    int               m_InvalidCount     = 0;
    bool              m_IsInitialized    = false;
    const char *      m_ErrorOperation   = nullptr;
    int               m_ErrorModule      = 0;
    int               m_ErrorDescription = 0;
  };
} // namespace wishbone::mii