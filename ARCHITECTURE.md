# Project Architecture Reference

Model after: **https://github.com/Daivuk/onut**

## Core Principles

1. **Vendor isolation** — public headers in `include/` have ZERO vendor includes. All vendor headers (`<volk.h>`, `<SDL3/SDL.h>`, `<d3d12.h>`, etc.) are confined to `src/`.
2. **Renderer-agnostic Window** — `Window.h` is generic, only forward-declares `SDL_Window*`, no Vulkan/D3D/GL types.
3. **Renderer abstraction** — concrete renderers (`RendererVulkan`, `RendererD3D12`) are separate from `Window`. Each has its own header in `include/` (with vendor types) and impl in `src/`.
4. **Examples only link `library`** — no vendor include paths in example CMakeLists, everything comes transitively.

## Directory Layout

```
library/
├── include/              ← Public API (minimized vendor deps)
│   ├── Window.h          ← Generic: SDL_Window* fwd-decl only
│   ├── RendererVulkan.h  ← Vulkan: includes <volk.h> (opt-in)
│   └── (future) RendererD3D12.h
│
└── src/                  ← Private impls (vendor headers here)
    ├── WindowSDL3.cpp    ← SDL init/window/events only
    └── RendererVulkanImpl.cpp ← ALL Vulkan: instance, device, swapchain, sync

examples/
├── shared/
│   ├── BaseApp.h/.cpp    ← Owns Window* + Renderer*, runs main loop
└── 00-window/
    └── Main.cpp          ← Thin App : BaseApp (~40 lines)
```

## Adding a New Renderer (e.g. D3D12)

1. Create `include/RendererD3D12.h` with D3D12 accessors
2. Create `src/RendererD3D12Impl.cpp` with all D3D12 init code
3. Example includes the renderer-specific header — `Window.h` never changes

## Key Patterns (from onut)

- **Abstract factory**: `Renderer::create(Window*)` returns concrete impl
- **Forward declarations**: hide concrete types from public API
- **Build-time selection**: CMake selects backend, only one compiled
- **Clean callbacks**: `closeEvent`, `resizeEvent` on Window
- **No PImpl**: direct members, private methods for init/cleanup
