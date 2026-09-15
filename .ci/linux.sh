#!/bin/bash -ex

if [[ "$TARGET" == "appimage" ]] || [[ "$TARGET" == "clang"* ]]; then
    # Compile the AppImage we distribute with Clang.
    export EXTRA_CMAKE_FLAGS=(-DCMAKE_CXX_COMPILER=clang++
                              -DCMAKE_C_COMPILER=clang
                              -DCMAKE_LINKER=/etc/bin/ld.lld
                              -DENABLE_ROOM_STANDALONE=OFF)
    # Bundle required QT wayland libraries
    export EXTRA_QT_PLUGINS="waylandcompositor"
    export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
else
    # For the linux-fresh verification target, verify compilation without PCH as well.
    export EXTRA_CMAKE_FLAGS=(-DCITRA_USE_PRECOMPILED_HEADERS=OFF)
fi

if [ "$GITHUB_REF_TYPE" == "tag" ]; then
    export EXTRA_CMAKE_FLAGS=("${EXTRA_CMAKE_FLAGS[@]}" -DENABLE_QT_UPDATE_CHECKER=ON)
fi

mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DENABLE_ROOM_STANDALONE=OFF \
    -DENABLE_DISCORD_RPC=ON \
    "${EXTRA_CMAKE_FLAGS[@]}"
ninja
strip -s bin/Release/*

if [ "$TARGET" == "appimage" ]; then
    ninja bundle
    # TODO: Our AppImage environment currently uses an older ccache version without the verbose flag.
    ccache -s
else
    ccache -s -v
fi

ctest -VV -C Release
