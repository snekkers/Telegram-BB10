FROM debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake file gperf libc6-i386 lib32stdc++6 libgmp10 libmpc3 \
    libmpfr6 libssl-dev perl php-cli pkg-config python3 xz-utils zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
