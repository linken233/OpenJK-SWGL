# Build stage
FROM debian:12 AS builder

RUN dpkg --add-architecture i386 && \
    apt-get -q update && \
    DEBIAN_FRONTEND=noninteractive apt-get -q install -y --no-install-recommends \
        build-essential cmake ninja-build git gcc-multilib g++-multilib \
        libjpeg-dev libjpeg-dev:i386 libpng-dev libpng-dev:i386 zlib1g-dev zlib1g-dev:i386 && \
    rm -rf /var/lib/apt/lists/*

COPY . /usr/src/swgl

RUN cmake -S /usr/src/swgl -B /usr/src/swgl/build.i386 -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=/usr/src/swgl/cmake/Toolchains/linux-i686.cmake \
        -DCMAKE_INSTALL_PREFIX=/opt/swgl-i386 \
        -DCMAKE_BUILD_TYPE=Release \
        -DUseInternalLibs=OFF -DBuildPortableVersion=OFF \
        -DBuildSPEngine=OFF -DBuildSPGame=OFF -DBuildSPRdVanilla=OFF -DBuildSPRend2=OFF \
        -DBuildMPEngine=OFF -DBuildMPCGame=OFF -DBuildMPUI=OFF -DBuildMPRdVanilla=OFF -DBuildMPRend2=OFF \
        -DBuildMPDed=ON -DBuildMPGame=ON && \
    cmake --build /usr/src/swgl/build.i386 -j "$(nproc)" && \
    cmake --install /usr/src/swgl/build.i386

RUN cmake -S /usr/src/swgl -B /usr/src/swgl/build.x86_64 -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/opt/swgl-x86_64 \
        -DCMAKE_BUILD_TYPE=Release \
        -DUseInternalLibs=OFF -DBuildPortableVersion=OFF \
        -DBuildSPEngine=OFF -DBuildSPGame=OFF -DBuildSPRdVanilla=OFF -DBuildSPRend2=OFF \
        -DBuildMPEngine=OFF -DBuildMPCGame=OFF -DBuildMPUI=OFF -DBuildMPRdVanilla=OFF -DBuildMPRend2=OFF \
        -DBuildMPDed=ON -DBuildMPGame=ON && \
    cmake --build /usr/src/swgl/build.x86_64 -j "$(nproc)" && \
    cmake --install /usr/src/swgl/build.x86_64

# Server stage
FROM debian:12

RUN dpkg --add-architecture i386 && \
    apt-get -q update && \
    DEBIAN_FRONTEND=noninteractive apt-get -q install -y --no-install-recommends \
        socat libstdc++6 libstdc++6:i386 zlib1g zlib1g:i386 && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/openjk/cdpath/base /opt/openjk/cdpath/SWGL /opt/openjk/basepath /opt/openjk/homepath/SWGL

COPY --from=builder /opt/swgl-i386/SWGL/SWGLDed.i386 /opt/openjk/SWGLDed.i386
COPY --from=builder /opt/swgl-x86_64/SWGL/SWGLDed.x86_64 /opt/openjk/SWGLDed.x86_64
COPY --from=builder /opt/swgl-i386/SWGL/SWGL/swglmpgamei386.so /opt/openjk/cdpath/SWGL/swglmpgamei386.so
COPY --from=builder /opt/swgl-x86_64/SWGL/SWGL/swglmpgamex86_64.so /opt/openjk/cdpath/SWGL/swglmpgamex86_64.so

COPY scripts/docker/*.sh /opt/openjk/
COPY scripts/docker/server.cfg /opt/openjk/cdpath/base/server.cfg
COPY scripts/docker/server.cfg /opt/openjk/cdpath/SWGL/server.cfg
COPY scripts/docker/server.cfg /opt/openjk/homepath/SWGL/server.cfg
RUN chmod +x /opt/openjk/SWGLDed.* /opt/openjk/*.sh

ENV OJK_OPTS="+exec server.cfg"
EXPOSE 29070/udp
HEALTHCHECK --interval=10s --timeout=9s --retries=6 CMD ["/opt/openjk/healthcheck.sh"]
CMD ["/opt/openjk/run.sh"]
