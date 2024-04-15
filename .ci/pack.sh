#!/bin/bash -ex

# Determine the full revision name.
GITDATE="`git show -s --date=short --format='%ad' | sed 's/-//g'`"
GITREV="`git show -s --format='%h'`"
REV_NAME="lime3ds-$OS-$TARGET-$GITDATE-$GITREV"

# Determine the name of the release being built.
if [[ "$GITHUB_REF_TYPE" == "tag" ]]; then
    RELEASE_NAME=lime3ds-$GITHUB_REF_NAME
    REV_NAME="lime3ds-$GITHUB_REF_NAME-$OS-$TARGET"
else
    RELEASE_NAME=lime3ds-head
fi

# Archive and upload the artifacts.
mkdir artifacts

function pack_artifacts() {
    ARTIFACTS_PATH="$1"

    # Set up root directory for archive.
    mkdir "$REV_NAME"
    if [ -f "$ARTIFACTS_PATH" ]; then
        mv "$ARTIFACTS_PATH" "$REV_NAME"

        # Use file extension to differentiate archives.
        FILENAME=$(basename "$ARTIFACT")
        EXTENSION="${FILENAME##*.}"
        ARCHIVE_NAME="$REV_NAME.$EXTENSION"
    else
        mv "$ARTIFACTS_PATH"/* "$REV_NAME"

        ARCHIVE_NAME="$REV_NAME"
    fi

    # Create .zip/.tar.gz
    if [ "$OS" = "windows" ]; then
        ARCHIVE_FULL_NAME="$ARCHIVE_NAME.zip"
        powershell Compress-Archive "$REV_NAME" "$ARCHIVE_FULL_NAME"
    elif [ "$OS" = "android" ]; then
        ARCHIVE_FULL_NAME="$ARCHIVE_NAME.zip"
        zip -r "$ARCHIVE_FULL_NAME" "$REV_NAME"
    else
        ARCHIVE_FULL_NAME="$ARCHIVE_NAME.tar.gz"
        tar czvf "$ARCHIVE_FULL_NAME" "$REV_NAME"
    fi
    mv "$ARCHIVE_FULL_NAME" artifacts/
     # Clean up created rev artifacts directory.
    rm -rf "$REV_NAME"
}

if [ -n "$UNPACKED" ]; then
    # Copy the artifacts to be uploaded unpacked.
    for ARTIFACT in build/bundle/*; do
        FILENAME=$(basename "$ARTIFACT")
        EXTENSION="${FILENAME##*.}"

        mv "$ARTIFACT" "artifacts/$REV_NAME.$EXTENSION"
    done
elif [ -n "$PACK_INDIVIDUALLY" ]; then
    # Pack and upload the artifacts one-by-one.
    for ARTIFACT in build/bundle/*; do
        pack_artifacts "$ARTIFACT"
    done
else
    # Pack all of the artifacts into a single archive.
    pack_artifacts build/bundle
fi
