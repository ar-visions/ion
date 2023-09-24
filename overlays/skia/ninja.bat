@echo on

setlocal enabledelayedexpansion
set sdk=%1

third_party\ninja\ninja -C out\Vulkan

echo Sdk is set to %sdk%

xcopy include ..\..\install\%sdk%\include\skia /E /I /Y
copy /Y out\Vulkan\skia.lib ..\..\install\%sdk%\lib\
