# SPDX-License-Identifier: GPL-3.0-only
FROM ubuntu:24.04

ARG UBUNTU_MIRROR=http://archive.ubuntu.com/ubuntu
RUN sed -i "s|http://archive.ubuntu.com/ubuntu|${UBUNTU_MIRROR}|g" /etc/apt/sources.list.d/ubuntu.sources \
    && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    autoconf automake build-essential ca-certificates dosbox mtools pkg-config \
    python3 python3-pil ripgrep libsdl2-dev libsdl2-net-dev libpng-dev \
    libfreetype6-dev libasound2-dev libncurses-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

COPY vendor/ /opt/nightwatch-vendor/
COPY build-dosbox-x.sh /opt/nightwatch-vendor/build-dosbox-x.sh
RUN bash /opt/nightwatch-vendor/build-dosbox-x.sh /usr/local \
    && rm -rf /opt/nightwatch-vendor

COPY TC/ /opt/dos-toolchain/TC/
COPY TASM/ /opt/dos-toolchain/TASM/
ENV DOS_TOOLCHAIN=/opt/dos-toolchain \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HOME=/tmp
WORKDIR /workspace
CMD ["make", "build"]
