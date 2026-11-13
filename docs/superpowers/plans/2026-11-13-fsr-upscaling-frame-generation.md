# FSR Upscaling and Frame Generation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** AMD FSR SDK 2.4 temporal upscaling and frame generation in the SP Vulkan renderer (raster and RTX), with setup menu options.

**Architecture:** A run-time-loaded FSR API (`amd_fidelityfx_loader_vk.dll` via `ffx_api_loader.h`) wrapped by `code/rd-vulkan/vk_fsr.cpp`. The RTX path swaps its TAAU pass for the FSR upscaler; the raster path splits the frame (3D at render size, FSR, 2D at display size); frame generation replaces the swapchain through the FSR swapchain context.

**Tech Stack:** C++ (MSVC, CMake), Vulkan 1.2, GLSL → SPIR-V embedded blobs, FSR SDK 2.4 C API.

**Spec:** `docs/superpowers/specs/2026-11-13-fsr-upscaling-frame-generation-design.md` — read it in full before any task.

## Global Constraints

- SP only: `code/rd-vulkan`. Never touch `codemp/`, rd-vanilla, rd-rend2.
- FSR off (`r_fsr 0`, `r_fsrFrameGen 0`, the defaults) = no change in behaviour or cost.
- Missing DLLs or unsupported GPU = FSR turns off with one console warning; no crash, no black screen.
- Comments: ASD-STE100 Simplified Technical English, one or two lines; 4-5 only for a function header.
- Edit sources with the Edit tool. Never `sed -i` / `perl -pi` (CRLF files become LF).
- Build both configs, both must succeed with no new warnings in the touched files:
  `cmake --build build --config Debug --target rdsp-vulkan_x86_64 -- -m -v:minimal -nologo`
  `cmake --build build --config Release --target rdsp-vulkan_x86_64 -- -m -v:minimal -nologo`
