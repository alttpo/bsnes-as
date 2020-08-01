FROM ubuntu:18.04

RUN apt-get update && apt-get -qq install -y --no-install-recommends ca-certificates git make cmake clang g++-mingw-w64-x86-64

RUN mkdir -p /tmp/wclang /tmp/bin

RUN git clone https://github.com/tpoechtrager/wclang /tmp/wclang ; \
  cd /tmp/wclang ; \
  cmake -DCMAKE_INSTALL_PREFIX=/tmp . ; \
  make -j8 ; \
  make install

ENTRYPOINT ["/bin/bash"]
