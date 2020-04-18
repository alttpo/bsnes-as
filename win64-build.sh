#!/bin/bash
docker build . -t bsnes-win64
id=$(docker create bsnes-win64)
docker cp $id:/build/bsnes.exe /Volumes/Users/Jaymz/Desktop/alttp-multiplayer-nightly/
docker cp $id:/build/win64-build-context.tar.gz .
docker rm $id
