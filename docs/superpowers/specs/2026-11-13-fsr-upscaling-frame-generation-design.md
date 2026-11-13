# AMD FSR upscaling and frame generation in rd-vulkan (SP)

Date: 2026-11-13. Status: design, written while the user is away.

## Intent

The user wants AMD FSR in the SP Vulkan renderer (`code/rd-vulkan`, the
`rdsp-vulkan` DLL), on both render paths:

- RTX (path tracer): get back playable frame rates.
- Vulkan raster: more fps in heavy scenes (many NPCs, stress maps).

Two features: temporal **upscaling** and **frame generation**. Each has
options in the setup menu (`Setup.menu` and `IngameSetup.menu`). The user
asked for the most recent FSR version.

## Version: AMD FSR SDK 2.4 (2026-10-28)

- GitHub tag `v2.4.0` of `GPUOpen-LibrariesAndSDKs/FidelityFX-SDK`.
- First FSR SDK where Vulkan has the same upscaling and frame generation
  features as DirectX 12.
- Effects are prebuilt, AMD-signed DLLs. The use of these DLLs is required:
  - `amd_fidelityfx_loader_vk.dll` (entry points `ffxCreateContext`,
    `ffxConfigure`, `ffxQuery`, `ffxDispatch`, `ffxDestroyContext`)
  - `amd_fidelityfx_upscaler_vk.dll`
  - `amd_fidelityfx_framegeneration_vk.dll`
- The API selects the effect version from the hardware:
  - RX 6000/7000/9000: FSR Upscaling 4.1.2 (ML). Otherwise FSR 3.1.6.
  - RX 9000: Frame Generation 4.0.2 + Swapchain 4.0.0. Otherwise
    Frame Generation 3.1.6 + Swapchain 3.1.7.
  - The user's GPU is NVIDIA, so the user gets FSR 3.1.6 + FG 3.1.6/3.1.7.
    The code does not choose the version. It uses the API defaults.
- Headers are MIT. The DLLs have an AMD binary-redistribution license
  (redistribute with the notice, no reverse engineering).
  `Kits/FidelityFX/docs/license.md`.
- A copy of the headers, DLLs and the technique docs is in the scratchpad of
  the 2026-11-13 session. The builder vendors them (see "Files").

## Goals

1. Temporal upscaling with FSR on the RTX path. It replaces the Q2RTX TAAU
   pass (`asvgf_taau.comp`) when FSR is on.
2. Temporal upscaling with FSR on the raster path, for the main 3D view.
3. Frame generation (FSR swapchain) on both paths.
4. Menu options for these, in both setup menus, with strings.
5. With FSR off, the renderer acts exactly as today (no cost, no change).

## Non-goals

- MP (`codemp/rd-vulkan`). SP only.
- rd-vanilla, rd-rend2, DX12.
- FSR Ray Regeneration and Radiance Caching (DX12 only).
- Dynamic resolution scaling.
- FSR 1 (spatial). Not added as a separate option.

## Anti-goals (a failure even if it "works")

- Any change in image or performance with FSR off.
- A crash or a black screen when the DLLs are missing or the GPU has no
  support. FSR must then turn off with one console warning.
- The HUD, menus, console or 2D text rendered at the low resolution, or
  smeared by frame generation.
- A broken `vid_restart`, renderer switch, `g_FastRendererSwitch`, or map
  change (see the memory notes on "torn down per-map, never rebuilt").
- Exclusion of surfaces or stages to hide artifacts.

## Cvars (builder's call on exact help text)

All `CVAR_ARCHIVE_ND | CVAR_LATCH` unless noted. They apply to raster and RTX.

| Cvar | Values | Meaning |
|---|---|---|
| `r_fsr` | 0/1 | FSR upscaling off/on. |
| `r_fsrQuality` | 0-4 | 0 Native AA (1.0x), 1 Quality (1.5x), 2 Balanced (1.7x), 3 Performance (2.0x), 4 Ultra Performance (3.0x). Use `ffxQueryDescUpscaleGetRenderResolutionFromQualityMode`. |
| `r_fsrSharpness` | 0.0-1.0, default 0.5, not latched | RCAS sharpening (`enableSharpening` when > 0). |
| `r_fsrFrameGen` | 0/1 | Frame generation off/on. Works with or without `r_fsr`. |

Rules:

- `r_fsr 1` turns off MSAA and `r_ext_supersample` for the 3D scene, and
  ignores `r_renderScale`. One console line says so.
- `r_fsr 1` on RTX forces the TAA mode to temporal upscale (jitter on) and
  uses FSR in place of `asvgf_taau`. `flt_taa` stays as it is for FSR off.
- Texture mip bias for the 3D view: `log2(render / display) - 1.0`
  (AMD recommendation), only while `r_fsr 1`.
- `g_FastRendererSwitch` dual mode: frame generation off (the window slots
  share present code). Upscaling may stay on.

## Inputs FSR needs, per path

### RTX

The tracer already has what Q2RTX's TAAU uses:

- Color: `FLAT_COLOR` (HDR, linear, render resolution). Use the HDR flag.
- Motion vectors: `FLAT_MOTION` (check the unit and sign; FSR wants
  render-resolution pixels or a scale via `motionVectorScale`).
