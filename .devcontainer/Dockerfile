FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_SDK_ROOT=/opt/android-sdk
ENV ANDROID_NDK_HOME=/opt/android-sdk/ndk/28.2.13676358
ENV PATH=/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools:/usr/local/bin:${PATH}

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build clang lld llvm clang-format clang-tidy \
    git git-lfs curl wget unzip zip xz-utils pkg-config python3 python3-pip \
    openjdk-17-jdk-headless \
    libvulkan-dev vulkan-tools glslang-tools spirv-tools glslc \
    libassimp-dev libglm-dev libstb-dev \
    ca-certificates file jq && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/android-sdk && chown -R vscode:vscode /opt/android-sdk

USER vscode
