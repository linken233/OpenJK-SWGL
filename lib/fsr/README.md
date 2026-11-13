# AMD FSR SDK

Version 2.4.0 ("Redstone"), from GPUOpen-LibrariesAndSDKs/FidelityFX-SDK (git tree 7539ae8b6568ad29ed78a753404351daf3b5cca7).
The DLLs in `bin/` are the AMD-signed binaries of the SDK (`Kits/FidelityFX/signedbin`).

- `api/`, `upscalers/`, `framegeneration/`: the SDK headers, unchanged, in the original folder layout (the headers include each other with relative paths). The Vulkan backend only; the `dx12` headers are not copied.
- `license.md`, `3rdpartynotice.md`: the SDK license and third-party notices.
- `bin/`: `amd_fidelityfx_loader_vk.dll`, `amd_fidelityfx_upscaler_vk.dll`, `amd_fidelityfx_framegeneration_vk.dll`. This folder is not in git (see `.gitignore`).

The renderer loads `amd_fidelityfx_loader_vk.dll` at run time with `LoadLibrary`. It does not link the import library.
The CMake build copies `bin/*.dll` next to the executable. The game launcher ships the DLLs to players.
When the DLLs are missing, the renderer prints one warning and FSR stays off.
