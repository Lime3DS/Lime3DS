#!/bin/bash -ex

# This script assumes that Docker is installed

docker run -it --rm -v $(pwd):/mnt --platform=linux/arm64 opensauce04/azahar-build-environment:arm64