- Shaders: never run `compile.bat`. Raster: compile one shader with glslangValidator and append with `tools/bin2hex.exe`, CRLF, symbol count only goes up. RTX: `glslangValidator --target-env vulkan1.2 -DGLSL -S <stage> -V`, splice only new/changed blobs into `shader_data_rtx.c`, diff the symbol list against HEAD.
- Do not commit. Do not stage. Do not touch `code/game/g_items.cpp` (user's own pending change).
- Never launch the game unless the task says so; the controller does the in-game tests.
- Never use a separate worktree.

## Review Focus

1. `vid_restart` / map change / window resize with FSR or FG on: contexts and images are re-created, not leaked or left dangling (this codebase has a history of "torn down per map, never rebuilt").
2. Toggling `r_fsr` / `r_fsrQuality` / `r_fsrFrameGen` and `vid_restart` back to off: image identical to today (no stale render extent, no forced MSAA loss, no leftover jitter).
3. DLLs missing from the exe folder: renderer loads, one warning, FSR cvars ignored.
4. Menus, console, loading screens and cinematics (no 3D world view): drawn at full size, never smeared by FG, no FSR dispatch on a frame with no world view.
5. Vulkan validation (Debug) clean with FSR on and off.

---

### Task 1: SDK vendoring, loader, `vk_fsr` core, cvars  (Sonnet)

**Files:**
- Create: `lib/fsr/include/...` (copy of `Kits/FidelityFX/api/include`, `upscalers/include`, `framegeneration/include`, without `dx12`), `lib/fsr/license.md`, `lib/fsr/3rdpartynotice.md`, `lib/fsr/README.md` (version 2.4.0, source tag, launcher ships DLLs), `lib/fsr/bin/*.dll` (not in git)
- Modify: `.gitignore` (add `lib/fsr/bin/`), `code/rd-vulkan/CMakeLists.txt` (include dir; post-build copy of `lib/fsr/bin/*.dll` next to the exe output if the folder exists)
- Create: `code/rd-vulkan/vk_fsr.h`, `code/rd-vulkan/vk_fsr.cpp`
- Modify: `code/rd-vulkan/tr_init.cpp` (cvars), `tr_local.h` (extern cvars)

SDK source: `C:\Users\lambe\AppData\Local\Temp\claude\79dca871e5ac0211\66edad0d-865e-468f-b5ab-262874b7a79b\scratchpad\fsr_dl` (AMD-signed, verified).

**Interfaces — Produces:**
- Cvars `r_fsr` (0/1), `r_fsrQuality` (0-4, default 2), `r_fsrSharpness` (0.0-1.0, default 0.5, not latched), `r_fsrFrameGen` (0/1). Flags per spec.
- `qboolean vk_fsr_load_library( void );` loads loader DLL once, fills a `ffxFunctions` table; returns qfalse + one warning if absent.
- `qboolean vk_fsr_upscale_active( void );` / `qboolean vk_fsr_framegen_active( void );` the *effective* state (cvar on AND library loaded AND context ok).
- `void vk_fsr_get_render_size( uint32_t displayW, uint32_t displayH, uint32_t *renderW, uint32_t *renderH );` via `ffxQueryDescUpscaleGetRenderResolutionFromQualityMode`; returns display size when inactive.
- `float vk_fsr_mip_bias( void );` `log2(render/display) - 1.0`, 0 when inactive.
- `qboolean vk_fsr_upscale_create( uint32_t maxRenderW, uint32_t maxRenderH, uint32_t displayW, uint32_t displayH, uint32_t flags );` / `void vk_fsr_upscale_destroy( void );`
- `void vk_fsr_get_jitter( int frameIndex, float *x, float *y );` via `ffxQueryDescUpscaleGetJitterPhaseCount` + `GetJitterOffset` (pixels at render size).
- `typedef struct { VkImage image; VkImageView view; VkFormat format; uint32_t width, height; uint32_t usage_state; } vkFsrImage_t;` and
  `void vk_fsr_upscale_dispatch( VkCommandBuffer cmd, const vkFsrImage_t *color, const vkFsrImage_t *depth, const vkFsrImage_t *motion, const vkFsrImage_t *reactive /*may be NULL*/, const vkFsrImage_t *output, float jitterX, float jitterY, float mvScaleX, float mvScaleY, float frameTimeMs, float cameraNear, float cameraFar, float fovY, qboolean reset );`
  Wraps resources with `ffxApiGetResourceVK`; reads `r_fsrSharpness`.
- `void vk_fsr_shutdown( void );` destroys contexts; library stays loaded.
- Console command `fsrinfo`: library loaded, versions from `ffxQueryDescGetVersions`, effective state.

- [ ] Step 1: Vendor files as listed; `.gitignore`; CMake include + copy step.
- [ ] Step 2: Implement `vk_fsr.cpp/.h` per Interfaces; backend desc `ffxCreateBackendVKDesc` from `vk.physical_device`, `vk.device`, `vkGetDeviceProcAddr`.
- [ ] Step 3: Register cvars and `fsrinfo`; call `vk_fsr_shutdown` from the renderer shutdown path before device destroy.
- [ ] Step 4: Build Debug + Release. Expected: both succeed; DLLs appear next to `build/Debug/SWGL_SP.x86_64.exe` and the Release one.
- [ ] Step 5: Rename `lib/fsr/bin` temporarily, confirm in code review that the load path only warns (no assert, no null call). Restore it.

### Task 2: RTX upscaling with FSR  (Opus)

**Files:**
- Modify: `code/rd-vulkan/rtx/vk_rtx.cpp` (`vk_rtx_get_render_extent`, screen/TAA extents), `rtx/vk_rtx_taa.cpp` (`vk_rtx_taa_evaluate_settings`, dispatch choice), `rtx/vk_rtx_shade.cpp` (jitter from `vk_fsr_get_jitter`, texture LOD bias, history reset)
- Create: `code/rd-vulkan/rtx/vk_rtx_fsr.cpp` (inputs prep + dispatch), new compute GLSL under `shaders/glsl/rtx/compute/` for (a) linear view depth → device depth for FSR, (b) FX-only output that keeps `TAA_FX`, `IMG_TAA_A`-dependent readers and `readback.hdr_color` correct (or an FX-only mode of `asvgf_taau.comp`), (c) reactive mask from `PT_TRANSPARENT` alpha
- Modify: `rtx/vk_rtx_compute_shaders.h`, `shaders/spirv/shader_data_rtx.c`

**Interfaces — Consumes:** Task 1 API. **Produces:** `void vk_rtx_fsr_record( VkCommandBuffer cmd );` called where `vk_rtx_taa()` runs, when `vk_fsr_upscale_active()`.

Rules from spec: `r_fsr 1` forces `AA_MODE_UPSCALE` even with denoiser none; skip FSR in accumulation mode (final blit stretches). Output into `TAA_OUTPUT`; bloom and tone map unchanged. Motion: `FLAT_MOTION` is UV with prev = cur + motion → pass `mvScale = (-renderW, -renderH)` or whatever matches FSR's convention (FSR: `prev = cur + mv*scale` in pixels? check `ffx_upscale.h` doc and state the conclusion in a comment). Reset on `vk_rtx_taa_invalidate_history`.

- [ ] Step 1: Render extent from `vk_fsr_get_render_size` when active; check every image sized `IMG_WIDTH` vs `IMG_WIDTH_TAA`, the god-rays, NRD, radiance cache and checkerboard paths at render < display.
- [ ] Step 2: Context create/destroy with the RTX image life cycle (`vk_rtx` image init/destroy, resize).
- [ ] Step 3: GLSL passes; regenerate blobs per Global Constraints.
- [ ] Step 4: Dispatch + jitter + LOD bias + reset.
- [ ] Step 5: Build Debug + Release.

### Task 3: Raster upscaling with FSR  (Opus)

**Files:** `vk_init.cpp`, `vk_attachments.cpp`, `vk_frame.cpp`, `tr_backend.cpp`, `tr_main.cpp` (projection jitter), `vk_image.cpp` (sampler mip bias), `vk_pipelines.cpp` / `vk_local.h` as needed; new raster GLSL for the reactive mask if chosen.

**Interfaces — Consumes:** Task 1 API.

- [ ] Step 1: When `vk_fsr_upscale_active()` on raster: force `r_fbo`, `r_depthPrepass`, `r_velocityBuffer`; MSAA and supersample off for the scene; ignore `r_renderScale`. One console line.
- [ ] Step 2: Scene attachments (color, depth, G-buffer, velocity, SSAO) at render size; final color at display size.
- [ ] Step 3: Split point: end of the first world view of the frame (not `RDF_NOWORLDMODEL`), before any later 2D. Dispatch FSR into the display-size color image; later 2D and other 3D views draw at display size. Frames with no world view: no dispatch.
- [ ] Step 4: Jitter on the projection of that view (incl. its portals/mirrors/sky portal); velocity without jitter (or the FSR jittered-MV flag); mip bias on world/model samplers.
- [ ] Step 5: Reactive mask for blended surfaces; glow/bloom after FSR at display size.
- [ ] Step 6: Build Debug + Release.

### Task 4: Frame generation  (Opus)

**Files:** `vk_instance.cpp` (extra queues from the graphics family when `r_fsrFrameGen 1`), `vk_swapchain.cpp`, `vk_frame.cpp`, `vk_init.cpp`, `vk_fsr.cpp/.h`, `rtx/` present path if separate.

**Interfaces — Produces:** `qboolean vk_fsr_framegen_create_swapchain( VkSwapchainCreateInfoKHR *ci, VkSwapchainKHR *swapchain );`, `void vk_fsr_framegen_destroy_swapchain( void );`, `void vk_fsr_framegen_frame( VkCommandBuffer cmd, ... );` (configure + PrepareV2 in AMD's mandatory order), HUD-less copy at the split point through the final gamma/present transform, in back-buffer format.

- [ ] Step 1: Queues + swapchain context + replacement function pointers used everywhere swapchain calls are made.
- [ ] Step 2: FG context; per-frame configure/prepare; HUD-less image; `frameGenerationEnabled` false on frames with no world view, on reset/camera cut/map load.
- [ ] Step 3: Teardown order per AMD doc (wait for presents, destroy, restore pointers) before any swapchain re-creation; FG off in `g_FastRendererSwitch` dual mode.
- [ ] Step 4: Raster FG without `r_fsr` forces the G-buffer cvars; RTX FG uses the RTX depth/motion.
- [ ] Step 5: Build Debug + Release.

### Task 5: Menu and strings  (Sonnet)

**Files:** `ui/Setup.menu`, `ui/IngameSetup.menu` in `C:\Program Files (x86)\Steam\steamapps\common\Jedi Academy - SWGL_DEV\GameData\SWGL\SWGL_Menu.pk3`; the string file that holds `SWGL_SETUP_*` (find it; every language).

- [ ] Step 1: Back up the pk3 to the scratchpad. Read entries by `ZipInfo` (duplicates); rewritten entries dated now.
- [ ] Step 2: Four items in `pg_video_vka` per spec (Upscaling, FSR quality, Sharpness, Frame generation), existing style, latched-cvar apply flow like the existing latched items, no overlapping rects.
- [ ] Step 3: Strings `SWGL_SETUP_FSR_*` + `_DESC`.
- [ ] Step 4: Verify: re-open the pk3, list entries, parse the edited menus for balanced braces; both menus have the four items.
