#============================================================================
# Copyright (C) 2013 - 2018, OpenJK contributors
#
# This file is part of the OpenJK source code.
#
# OpenJK is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#============================================================================

# Make sure the user is not executing this script directly
if(NOT InOpenJK)
	message(FATAL_ERROR "Use the top-level cmake script!")
endif(NOT InOpenJK)

# Check for dependencies first

set(SPVulkanRendererIncludeDirectories ${SPDir})
set(SPVulkanRendererIncludeDirectories
	${SPVulkanRendererIncludeDirectories}
	${SharedDir}
	${SPDir}
	"${SPDir}/rd-vulkan"
	"${SPDir}/rd-vulkan/vulkan"
	"${GSLIncludeDirectory}"
	)

set(SPVulkanRendererFiles
	"${SPDir}/rd-vulkan/G2_API.cpp"
	"${SPDir}/rd-vulkan/G2_bolts.cpp"
	"${SPDir}/rd-vulkan/G2_bones.cpp"
	"${SPDir}/rd-vulkan/G2_gore_r2.cpp"
	"${SPDir}/rd-vulkan/G2_gore_r2.h"
	"${SPDir}/rd-vulkan/G2_misc.cpp"
	"${SPDir}/rd-vulkan/G2_surfaces.cpp"
	"${SPDir}/rd-vulkan/tr_backend.cpp"
	"${SPDir}/rd-vulkan/tr_bsp.cpp"
	"${SPDir}/rd-vulkan/tr_cache.h"
	"${SPDir}/rd-vulkan/tr_cache.cpp"
	"${SPDir}/rd-vulkan/tr_cmds.cpp"
	"${SPDir}/rd-vulkan/tr_curve.cpp"
	"${SPDir}/rd-vulkan/tr_decals.cpp"
	"${SPDir}/rd-vulkan/tr_ghoul2.cpp"
	"${SPDir}/rd-vulkan/tr_ghoul2_phys.cpp"
	"${SPDir}/rd-vulkan/tr_image.cpp"
	"${SPDir}/rd-vulkan/tr_init.cpp"
	"${SPDir}/rd-vulkan/tr_light.cpp"
	"${SPDir}/rd-vulkan/tr_local.h"
	"${SPDir}/rd-vulkan/tr_main.cpp"
	"${SPDir}/rd-vulkan/tr_marks.cpp"
	"${SPDir}/rd-vulkan/tr_mesh.cpp"
	"${SPDir}/rd-vulkan/tr_model.cpp"
	"${SPDir}/rd-vulkan/tr_quicksprite.cpp"
	"${SPDir}/rd-vulkan/tr_quicksprite.h"
	"${SPDir}/rd-vulkan/tr_scene.cpp"
	"${SPDir}/rd-vulkan/tr_shade.cpp"
	"${SPDir}/rd-vulkan/tr_shader.cpp"
	"${SPDir}/rd-vulkan/tr_shade_calc.cpp"
	"${SPDir}/rd-vulkan/tr_shadows.cpp"
	"${SPDir}/rd-vulkan/tr_skin.cpp"
	"${SPDir}/rd-vulkan/tr_sky.cpp"
	"${SPDir}/rd-vulkan/tr_subs.cpp"
	"${SPDir}/rd-vulkan/tr_surface.cpp"
	"${SPDir}/rd-vulkan/tr_surfacesprites.cpp"
	"${SPDir}/rd-vulkan/tr_world.cpp"
	"${SPDir}/rd-vulkan/tr_visgen.cpp"
	"${SPDir}/rd-vulkan/tr_visgen.h"
	"${SPDir}/rd-vulkan/tr_WorldEffects.cpp"
	"${SPDir}/rd-vulkan/tr_WorldEffects.h"
	"${SPDir}/rd-vulkan/vk_attachments.cpp"
	"${SPDir}/rd-vulkan/vk_bloom.cpp"
	"${SPDir}/rd-vulkan/vk_cmd.cpp"
	"${SPDir}/rd-vulkan/vk_debug.cpp"
	"${SPDir}/rd-vulkan/vk_dynamic_glow.cpp"
	"${SPDir}/rd-vulkan/vk_flares.cpp"
	"${SPDir}/rd-vulkan/vk_frame.cpp"
	"${SPDir}/rd-vulkan/vk_image.cpp"
	"${SPDir}/rd-vulkan/vk_normalmap.cpp"
	"${SPDir}/rd-vulkan/vk_perf.cpp"
	"${SPDir}/rd-vulkan/vk_perf.h"
	"${SPDir}/rd-vulkan/vk_mikktspace.cpp"
	"${SPDir}/rd-vulkan/vk_pbr.cpp"
	"${SPDir}/rd-vulkan/vk_cubemap.cpp"
"${SPDir}/rd-vulkan/vk_image_process.cpp"
	"${SPDir}/rd-vulkan/vk_info.cpp"
	"${SPDir}/rd-vulkan/vk_init.cpp"
	"${SPDir}/rd-vulkan/vk_instance.cpp"
	"${SPDir}/rd-vulkan/vk_local.h"
	"${SPDir}/rd-vulkan/vk_pipelines.cpp"
	"${SPDir}/rd-vulkan/vk_shaders.cpp"
	"${SPDir}/rd-vulkan/vk_shade_geometry.cpp"
	"${SPDir}/rd-vulkan/vk_swapchain.cpp"
	"${SPDir}/rd-vulkan/vk_vbo.cpp"
	"${SPDir}/rd-vulkan/vk_vbo_surfacesprites.cpp"
	)
