# this is a 'resource' directory in extern without a version
# having issue with depot_tools, disabling for now
#export PATH="$(pwd)/../depot_tools:$PATH"

# run the command to sync dependencies
python3 tools/git-sync-deps

# configure with these (im sure this is only 12% of skia configurables) NOT debug
# trivial abi is what gets me.. who wouldnt want a trivial abi.. who rationalizes having an option for that
# everything is good but your abi looks too trivial for us
gn gen out/Vulkan --args='
is_official_build=false skia_enable_tools=false skia_use_gl=false skia_use_expat=false 
skia_enable_fontmgr_empty=true skia_enable_svg=false skia_use_icu=false is_debug=true 
is_component_build=false is_trivial_abi=true werror=true skia_use_fonthost_mac=false 
skia_use_angle=false skia_use_dng_sdk=false skia_use_dawn=false skia_use_webgl=false 
skia_use_webgpu=false skia_use_expat=false skia_use_fontconfig=false skia_use_freetype=true 
skia_use_libheif=false skia_use_libjpeg_turbo_decode=false skia_use_libjpeg_turbo_encode=false 
skia_use_no_jpeg_encode=false skia_use_libpng_decode=false skia_use_libpng_encode=false 
skia_use_no_png_encode=false skia_use_libwebp_decode=false skia_use_libwebp_encode=false 
skia_use_no_webp_encode=false  skia_use_lua=false skia_use_piex=false skia_use_system_freetype2=false 
skia_use_system_libpng=false skia_use_system_zlib=false skia_use_vulkan=true skia_use_wuffs=false 
skia_use_zlib=false skia_enable_ganesh=true skia_enable_sksl=true skia_build_for_debugger=false 
skia_enable_sksl_tracing=false skia_enable_skshaper=false skia_enable_skparagraph=false skia_enable_pdf=false 
skia_canvaskit_force_tracing=false skia_canvaskit_profile_build=false skia_canvaskit_enable_skp_serialization=false 
skia_canvaskit_enable_effects_deserialization=false skia_canvaskit_enable_skottie=false 
skia_canvaskit_include_viewer=false skia_canvaskit_enable_pathops=false skia_canvaskit_enable_rt_shader=false 
skia_canvaskit_enable_matrix_helper=false skia_canvaskit_enable_canvas_bindings=false skia_canvaskit_enable_font=false 
skia_canvaskit_enable_embedded_font=false skia_canvaskit_enable_alias_font=false 
skia_canvaskit_legacy_draw_vertices_blend_mode=false skia_canvaskit_enable_debugger=false 
skia_canvaskit_enable_paragraph=false skia_canvaskit_enable_webgl=false skia_canvaskit_enable_webgpu=false skia_use_vma=false'

# run build
ninja -C out/Vulkan