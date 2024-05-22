#!/bin/bash -ex

ARTIFACTS_LIST=($ARTIFACTS)

BUNDLE_DIR=build/bundle
mkdir build

# Set up the base artifact to combine into.
BASE_ARTIFACT=${ARTIFACTS_LIST[0]}
BASE_ARTIFACT_ARCH="${BASE_ARTIFACT##*-}"
mv $BASE_ARTIFACT $BUNDLE_DIR

# Executable binary paths that need to be combined.
BIN_PATHS=(lime3ds-cli lime3ds-room lime3ds-gui.app/Contents/MacOS/lime3ds-gui)

# Dylib paths that need to be combined.
IFS=$'\n'
DYLIB_PATHS=($(cd $BUNDLE_DIR && find . -name '*.dylib'))
unset IFS

# Combine all of the executable binaries and dylibs.
for OTHER_ARTIFACT in "${ARTIFACTS_LIST[@]:1}"; do
    OTHER_ARTIFACT_ARCH="${OTHER_ARTIFACT##*-}"

    for BIN_PATH in "${BIN_PATHS[@]}"; do
        lipo -create -output $BUNDLE_DIR/$BIN_PATH $BUNDLE_DIR/$BIN_PATH $OTHER_ARTIFACT/$BIN_PATH
    done

    for DYLIB_PATH in "${DYLIB_PATHS[@]}"; do
        # Only merge if the libraries do not have conflicting arches, otherwise it will fail.
        DYLIB_INFO=`file $BUNDLE_DIR/$DYLIB_PATH`
        OTHER_DYLIB_INFO=`file $OTHER_ARTIFACT/$DYLIB_PATH`
        if ! [[ "$DYLIB_INFO" =~ "$OTHER_ARTIFACT_ARCH" ]] && ! [[ "$OTHER_DYLIB_INFO" =~ "$BASE_ARTIFACT_ARCH" ]]; then
            lipo -create -output $BUNDLE_DIR/$DYLIB_PATH $BUNDLE_DIR/$DYLIB_PATH $OTHER_ARTIFACT/$DYLIB_PATH
        fi
    done
done

# Re-sign executables and bundles after combining.
APP_PATHS=(lime3ds-cli lime3ds-room lime3ds-gui.app)
for APP_PATH in "${APP_PATHS[@]}"; do
    codesign --deep -fs - $BUNDLE_DIR/$APP_PATH
done
