@echo on

setlocal enabledelayedexpansion
set sdk=%1
set cfg=%2

third_party\ninja\ninja -C out\%cfg%

echo sdk is set to %sdk%
echo cfg is set to %cfg%

if not exist ".\..\install\%sdk%-%cfg%\include\skia" mkdir ..\..\install\%sdk%-%cfg%\include\skia
xcopy include ..\..\install\%sdk%-%cfg%\include\skia\ /E /I /Y
xcopy modules ..\..\install\%sdk%-%cfg%\include\skia\ /E /I /Y
copy /Y out\%cfg%\skia.lib ..\..\install\%sdk%-%cfg%\lib\
