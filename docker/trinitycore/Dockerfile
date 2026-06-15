# syntax=docker/dockerfile:1

##############################################################################
# TrinityCore (master / retail-like) build + runtime image
#
# Stage 1 builds worldserver, authserver and the data-extraction tools from
# source. Stage 2 is a slim runtime image containing only the binaries, the
# installed config templates and the SQL tree (needed by the DB auto-updater).
##############################################################################

############################
# Stage 1: build
############################
FROM ubuntu:24.04 AS build

ARG TC_REPO=https://github.com/TrinityCore/TrinityCore.git
ARG TC_BRANCH=master
# Leave empty to use all available cores. Set e.g. BUILD_JOBS=4 to cap memory.
ARG BUILD_JOBS=

ENV DEBIAN_FRONTEND=noninteractive

# Build toolchain + TrinityCore build dependencies (Ubuntu 24.04 ships
# Boost 1.83 and MySQL client libs, both new enough for master).
RUN apt-get update && apt-get install -y --no-install-recommends \
        git \
        cmake \
        make \
        gcc \
        g++ \
        clang \
        ca-certificates \
        libssl-dev \
        libbz2-dev \
        libreadline-dev \
        libncurses-dev \
        zlib1g-dev \
        libboost-all-dev \
        default-libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

# The compiled-in source directory is baked into the binaries and used by the
# database auto-updater at runtime, so build at a stable, persistent path.
WORKDIR /opt
RUN git clone --depth 1 --branch "${TC_BRANCH}" "${TC_REPO}" trinitycore-src

WORKDIR /opt/trinitycore-src/build
RUN cmake .. \
        -DCMAKE_INSTALL_PREFIX=/opt/trinitycore \
        -DCMAKE_BUILD_TYPE=Release \
        -DTOOLS=1 \
        -DSCRIPTS=static \
        -DWITH_WARNINGS=0 \
    && make -j"${BUILD_JOBS:-$(nproc)}" \
    && make install

############################
# Stage 2: runtime
############################
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Runtime shared libraries (Boost 1.83 on 24.04), plus helpers used by the
# entrypoints: curl (TDB download), p7zip (TDB extraction), mysql client
# (readiness checks), netcat (port waits).
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        p7zip-full \
        netcat-openbsd \
        default-mysql-client \
        tzdata \
        libssl3 \
        zlib1g \
        libbz2-1.0 \
        libreadline8 \
        libncurses6 \
        libboost-system1.83.0 \
        libboost-filesystem1.83.0 \
        libboost-program-options1.83.0 \
        libboost-iostreams1.83.0 \
        libboost-regex1.83.0 \
        libboost-thread1.83.0 \
        libboost-chrono1.83.0 \
        libboost-atomic1.83.0 \
        libboost-date-time1.83.0 \
        libboost-locale1.83.0 \
    && rm -rf /var/lib/apt/lists/*

# Installed binaries + config templates (/opt/trinitycore/{bin,etc}).
COPY --from=build /opt/trinitycore /opt/trinitycore
# SQL tree at the compiled-in source path so the auto-updater can find it.
COPY --from=build /opt/trinitycore-src/sql /opt/trinitycore-src/sql

ENV PATH="/opt/trinitycore/bin:${PATH}"

# Shared entrypoint helpers + per-service entrypoints.
COPY docker/common/entrypoint-lib.sh /usr/local/lib/entrypoint-lib.sh
COPY docker/authserver/entrypoint.sh /usr/local/bin/authserver-entrypoint.sh
COPY docker/worldserver/entrypoint.sh /usr/local/bin/worldserver-entrypoint.sh
RUN chmod +x /usr/local/bin/authserver-entrypoint.sh /usr/local/bin/worldserver-entrypoint.sh

WORKDIR /opt/trinitycore
