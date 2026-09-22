#pragma once

namespace wishbone::mii
{
  class Catalog;
  class FixedModelRenderer;
  class RenderResources;
} // namespace wishbone::mii

namespace wishbone::dev
{
  class MiiLab
  {
  public:
    void Build( wishbone::mii::Catalog &                  catalog,
                const wishbone::mii::RenderResources &    renderResources,
                const wishbone::mii::FixedModelRenderer & fixedModelRenderer );

    bool IsVisible() const;
    void SetVisible( bool visible );

  private:
    bool m_IsVisible   = false;
    int  m_SelectedMii = 0;
  };
} // namespace wishbone::dev
