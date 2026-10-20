# NVIDIA NRD (Real-Time Denoisers) v4.17.3

This folder holds a copy of the NVIDIA NRD denoiser library. It is compiled into
`rdsp-vulkan_x86_64.dll` as the OBJECT library `rdsp-vulkan_x86_64_nrd` (see
`code/rd-vulkan/CMakeLists.txt`). There is no separate library and no download at build time.

This software contains source code provided by NVIDIA Corporation.

## Origin

- NRD v4.17.3: https://github.com/NVIDIA-RTX/NRD (`Include/`, `Source/`, `Resources/Version.h`, and the `*.resources.hlsli`, `*_Config.hlsli`, `NRD.hlsli`, `NRDConfig.hlsli` files from `Shaders/`)
- MathLib v11: https://github.com/NVIDIA-RTX/MathLib (`MathLib/`, header-only)
- ShaderMake (commit 18f5a344e7ca8fa65daaf079d07bc8ce38453e05): https://github.com/NVIDIA-RTX/ShaderMake (`ShaderMake/ShaderBlob.h` and `ShaderBlob.cpp`, the blob reader)

The files are unmodified. Licenses: `LICENSE-NRD.txt`, `LICENSE-MathLib.txt`, `LICENSE-ShaderMake.txt`.

## Layout

- `Include/`, `Source/`: the NRD API and the C++ side of the library.
- `Shaders/`: the HLSL headers that `Source/*.cpp` includes. The HLSL sources are not copied.
- `Spirv/`: the 31 generated `*.cs.spirv.h` headers (SPIR-V permutation blobs, SPIR-V only, no DXIL, no DXBC).
- `MathLib/`, `ShaderMake/`: the two dependencies.

## Options used to generate the SPIR-V

These are the same options that the compile definitions in `code/rd-vulkan/CMakeLists.txt` use.
Both sets must stay equal.

| Option | Value |
| --- | --- |
| NRD_NORMAL_ENCODING | 2 |
| NRD_ROUGHNESS_ENCODING | 1 |
| NRD_SUPPORTS_VIEWPORT_OFFSET | OFF |
| NRD_SUPPORTS_CHECKERBOARD | ON |
| NRD_SUPPORTS_HISTORY_CONFIDENCE | ON |
| NRD_SUPPORTS_DISOCCLUSION_THRESHOLD_MIX | ON |
| NRD_SUPPORTS_ANTIFIREFLY | ON |
| REBLUR_PERFORMANCE_MODE | OFF |
| NRD_EMBEDS_SPIRV_SHADERS / DXIL / DXBC | ON / OFF / OFF |
| SPIRV_SREG_OFFSET / BREG / UREG / TREG | 0 / 2 / 3 / 20 |

ShaderMake arguments: `--flatten --stripReflection --WX --sRegShift 0 --bRegShift 2 --uRegShift 3 --tRegShift 20 --headerBlob --allResourcesBound --vulkanVersion 1.2 --sourceDir Shaders --ignoreConfigDir -c Shaders/Shaders.cfg -D NRD_INTERNAL -p SPIRV`

`Shaders/NRDConfig.hlsli` is the file that NRD's CMake generates from these options.

## How to regenerate the SPIR-V

1. Download NRD v4.17.3 and run its CMake configure with the options above
   (`-DNRD_STATIC_LIBRARY=ON -DNRD_EMBEDS_DXIL_SHADERS=OFF -DNRD_EMBEDS_DXBC_SHADERS=OFF
   -DNRD_SHADERS_PATH=<output dir>`). CMake downloads MathLib and ShaderMake.
2. Build the `NRDShaders` target. It writes the `*.cs.spirv.h` headers to `<output dir>`.
3. Copy the headers to `Spirv/`. Copy `Shaders/NRDConfig.hlsli` if the options changed.