source_group("renderer" FILES ${SPVulkanRendererFiles})

set(SPVulkanRendererGhoul2Files
	"${SPDir}/ghoul2/G2.h"
	"${SPDir}/ghoul2/ghoul2_gore.h")
source_group("ghoul2" FILES ${SPVulkanRendererGhoul2Files})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererGhoul2Files})

set(SPVulkanRendererRdCommonFiles
	"${SPDir}/rd-common/mdx_format.h"
	"${SPDir}/rd-common/mdx_merge.h"
	"${SPDir}/rd-common/phys_cloth.cpp"
	"${SPDir}/rd-common/phys_cloth.h"
	"${SPDir}/rd-common/tr_common.h"
	"${SPDir}/rd-common/tr_font.cpp"
	"${SPDir}/rd-common/tr_font.h"
	"${SPDir}/rd-common/tr_glamerge.cpp"
	"${SPDir}/rd-common/tr_image_load.cpp"
	"${SPDir}/rd-common/tr_image_jpg.cpp"
	"${SPDir}/rd-common/tr_image_tga.cpp"
	"${SPDir}/rd-common/tr_image_png.cpp"
	"${SPDir}/rd-common/tr_noise.cpp"
	"${SPDir}/rd-common/tr_public.h"
	"${SPDir}/rd-common/tr_types.h")
source_group("rd-common" FILES ${SPVulkanRendererRdCommonFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererRdCommonFiles})

set(SPVulkanRendererVulkanAssets
	"${SPDir}/rd-vulkan/vulkan/vk_platform.h"
	"${SPDir}/rd-vulkan/vulkan/vulkan.h"
	"${SPDir}/rd-vulkan/vulkan/vulkan_core.h"
	"${SPDir}/rd-vulkan/vulkan/vulkan_win32.h"
	"${SPDir}/rd-vulkan/vulkan/vulkan_xlib.h"
	"${SPDir}/rd-vulkan/vulkan/vulkan_xlib_xrandr.h")
source_group("renderer\\vulkan" FILES ${SPVulkanRendererVulkanAssets})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererVulkanAssets})

set(SPVulkanRendererVulkanVideoAssets
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codecs_common.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_av1std.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_av1std_decode.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_av1std_encode.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h264std.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h264std_decode.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h264std_encode.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h265std.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h265std_decode.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h265std_encode.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_vp9std.h"
    "${SPDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_vp9std_decode.h"
)
source_group("renderer\\vulkan\\vk_video" FILES ${SPVulkanRendererVulkanVideoAssets})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererVulkanVideoAssets})

set(SPVulkanRendererUtils
	"${SPDir}/rd-vulkan/utils/stb_dxt.h"
	"${SPDir}/rd-vulkan/utils/stb_image.h")
