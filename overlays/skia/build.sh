# use project's depot_tools
export DEPOT_TOOLS_UPDATE=0
export PATH="$(pwd)/../depot_tools:$PATH"

PYTHON3="python3"
GN="gn"
NINJA="ninja"
sdk=$1
cfg=$2
cxx="clang++"
cc="clang"
dbg="true"


if [ -n "$MSYSTEM" ]; then
    PYTHON3="/mingw64/bin/python3"
    GN="/mingw64/bin/gn"
    NINJA="/mingw64/bin/ninja"
fi

if [ $sdk != "native" ]; then
    cxx="$sdk-g++"
    cc="$sdk-gcc"
fi

if [ $cfg = "release" ]; then
    dbg="false"
fi

# run the command to sync dependencies
$PYTHON3 tools/git-sync-deps

$GN gen out/Vulkan --args='is_official_build=false cxx="$cxx" cc="$cc" skia_use_vulkan=true skia_use_vma=true skia_enable_gpu=true skia_enable_tools=false skia_use_gl=false skia_use_expat=true skia_enable_fontmgr_empty=false skia_enable_svg=true skia_use_icu=true is_debug=$dbg is_component_build=false is_trivial_abi=false werror=true skia_use_fonthost_mac=false skia_use_angle=false skia_use_dng_sdk=false skia_use_dawn=false skia_use_webgl=false skia_use_webgpu=false skia_use_fontconfig=false skia_use_freetype=true skia_use_libheif=false skia_use_libjpeg_turbo_decode=false skia_use_libjpeg_turbo_encode=false skia_use_no_jpeg_encode=false skia_use_libpng_decode=true skia_use_libpng_encode=true skia_use_no_png_encode=false skia_use_libwebp_decode=false skia_use_libwebp_encode=false skia_use_no_webp_encode=false  skia_use_lua=false skia_use_piex=false skia_use_system_freetype2=false skia_use_system_libpng=false skia_use_system_zlib=false skia_use_wuffs=false skia_use_zlib=false skia_enable_ganesh=true skia_build_for_debugger=false skia_enable_sksl_tracing=false skia_enable_skshaper=false skia_enable_skparagraph=false skia_enable_pdf=false skia_canvaskit_force_tracing=false skia_canvaskit_profile_build=false skia_canvaskit_enable_skp_serialization=false skia_canvaskit_enable_effects_deserialization=false skia_canvaskit_enable_skottie=false skia_canvaskit_include_viewer=false skia_canvaskit_enable_pathops=false skia_canvaskit_enable_rt_shader=false skia_canvaskit_enable_matrix_helper=false skia_canvaskit_enable_canvas_bindings=false skia_canvaskit_enable_font=false skia_canvaskit_enable_embedded_font=false skia_canvaskit_enable_alias_font=false skia_canvaskit_legacy_draw_vertices_blend_mode=false skia_canvaskit_enable_debugger=false skia_canvaskit_enable_paragraph=false skia_canvaskit_enable_webgl=false skia_canvaskit_enable_webgpu=false'

# run build
$NINJA -C out/Vulkan