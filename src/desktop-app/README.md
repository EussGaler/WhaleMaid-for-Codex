# WhaleMaid desktop app source

This directory contains the original C++/Qt application source. It does not vendor Qt or the Live2D Cubism SDK.

## Requirements

- CMake 3.24 or later
- A C++20 compiler (the packaged Windows build used MSVC)
- Qt 6.7 with Widgets, OpenGLWidgets, OpenGL, Network, and Test
- Live2D Cubism SDK for Native 5 when `WHALE_ENABLE_LIVE2D=ON`

Configure the SDK location explicitly instead of editing personal paths into the project:

```powershell
cmake -S . -B build -DWHALE_LIVE2D_SDK_ROOT="D:\path\to\CubismSdkForNative"
cmake --build build --config Release
ctest --test-dir build -C Release
```

The runtime model/art directories are intentionally not part of the public source tree until their redistribution rights are confirmed. Place approved assets in `assets/` and `Resources/WhaleMaid/` for a Live2D release build. See the repository's `NOTICE.md`.

The original application source is provided under the repository's PolyForm Noncommercial License 1.0.0. Commercial use requires separate written permission. Character and art assets use the separate terms in `ASSETS-LICENSE.md`; third-party components retain their own licenses.