- Depth: `PT_VIEW_DEPTH_A/B` (linear view depth). FSR wants device depth;
  the builder converts it (a small compute pass) or uses the matching FSR
  flags. Infinite far plane: check the RTX projection.
- Jitter: `vk.taa_samples` (Halton) already exists for `AA_MODE_UPSCALE`.
  Use `ffxQueryDescUpscaleGetJitterPhaseCount` / `GetJitterOffset` or keep
  Halton with the matching phase count.
- Weapon FX layer `FLAT_FX` / `TAA_FX`: today it is added after the
  tonemap (see the RTX weapon fx memory note). It is not FSR input.
  `asvgf_taau.comp` writes `TAA_FX` itself (bilinear upscale of `FLAT_FX`,
  jitter removed), and also `IMG_TAA_A` (its history) and
  `readback.hdr_color` (centre-pixel readback). When FSR replaces the pass,
  these outputs must still be produced: an "FX-only" mode of
  `asvgf_taau` or a small new compute pass (builder's call). New GLSL is
  therefore expected.
- Denoiser modes (my call, 2026-11-13): FSR runs whenever `r_fsr 1`,
  also with `pt_denoiser` none. In reference accumulation mode
  (`enable_accumulation`) FSR is skipped and the final blit stretches the
  render-size image (debug mode only; no image re-creation at run time).
  Today `vk_rtx_taa_evaluate_settings()` gives `AA_MODE_OFF` when the
  denoiser is none; with `r_fsr 1` it must give the upscale mode instead.
- Reactive / transparency mask: from `PT_TRANSPARENT` alpha (particles,
  sprites, glass). Builder decides the exact mapping.
- Output: `TAA_OUTPUT` at `extent_taa_output`. Bloom and tone mapping run
  after it as today.
- `vk_rtx_get_render_extent()` returns 100 % today. With `r_fsr 1` it
  returns the FSR render size. The code for render < display (Q2RTX DRS
  leftovers) exists but has not run in this port; expect bugs there.
- Reset FSR history when the TAA history is invalidated
  (`vk_rtx_taa_invalidate_history`), on map load, camera cuts and teleports.

### Raster

What exists:

- `r_fbo 1` + `r_depthPrepass 1` + `r_velocityBuffer 1` make a G-buffer with depth and
  per-pixel motion vectors (`vk.gbuffer_velocity_image`), including skinned
  Ghoul2 (`gbuffer_skinned_vel.vert`). FSR forces these on.
- `r_renderScale` renders everything, 2D too, at a low size and blits. That
  is the wrong model for FSR; do not reuse it.

What is new:

- **Split frame.** The main 3D world view renders at the render size into
  scene images. At the end of that view (before the first 2D draw), FSR
  writes the full-size color image. 2D (HUD, menus, console, cinematics)
  then draws at full size. Only the first view with a world model gets FSR;
  other 3D views in the same frame (UI model previews, `RDF_NOWORLDMODEL`)
  render at full size as today.
- **Jitter** on the projection matrix of the FSR view only. Motion vectors
  must not contain the jitter, or the builder sets the FSR flag for jittered
  motion vectors. Portals, mirrors and sky portals of that view use the same
  jitter.
- **Reactive mask** for blended surfaces (sprites, glows, sabers, fx):
  these are not in the G-buffer, so they have no motion vectors. A first
  version can write the mask from blended draws in the main pass; the
  builder chooses the method. FSR (upscaling or FG) forces `r_fbo`,
  `r_depthPrepass` and `r_velocityBuffer` on for the raster path. Dynamic glow and bloom run on the upscaled
  image, after FSR.
- Post effects on the scene (SSAO, glow, bloom, PBR cubemaps): those that
  read the G-buffer run at the render size; those that make the final image
  run at the display size. The builder lists each one.

## Frame generation

- Uses the FSR frame generation swapchain (Vulkan): `ffxCreateContext` with
  `ffxCreateContextDescFrameGenerationSwapChainVK` replaces `vk.swapchain`;
  `ffxQueryDescSwapchainReplacementFunctionsVK` gives replacements for
  create/destroy swapchain, get images, acquire, present, HDR metadata. All
  calls in `vk_swapchain.cpp`, `vk_frame.cpp`, `vk_init.cpp` go through
  these pointers while FG is on.
- Queues: today the device has one queue (`vk.queue`). FG needs a game
  queue, a present queue (second graphics queue), an image-acquire queue and
  optionally an async compute queue. Request extra queues from the graphics
  family at device creation, only when `r_fsrFrameGen 1`.
- **HUD-less image** (required for a clean HUD): copy the frame at the
  split point (after the 3D scene, before 2D), passed through the same
  final transform as the back buffer (gamma / overbright / HDR output pass),
  in the back-buffer format. Give it as `HUDLessColor`.
- Per frame: `ffxConfigure` (`ffxConfigureDescFrameGeneration`, frame ID,
  enabled flag), then `ffxDispatch` with
  `ffxDispatchDescFrameGenerationPrepareV2` (depth, motion vectors, jitter,
  camera data), then present. Keep AMD's mandatory call order.
- Depth and motion vectors: the same images as the upscaler input. Without
  `r_fsr`, raster FG still needs them: FG forces `r_depthPrepass` and
  `r_velocityBuffer` on.
- Two FSR contexts: the swapchain context
  (`ffxCreateContextDescFrameGenerationSwapChainVK`) and the frame
  generation context (`ffxCreateContextDescFrameGeneration`).
- Frame pacing (my call): `com_maxfps` caps the real (rendered) frames, as
  today; the screen shows about twice that. The limiter is not changed.
  Vulkan has no latency waitable. Vsync: follow AMD's doc on FG + vsync.
- The swapchain is SDR today (`vk.rtx_surf_is_hdr = qfalse`) and
  `vkSetHdrMetadataEXT` is never called; the HDR-metadata replacement is
  stored but has no caller.
- Turn FG off (no interpolation) for one frame on camera cuts, map load,
  menus with no 3D, loading screens and cinematics (`reset` / not enabled).
- Teardown: wait for presents (`ffxDispatchDescFrameGenerationSwapChainWaitForPresentsVK`),
  destroy the FG context, restore the normal function pointers, before any
  swapchain re-creation (`vid_restart`, window resize, renderer switch).

## Menu

- Files: `ui/Setup.menu` and `ui/IngameSetup.menu` in
  `Jedi Academy - SWGL_DEV\GameData\SWGL\SWGL_Menu.pk3` (not in git).
  Back up the pk3 first. The pk3 has duplicate zip entries: read entries by
  `ZipInfo`. Set the date of each rewritten entry to now.
- Group `pg_video_vka` (Vulkan advanced), same style as the existing items
  (`ITEM_TYPE_MULTI`, `cvarFloatList`, `SubButton_Glow`, apply-changes
  flow for latched cvars as the other latched items do it):
  - Upscaling: Off / AMD FSR
  - FSR quality: Native AA / Quality / Balanced / Performance / Ultra Performance
  - FSR sharpness: slider or a short list (0, 0.25, 0.5, 0.75, 1)
  - Frame generation: Off / On
- Strings `@SWGL_SETUP_FSR_*` with `_DESC` text, in the same string file
  as the other `SWGL_SETUP_*` strings, for every language that file has.
- Rects: put the items in free rows of the page; do not overlap.

## Files (builder's call where not stated)

- Vendor the SDK at `lib/fsr/` (headers under `include/`, DLLs under
  `bin/`, `Kits/FidelityFX/docs/license.md`, and `3rdpartynotice.md` from
  the SDK repo root). The user decided (2026-11-13): the game launcher
  ships the DLLs to players. `lib/fsr/bin` is not in git (`.gitignore`);
  headers and license files are. Load
  `amd_fidelityfx_loader_vk.dll` at run time with `LoadLibrary` +
  `ffx_api_loader.h`; do not link the `.lib` (a missing DLL must not stop
  the renderer from loading).
- CMake copies the three DLLs next to the built executable for every
  config. The user copies binaries to `GameData` by hand.
- New renderer code in `code/rd-vulkan/vk_fsr.cpp` / `vk_fsr.h` (context
  life cycle, resource wrapping, dispatch), with hooks in the existing
  files. RTX glue may go in `rtx/vk_rtx_fsr.cpp`.

## Constraints from this project

- Comments: Simplified Technical English, one or two lines.
- Build Debug and Release (`DEBUG_SP.bat` builds `build/`); the user runs
  Debug. Never work in a separate worktree.
- Shaders: regenerate `shader_data.c` / `shader_data_rtx.c` per the memory
  notes (compile one by one, check the symbol count). New GLSL is
  expected: RTX FX-only upscale, RTX depth conversion, raster reactive mask.
- `msys sed -i` turns CRLF files to LF. Do not use it on sources.
- In-game tests: Release, `+devmap mp/duel1`, the unattended test instance
  (scratchpad bin dir + junctions to SWGL_DEV data, isolated homepath).
  The user allowed these runs for this work on 2026-11-13. Never touch the
  user's own install binaries or homepath.
- Vulkan validation must stay clean in Debug with FSR on and off.

## Order of work

1. SDK vendoring, loader, `vk_fsr` skeleton, cvars, graceful fallback.
2. RTX upscaling (the RTX path already has jitter, motion and the
   render < display plumbing).
3. Raster upscaling (split frame, jitter, mask, mip bias).
4. Frame generation (queues, swapchain replacement, HUD-less copy).
5. Menu and strings.

Each step builds in Debug and Release and passes review before the next.

## Left to the builder

- Exact FSR flags (HDR, inverted depth, infinite far, auto exposure).
- The depth conversion method on RTX.
- The reactive mask method on raster.
- Which raster post effects run at render size vs display size.
- Exact menu rects and the sharpness widget.

## Risks to tell the user

- OpenJK is GPLv2. Loading AMD's closed DLLs at run time is the usual
  practice for FSR/DLSS in GPL games, but it is a license question for the
  project owner.
- In-game quality and FG pacing are not verified until the user tests.
