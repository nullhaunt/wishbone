# Wishbone

The development harness uses the official Nintendo SDK 18.3.1 NVN backend and
its bundled Dear ImGui 1.87. It provides a small input test panel, a read-only
system Mii browser, and a fixed-model rendering checkpoint. The renderer builds
the first valid catalog entry with the SDK Mii draw library, generates its
faceline and all 19 SDK expression masks, and draws the model with depth before
the development UI. A Wishbone-owned RGBA canvas adds a transformable face-paint
layer without changing the imported system Mii data.

## Build

Set `NINTENDO_SDK_ROOT` to your NintendoSDK directory and install the SDK's
Visual Studio integration. Open `Wishbone.sln` in Visual Studio 2022, select
`Develop | NX64`, and build/run using the Nintendo SDK debugger on your devkit.
From a developer PowerShell, the equivalent build is:

```powershell
msbuild Wishbone.sln /t:Build /p:Configuration=Develop /p:Platform=NX64 /m
```

The output is `Build/NX64/Develop/Wishbone_Develop.nspd_root` (with the NSO and
NPDM alongside it). `Debug` and `Release` configurations are also available.
Use `Develop | x64` for the SDK's Windows NVN implementation; that executable
is `Build/x64/Develop/Wishbone.exe` and requires a compatible NVIDIA driver/GPU.
The build copies the SDK's `nvn.dll` next to it.

`SdkDebugUi.props` compiles the SDK's ImGui, `nns::dbgui`, and graphics sample
sources directly into Wishbone's local build directory. It references the SDK's
generated shader headers and leaves the SDK installation unchanged. Keep these
sources, headers, and libraries on the same SDK version. The graphics framework
is sample infrastructure for this harness, not a completed engine renderer.

`SdkMii.props` copies the matching SDK `TextureShader.bnsh` and compiles
`Resources/Mii/MiiModel.glsl` into the application content directory. The NX
package exposes both as `Contents:/Wishbone/Mii/*`. The fixed renderer owns its
Mii objects, GPU allocations, descriptors, generated textures, render states,
and teardown; application code only supplies the first catalog entry and calls
the renderer before ImGui.

## Input

The panel uses the SDK sample's default ImGui bindings:

| Input | Action |
| --- | --- |
| D-pad | Navigate widgets |
| B | Activate |
| A | Cancel/back |
| X | Enter text/input mode |
| Hold Y | Window navigation/movement |
| L / R | Slow / fast value adjustment |
| Mouse / keyboard | Pointer, navigation, and basic text entry |
| Touch screen | Pointer input in handheld mode |

Supported controller styles are handheld, paired Joy-Con, and FullKey (such as
a Pro Controller). A connected SDK DebugPad takes priority. On Windows, input
is accepted while the graphics window has focus. Native keyboard IME is not
wired; on Switch, the SDK software keyboard is used when no keyboard is connected.
The harness uses the default font and does not save window layouts.

## Runtime checkpoint

Build success alone does not verify rendering or device input. On the devkit:

1. Confirm the development panel appears with readable text and no graphics errors.
2. Navigate to **Test button**, press B, and verify one increment per activation.
   The SDK log also reports `[Wishbone] ImGui button pressed: N`.
3. Change the slider and background color; open/close the demo and metrics windows.
4. Edit the text field, including the software keyboard path without a USB keyboard.
5. Check mouse/keyboard, touch, and controller disconnect/reconnect as available.
6. Select **Exit Wishbone** and verify clean shutdown without a GPU/assertion error.

For the fixed-Mii checkpoint:

1. Confirm a Mii head appears behind the development UI with correct hair,
   skin, eyes, mouth, and facial details.
2. Open **Mii Lab** and confirm `Fixed Mii renderer: ready`.
3. Select several system and default rows and confirm the rendered head changes
   to the selected Mii.
4. Cycle all entries in the **Expression** combo and confirm the eyes, eyebrows,
   and mouth match the label. Expression changes should be immediate and should
   not rebuild the selected Mii; changing Miis should preserve the expression.
5. In **Face paint proof**, confirm the cyan dot and pink arrow appear on the
   face. Toggle the layer and change its color, opacity, position, scale, and
   rotation. Use the asymmetric pattern to check horizontal and vertical
   orientation, then move it across the cheeks, chin, brow, and forehead to
   expose UV seams or unsupported regions.
6. Change Miis and expressions while the paint is visible. The paint should
   remain anchored to the face, persist across both changes, and stay behind
   the SDK-rendered eyes, eyebrows, and mouth.
7. After the first Mii has rendered, disconnect the deployment USB connection
   and select several other Miis. Selection should remain responsive because
   the packaged Mii shader files are cached during initial renderer setup.
8. Change the background color and look for missing transparency, incorrect
   draw order, or depth artifacts around hair, glasses, and facial features.
9. Exit normally and confirm there is no graphics or allocator assertion.

Startup reports `[Wishbone] ImGui ready.`; normal exit reports
`[Wishbone] Development UI shut down.` in logging-enabled configurations.

Validation on 2026-09-21: NX64 Debug/Develop/Release and x64 Develop builds pass,
including Mii shader generation and NX application-data packaging.
The local Windows startup check stops at NVN device initialization
(`devInit must not be nullptr`); this machine has an AMD Radeon RX 7900 XT.
Owner hardware validation confirms Mii selection, all-expression preview, and
face-paint rendering and controls. The disconnected-USB regression check and
the known shutdown assertion remain pending hardware validation.
