FROM purplekarrot/mingw-w64-x86-64:latest AS build-base

RUN apt-get update && apt-get install -y zip

ENTRYPOINT /bin/bash

#######################################################
FROM build-base

WORKDIR /build

ADD win64-build-context.tar.gz /build

COPY . /build

RUN mkdir -p /build/bsnes/out && mkdir -p /build/bsnes/obj

RUN make -C bsnes -j4 local=false build=performance_debug console=true platform=windows compiler="x86_64-w64-mingw32-g++" windres="x86_64-w64-mingw32-windres"

RUN tar czf win64-build-context.tar.gz ./bsnes/out ./bsnes/obj

RUN cp -a bsnes/out/bsnes bsnes.exe