source_group("renderer\\utils" FILES ${SPVulkanRendererUtils})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererUtils})

set(SPVulkanRendererUtilsMikktFiles
	"${SPDir}/rd-vulkan/utils/mikktspace/mikktspace.h"
	"${SPDir}/rd-vulkan/utils/mikktspace/mikktspace.c")
source_group("renderer\\utils\\mikktspace" FILES ${SPVulkanRendererUtilsMikktFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererUtilsMikktFiles})

# Path tracer, ported from JKSunny/EternalJK rtx-update (itself from NVIDIA's Q2RTX).
# Compiled in only when USE_RTX is defined; the raster path is untouched either way.
file(GLOB SPVulkanRendererRtxFiles
	"${SPDir}/rd-vulkan/rtx/*.cpp"
	"${SPDir}/rd-vulkan/rtx/*.h")
source_group("renderer\\rtx" FILES ${SPVulkanRendererRtxFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererRtxFiles})

set(SPVulkanRendererCommonFiles
	"${SPDir}/qcommon/matcomp.cpp"
	"${SPDir}/qcommon/q_shared.cpp"
	"${SPDir}/qcommon/q_shared.h"
	"${SPDir}/qcommon/ojk_i_saved_game.h"
	"${SPDir}/qcommon/ojk_saved_game_class_archivers.h"
	"${SPDir}/qcommon/ojk_saved_game_helper.h"
	"${SPDir}/qcommon/ojk_saved_game_helper_fwd.h"
	"${SPDir}/qcommon/ojk_scope_guard.h"
	${SharedCommonFiles})
source_group("common" FILES ${SPVulkanRendererCommonFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererCommonFiles})

set(SPVulkanRendererCommonSafeFiles
	${SharedCommonSafeFiles}
	)
source_group("common/safe" FILES ${SPVulkanRendererCommonSafeFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererCommonSafeFiles})

# Transparently use either bundled or system libjpeg.
list(APPEND SPVulkanRendererIncludeDirectories ${JPEG_INCLUDE_DIR})
list(APPEND SPVulkanRendererLibraries          ${JPEG_LIBRARIES})

# Transparently use either bundled or system libpng.  Order is important --
# libpng used zlib, so it must come before it on the linker command line.
list(APPEND SPVulkanRendererIncludeDirectories ${PNG_INCLUDE_DIRS})
list(APPEND SPVulkanRendererLibraries          ${PNG_LIBRARIES})

# Transparently use either bundled or system zlib.
list(APPEND SPVulkanRendererIncludeDirectories ${ZLIB_INCLUDE_DIR})
list(APPEND SPVulkanRendererLibraries          ${ZLIB_LIBRARIES})

# Transparently use our bundled minizip.
list(APPEND SPVulkanRendererIncludeDirectories ${MINIZIP_INCLUDE_DIRS})
list(APPEND SPVulkanRendererLibraries          ${MINIZIP_LIBRARIES})

# G2_*.cpp (ported from code/rd-vanilla) use bare #include "tr_common.h" etc.
list(APPEND SPVulkanRendererIncludeDirectories "${SPDir}/rd-common")

#find_package(OpenGL REQUIRED)
#set(SPVulkanRendererIncludeDirectories ${SPVulkanRendererIncludeDirectories} ${OPENGL_INCLUDE_DIR})
#set(SPVulkanRendererLibraries ${SPVulkanRendererLibraries} ${OPENGL_LIBRARIES})

set(SPVulkanRendererIncludeDirectories ${SPVulkanRendererIncludeDirectories} ${OpenJKLibDir})

set(SPVulkanDefines ${SharedDefines} "Q_MATH_INLINE" "RENDERER")

