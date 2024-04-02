#!/bin/bash -ex

export NDK_CCACHE=$(which ccache)
BUILD_FLAVOR=nightly

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    export ANDROID_KEYSTORE_FILE="${GITHUB_WORKSPACE}/ks.jks"
    base64 --decode <<< "${ANDROID_KEYSTORE_B64}" > "${ANDROID_KEYSTORE_FILE}"
fi

cd src/android
chmod +x ./gradlew
./gradlew assemble${BUILD_FLAVOR}Release
./gradlew bundle${BUILD_FLAVOR}Release

ccache -s -v

if [ ! -z "${ANDROID_KEYSTORE_B64}" ]; then
    rm "${ANDROID_KEYSTORE_FILE}"
fi
