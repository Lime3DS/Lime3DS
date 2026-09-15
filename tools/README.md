# Tools

This directory contains several scripts which are intended to both document and ease the convenience of certain development processes.

Most of the scripts in this directory assume that your current working directory is the Azahar root directory and the script is being called via `./tools/xxx.sh`.

## Pre-release checklist

- [ ] Update compatibility list
- [ ] Update translations if either of the following apply:
    - This is a major release (e.g. 2125.x --> 2126.0)
    - This is a minor release, but the release branch hasn't yet diverged from master

## Post-release checklist

- [ ] Publish to Google Play Store
- [ ] Publish files to the official Internet Archive account (incl. changelog as .md file)
- [ ] Publish to Flathub
- [ ] Force-push tagged commit to retroarch-live branch

### Note:

For reasons unknown, some part of the translation update process can inexplicably produce files with different content depending on the environment in which it is running, even when using the same version of the tool and the same distro.

For consistency, when updating the translations to be committed to the repository, always perform the update within the the Docker environment we use for Azahar's CI (`opensauce04/azahar-build-environment:latest`).