# NVIDIA NRD v4.17.3, compiled into the renderer DLL (see nrd/README.md).
# A separate OBJECT library keeps the C++17 setting and the NRD defines away from the renderer files.
set(SPVulkanNrdDir "${SPDir}/rd-vulkan/nrd")
set(SPVulkanNrdFiles
	"${SPVulkanNrdDir}/Source/InstanceImpl.cpp"
	"${SPVulkanNrdDir}/Source/Reblur.cpp"
	"${SPVulkanNrdDir}/Source/Reference.cpp"
	"${SPVulkanNrdDir}/Source/Relax.cpp"
	"${SPVulkanNrdDir}/Source/Sigma.cpp"
	"${SPVulkanNrdDir}/Source/Timer.cpp"
	"${SPVulkanNrdDir}/Source/Wrapper.cpp"
	"${SPVulkanNrdDir}/ShaderMake/ShaderBlob.cpp")
set(SPVulkanNrdIncludeDirectories
	"${SPVulkanNrdDir}/Include"
	"${SPVulkanNrdDir}/MathLib"
	"${SPVulkanNrdDir}"
	"${SPVulkanNrdDir}/Spirv")
set(SPVulkanNrdDefines
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	_CRT_SECURE_NO_WARNINGS
	NRD_STATIC_LIBRARY=1
	NRD_EMBEDS_SPIRV_SHADERS=1
	NRD_EMBEDS_DXIL_SHADERS=0
	NRD_EMBEDS_DXBC_SHADERS=0
	SPIRV_SREG_OFFSET=0
	SPIRV_BREG_OFFSET=2
	SPIRV_UREG_OFFSET=3
	SPIRV_TREG_OFFSET=20
	NRD_SUPPORTS_VIEWPORT_OFFSET=0
	NRD_SUPPORTS_CHECKERBOARD=1
	NRD_SUPPORTS_HISTORY_CONFIDENCE=1
	NRD_SUPPORTS_DISOCCLUSION_THRESHOLD_MIX=1
	NRD_SUPPORTS_ANTIFIREFLY=1
	REBLUR_PERFORMANCE_MODE=0)
source_group("renderer\nrd" FILES ${SPVulkanNrdFiles})
add_library(${SPVulkan}_nrd OBJECT ${SPVulkanNrdFiles})
set_target_properties(${SPVulkan}_nrd PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE ON)
target_include_directories(${SPVulkan}_nrd PRIVATE ${SPVulkanNrdIncludeDirectories})
target_compile_definitions(${SPVulkan}_nrd PRIVATE ${SPVulkanNrdDefines})
if(MSVC)
	target_compile_options(${SPVulkan}_nrd PRIVATE /wd4324)
endif()
# The renderer includes NRD.h for vk_rtx_nrd.cpp.
list(APPEND SPVulkanRendererIncludeDirectories "${SPVulkanNrdDir}/Include")

add_library(${SPVulkan} SHARED ${SPVulkanRendererFiles} $<TARGET_OBJECTS:${SPVulkan}_nrd>)

if(NOT MSVC)
	# remove "lib" prefix for .so/.dylib files
	set_target_properties(${SPVulkan} PROPERTIES PREFIX "")
endif()

if(WIN32)
	install(TARGETS ${SPVulkan}
		RUNTIME
		DESTINATION ${JKAInstallDir}
		COMPONENT ${JKASPClientComponent})
else(WIN32)
	if(MakeApplicationBundles AND BuildSPEngine)
		install(TARGETS ${SPVulkan}
			LIBRARY
			DESTINATION "${JKAInstallDir}/${SPEngine}.app/Contents/MacOS/"
			COMPONENT ${JKASPClientComponent})
	else()
		install(TARGETS ${SPVulkan}
			LIBRARY
			DESTINATION ${JKAInstallDir}
			COMPONENT ${JKASPClientComponent})
	endif()
endif()

set_target_properties(${SPVulkan} PROPERTIES COMPILE_DEFINITIONS "${SPVulkanDefines}")

# Hide symbols not explicitly marked public.
set_property(TARGET ${SPVulkan} APPEND PROPERTY COMPILE_OPTIONS ${OPENJK_VISIBILITY_FLAGS})

set_target_properties(${SPVulkan} PROPERTIES INCLUDE_DIRECTORIES "${SPVulkanRendererIncludeDirectories}")
set_target_properties(${SPVulkan} PROPERTIES PROJECT_LABEL "SP Vulkan Renderer")
target_link_libraries(${SPVulkan} ${SPVulkanRendererLibraries})
