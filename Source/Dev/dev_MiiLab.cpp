#include "Dev/dev_MiiLab.hpp"

#include "Mii/mii_Catalog.hpp"
#include "Mii/mii_RenderResources.hpp"

#include <imgui.h>

#include <nn/mii/mii_CharInfoAccessor.h>

namespace wishbone::dev
{
  void MiiLab::Build( wishbone::mii::Catalog & catalog, const wishbone::mii::RenderResources & renderResources )
  {
    if ( !m_IsVisible )
    {
      return;
    }

    ImGui::SetNextWindowPos( ImVec2( 620, 24 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 620, 600 ), ImGuiCond_FirstUseEver );

    if ( ImGui::Begin( "Wishbone | Mii Lab", &m_IsVisible ) )
    {
      ImGui::TextUnformatted( "Read-only system Mii browser" );
      ImGui::Separator();

      if ( renderResources.IsReady() )
      {
        constexpr float BytesPerMebibyte = 1024.0f * 1024.0f;

        ImGui::TextColored( ImVec4( 0.35f, 0.9f, 0.45f, 1.0f ), "Rendering resources: ready" );
        ImGui::Text( "Middle shape / Low texture / sRGB | %.2f MiB total",
                     ( renderResources.GetTextureDataSize() + renderResources.GetShapeDataSize() +
                       renderResources.GetWorkingMemorySize() ) /
                       BytesPerMebibyte );
      }
      else if ( !renderResources.IsSupported() )
      {
        ImGui::TextDisabled( "Rendering resources: NX hardware only" );
      }
      else if ( renderResources.HasError() )
      {
        ImGui::TextColored( ImVec4( 1.0f, 0.35f, 0.3f, 1.0f ), "Rendering resources: failed" );

        if ( renderResources.HasResultCode() )
        {
          ImGui::TextWrapped( "%s failed (%d-%d)",
                              renderResources.GetErrorOperation(),
                              renderResources.GetErrorModule(),
                              renderResources.GetErrorDescription() );
        }
        else
        {
          ImGui::TextWrapped( "%s failed.", renderResources.GetErrorOperation() );
        }
      }

      ImGui::Separator();

      if ( !catalog.IsAvailable() )
      {
        ImGui::TextColored( ImVec4( 1.0f, 0.35f, 0.3f, 1.0f ), "Mii database unavailable" );

        if ( catalog.HasError() )
        {
          ImGui::TextWrapped(
            "%s failed (%d-%d)", catalog.GetErrorOperation(), catalog.GetErrorModule(), catalog.GetErrorDescription() );
        }

        if ( ImGui::Button( "Retry database initialization" ) )
        {
          catalog.Initialize();
        }
      }
      else
      {
        ImGui::Text( "System: %d | Defaults: %d | Invalid skipped: %d",
                     catalog.GetDatabaseCount(),
                     catalog.GetDefaultCount(),
                     catalog.GetInvalidCount() );

        if ( ImGui::Button( "Refresh" ) )
        {
          catalog.Refresh();
        }

        if ( catalog.HasError() )
        {
          ImGui::SameLine();
          ImGui::TextColored( ImVec4( 1.0f, 0.35f, 0.3f, 1.0f ),
                              "%s failed (%d-%d)",
                              catalog.GetErrorOperation(),
                              catalog.GetErrorModule(),
                              catalog.GetErrorDescription() );
        }

        const int count = catalog.GetCount();

        if ( count == 0 )
        {
          ImGui::TextUnformatted( "No valid Mii entries were returned." );
        }
        else
        {
          if ( m_SelectedMii >= count )
          {
            m_SelectedMii = count - 1;
          }

          ImGui::BeginChild( "Mii list", ImVec2( 280, 470 ), true );

          for ( int i = 0; i < count; ++i )
          {
            const auto & entry = catalog.GetEntry( i );

            ImGui::PushID( i );

            if ( ImGui::Selectable( entry.m_Nickname, m_SelectedMii == i ) )
            {
              m_SelectedMii = i;
            }

            ImGui::SameLine( 190.0f );
            ImGui::TextDisabled( "%s", entry.m_Source == nn::mii::Source_Database ? "System" : "Default" );
            ImGui::PopID();
          }

          ImGui::EndChild();
          ImGui::SameLine();

          const auto &                    selected = catalog.GetEntry( m_SelectedMii );
          const nn::mii::CharInfoAccessor accessor( selected.m_CharInfo );

          ImGui::BeginGroup();
          ImGui::Text( "%s", selected.m_Nickname );
          ImGui::Separator();
          ImGui::Text( "Source: %s", selected.m_Source == nn::mii::Source_Database ? "System" : "Default" );
          ImGui::Text( "Font region: %d", static_cast<int>( accessor.GetFontRegion() ) );
          ImGui::Text( "Favorite color: %d", static_cast<int>( accessor.GetFavoriteColor() ) );
          ImGui::Text( "Height: %d", accessor.GetHeight() );
          ImGui::Text( "Build: %d", accessor.GetBuild() );
          ImGui::Text( "Faceline: %d", accessor.GetFacelineType() );
          ImGui::Text( "Skin color: %d", accessor.GetSkinColor() );
          ImGui::Text( "Hair: %d%s", accessor.GetHairType(), accessor.IsHairFlip() ? " (flipped)" : "" );
          ImGui::EndGroup();
        }
      }

      ImGui::Separator();
      ImGui::TextDisabled( "No system Mii data is modified by this tool." );
    }

    ImGui::End();
  }

  bool MiiLab::IsVisible() const
  {
    return m_IsVisible;
  }

  void MiiLab::SetVisible( bool visible )
  {
    m_IsVisible = visible;
  }
} // namespace wishbone::dev