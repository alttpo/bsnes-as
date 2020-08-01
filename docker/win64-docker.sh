#!/bin/bash
set -e
docker build -t clang-build-win64 clang-build-win64
docker volume create clang-win64-volume
docker rm clang-win64
docker create --name clang-win64 -v clang-win64-volume:/app -w /app -it clang-build-win64
docker run clang-build-win64 -c "rm -rf /app/.git"
docker cp `pwd`/.git clang-win64:/app/
git diff --binary --no-color > tmp-patch
docker cp `pwd`/tmp-patch clang-win64:/app/tmp-patch
docker start -ia clang-win64
