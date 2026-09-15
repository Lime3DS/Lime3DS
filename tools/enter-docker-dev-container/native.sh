#!/bin/bash -ex

# This script assumes that Docker is installed

docker run -it --rm -v $(pwd):/mnt opensauce04/azahar-build-environment:$(uname -m)
