#!/bin/bash -ex

# This script assumes that Docker is installed

docker run -it --rm -v $(pwd):/mnt --platform=linux/amd64 opensauce04/azahar-build-environment:x86_64
