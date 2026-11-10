#!/usr/bin/env bash
set -euo pipefail

SDK_ROOT="${ANDROID_HOME:-/opt/android-sdk}"
CMDLINE_TOOLS_VERSION="11076708"
CMDLINE_TOOLS_URL="https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS_VERSION}_latest.zip"

mkdir -p "$SDK_ROOT/cmdline-tools"

if [[ ! -x "$SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" ]]; then
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' EXIT
  curl -fsSL "$CMDLINE_TOOLS_URL" -o "$tmp/cmdline-tools.zip"
  unzip -q "$tmp/cmdline-tools.zip" -d "$tmp"
  rm -rf "$SDK_ROOT/cmdline-tools/latest"
  mkdir -p "$SDK_ROOT/cmdline-tools/latest"
  cp -a "$tmp/cmdline-tools/." "$SDK_ROOT/cmdline-tools/latest/"
fi

export ANDROID_HOME="$SDK_ROOT"
export ANDROID_SDK_ROOT="$SDK_ROOT"
export PATH="$SDK_ROOT/cmdline-tools/latest/bin:$SDK_ROOT/platform-tools:$PATH"

# sdkmanager may return a successful exit while the yes process receives SIGPIPE.
yes | sdkmanager --licenses >/dev/null 2>&1 || true

sdkmanager --install \
  "platform-tools" \
  "platforms;android-35" \
  "build-tools;35.0.0" \
  "ndk;28.2.13676358" \
  "cmake;3.31.6"

echo "Android SDK/NDK installation finished."
