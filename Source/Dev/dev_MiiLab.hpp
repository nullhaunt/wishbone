#pragma once

namespace wishbone::mii
{
  class Catalog;
  class RenderResources;
} // namespace wishbone::mii

namespace wishbone::dev
{
  class MiiLab
  {
  public:
    void Build( wishbone::mii::Catalog & catalog, const wishbone::mii::RenderResources & renderResources );

    bool IsVisible() const;
    void SetVisible( bool visible );

  private:
    bool m_IsVisible   = true;
    int  m_SelectedMii = 0;
  };
} // namespace wishbone::dev