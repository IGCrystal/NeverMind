FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential gcc clang lld binutils make \
    grub-pc-bin grub-common xorriso \
    qemu-system-x86 ovmf \
    cppcheck clang-tools \
    git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["bash", "-lc", "make clean all && make test && make integration && make smoke"]
