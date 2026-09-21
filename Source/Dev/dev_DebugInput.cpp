#include "dev_DebugInput.hpp"

#include <nn/hid.h>
#include <nn/vi.h>
#include <nns/dbgui.h>

#if defined( NN_BUILD_TARGET_PLATFORM_OS_WIN )
  #include <nn/nn_Windows.h>
#endif

namespace wishbone::dev
{
  namespace
  {
    // SDK histories are newest first. Deliver new samples oldest first so a quick
    // press/release between rendered frames does not disappear (especially touch).
    template <typename State> struct History
    {
      State   states[ 16 ] = {};
      int     count        = 0;
      int64_t lastSample   = -1;

      template <typename Read, typename Deliver> void Update( Read read, Deliver deliver )
      {
        count = read( states, 16 );

        for ( int i = count - 1; i >= 0; --i )
        {
          if ( states[ i ].samplingNumber > lastSample )
          {
            deliver( states[ i ] );
          }
        }

        if ( count > 0 )
        {
          lastSample = states[ 0 ].samplingNumber;
        }
      }
    };

    History<nn::hid::KeyboardState> keyboardHistory;
    History<nn::hid::MouseState>    mouseHistory;

    bool HasFocus( nn::vi::Layer * layer )
    {
#if defined( NN_BUILD_TARGET_PLATFORM_OS_WIN )
      nn::vi::NativeWindowHandle window = {};
      nn::vi::GetNativeWindow( &window, layer );
      return reinterpret_cast<HWND>( window ) == GetForegroundWindow();
#else
      ( void )layer;
      return true;
#endif
    }

#if defined( NN_BUILD_TARGET_PLATFORM_NX )
    History<nn::hid::TouchScreenState<1>> touchHistory;

    template <typename State>
    bool ReadNpad( nn::hid::NpadIdType         id,
                   nn::hid::NpadButtonSet &    buttons,
                   nn::hid::AnalogStickState & left,
                   nn::hid::AnalogStickState & right )
    {
      State state = {};
      nn::hid::GetNpadState( &state, id );

      if ( !state.attributes.template Test<nn::hid::NpadAttribute::IsConnected>() )
      {
        return false;
      }

      buttons = state.buttons;
      left    = state.analogStickL;
      right   = state.analogStickR;

      return true;
    }
#endif
  } // namespace

  void InitializeDebugInput()
  {
    nn::hid::InitializeDebugPad();
    nn::hid::InitializeKeyboard();
    nn::hid::InitializeMouse();

#if defined( NN_BUILD_TARGET_PLATFORM_NX )
    nn::hid::InitializeTouchScreen( 1280, 720 );
    nn::hid::InitializeNpad();

    const nn::hid::NpadIdType ids[] = { nn::hid::NpadId::No1, nn::hid::NpadId::Handheld };

    nn::hid::SetSupportedNpadStyleSet( nn::hid::NpadStyleFullKey::Mask | nn::hid::NpadStyleHandheld::Mask |
                                       nn::hid::NpadStyleJoyDual::Mask );
    nn::hid::SetSupportedNpadIdType( ids, 2 );
#endif
  }

  DebugInputStatus UpdateDebugInput( nn::vi::Layer * layer )
  {
    DebugInputStatus status;
    const bool       focused = HasFocus( layer );

    keyboardHistory.Update( nn::hid::GetKeyboardStates, [ & ]( const nn::hid::KeyboardState & sampled ) {
      nn::hid::KeyboardState state = sampled;

      if ( !focused || !state.attributes.Test<nn::hid::KeyboardAttribute::IsConnected>() )
      {
        state = {};
      }

      nns::dbgui::SetKeyboardState( state );

      // Basic keyboard text through the SDK translator; native IME is not wired yet.
      nns::dbgui::AddInputCharactersFromKeyboardState( state );
    } );

    status.keyboard = focused && keyboardHistory.count > 0 &&
                      keyboardHistory.states[ 0 ].attributes.Test<nn::hid::KeyboardAttribute::IsConnected>();

    mouseHistory.Update(
      []( nn::hid::MouseState * states, int count ) {
        return nn::hid::GetMouseStates( states, count );
      },
      [ & ]( const nn::hid::MouseState & sampled ) {
        auto state = sampled;

        if ( !focused || !state.attributes.Test<nn::hid::MouseAttribute::IsConnected>() )
        {
          state.buttons.Reset();
          state.deltaX = state.deltaY = state.wheelDelta = 0;
        }

        nns::dbgui::SetMouseState( state );
      } );

    status.mouse = focused && mouseHistory.count > 0 &&
                   mouseHistory.states[ 0 ].attributes.Test<nn::hid::MouseAttribute::IsConnected>();

    nn::hid::DebugPadState debugPad = {};
    nn::hid::GetDebugPadState( &debugPad );
    const bool debugConnected = focused && debugPad.attributes.Test<nn::hid::DebugPadAttribute::IsConnected>();
    const nn::hid::AnalogStickState neutralStick = {};
    nn::hid::NpadButtonSet          buttons      = {};
    nn::hid::AnalogStickState       left = {}, right = {};

#if defined( NN_BUILD_TARGET_PLATFORM_NX )
    if ( !debugConnected && focused )
    {
      const auto handheld = nn::hid::GetNpadStyleSet( nn::hid::NpadId::Handheld );
      const auto player   = nn::hid::GetNpadStyleSet( nn::hid::NpadId::No1 );

      if ( handheld.Test<nn::hid::NpadStyleHandheld>() &&
           ReadNpad<nn::hid::NpadHandheldState>( nn::hid::NpadId::Handheld, buttons, left, right ) )
      {
        status.controller = "Handheld";
      }
      else if ( player.Test<nn::hid::NpadStyleFullKey>() &&
                ReadNpad<nn::hid::NpadFullKeyState>( nn::hid::NpadId::No1, buttons, left, right ) )
      {
        status.controller = "Pro Controller / FullKey";
      }
      else if ( player.Test<nn::hid::NpadStyleJoyDual>() &&
                ReadNpad<nn::hid::NpadJoyDualState>( nn::hid::NpadId::No1, buttons, left, right ) )
      {
        status.controller = "Joy-Con pair";
      }
    }
#endif

    // Release the previous device first. Both backends generate the same ImGui
    // gamepad keys, so sending two active pads would conflict.
    if ( debugConnected )
    {
      nns::dbgui::SetNpadState( {}, neutralStick, neutralStick );
      nns::dbgui::SetDebugPadState( debugPad.buttons, debugPad.analogStickL, debugPad.analogStickR );
      status.controller = "DebugPad";
    }
    else
    {
      nns::dbgui::SetDebugPadState( {}, neutralStick, neutralStick );
      nns::dbgui::SetNpadState( buttons, left, right );
    }

#if defined( NN_BUILD_TARGET_PLATFORM_NX )
    touchHistory.Update( nn::hid::GetTouchScreenStates<1>, [ & ]( const nn::hid::TouchScreenState<1> & state ) {
      if ( state.count > 0 )
      {
        nns::dbgui::SetTouchState( state.touches[ 0 ] );
      }
    } );

    status.touch = touchHistory.count > 0 && touchHistory.states[ 0 ].count > 0;

    if ( !status.keyboard )
    {
      nns::dbgui::UpdateSoftwareKeyboard();
    }
#endif

    return status;
  }
} // namespace wishbone::dev