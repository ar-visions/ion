# use project's depot_tools
export DEPOT_TOOLS_UPDATE=0
export PATH="$(pwd)/../depot_tools:$PATH"

PYTHON3="python3"
GN="gn"
NINJA="ninja"
sdk=$1
cxx="clang++"
cc="clang"

gclient sync

if [ -n "$MSYSTEM" ]; then
    PYTHON3="/mingw64/bin/python3"
    GN="/mingw64/bin/gn"
    NINJA="/mingw64/bin/ninja"
fi

if [ $sdk != "native" ]; then
    cxx="$sdk-g++"
    cc="$sdk-gcc"
fi

$GN gen out/Default "--args=is_debug=false"
$NINJA -C out/Default