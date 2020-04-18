#!/bin/bash
# exit on any command failure
set -e

# compile!
docker build . -t bsnes-win64

# extract bsnes.exe and build-context.tar.gz:
id=$(docker create bsnes-win64)
docker cp $id:/build/bsnes.exe /Volumes/Users/Jaymz/Desktop/alttp-multiplayer-nightly/
docker cp $id:/build/win64-build-context.tar.gz .
docker rm $id
