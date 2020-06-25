FROM ubuntu:18.04 AS build-base

RUN apt-get update && apt-get -qq install -y --no-install-recommends ca-certificates git make cmake clang g++-mingw-w64-x86-64

RUN mkdir -p /tmp/wclang /tmp/bin

RUN git clone https://github.com/tpoechtrager/wclang /tmp/wclang ; \
  cd /tmp/wclang ; \
  cmake -DCMAKE_INSTALL_PREFIX=/tmp . ; \
  make -j8 ; \
  make install

#######################################################
FROM build-base

WORKDIR /build

COPY . /build

RUN mkdir -p /build/bsnes/out && mkdir -p /build/bsnes/obj

RUN make -C bsnes -j6 local=false build=performance_debug console=true platform=windows compiler="x86_64-w64-mingw32-g++-posix" windres="x86_64-w64-mingw32-windres"

RUN tar czf win64-build-context.tar.gz ./bsnes/out ./bsnes/obj

RUN cp -a bsnes/out/bsnes bsnes.exe

ENTRYPOINT /bin/bash
