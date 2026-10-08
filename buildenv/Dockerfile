# SPDX-License-Identifier: GPL-3.0-only
FROM debian:trixie-slim

# Use Debian's packaged emulator so cold CI builds do not compile DOSBox-X.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential ca-certificates dosbox dosbox-x git mtools \
    python3 python3-pil ripgrep \
    && rm -rf /var/lib/apt/lists/*

COPY TC/ /opt/dos-toolchain/TC/
COPY TASM/ /opt/dos-toolchain/TASM/
ENV DOS_TOOLCHAIN=/opt/dos-toolchain \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HOME=/tmp
WORKDIR /workspace
CMD ["make", "build"]
