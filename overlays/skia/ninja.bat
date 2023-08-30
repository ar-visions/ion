@echo on

third_party\ninja\ninja -C out\Vulkan

xcopy include ..\..\install\include\skia /E /I /Y
copy /Y out\Vulkan\skia.lib ..\..\install\lib\
