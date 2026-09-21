#pragma once

namespace nn::vi
{
  class Layer;
}

namespace wishbone::dev
{
  struct DebugInputStatus
  {
    const char * controller = "none";
    bool         keyboard   = false;
    bool         mouse      = false;
    bool         touch      = false;
  };

  void InitializeDebugInput();

  // Call before dbgui::NewFrame. Input sampling and character entry use SDK HID.
  DebugInputStatus UpdateDebugInput( nn::vi::Layer * layer );
} // namespace wishbone::dev