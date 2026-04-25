#!/usr/bin/env bash
set -euo pipefail

# Mordor Ubuntu dependency bootstrap
# Installs core build + tooling packages used by this repo.

if [[ "${EUID}" -eq 0 ]]; then
  echo "Do not run as root. Run as a normal user; the script uses sudo as needed."
  exit 1
fi

if [[ ! -f /etc/os-release ]]; then
  echo "Cannot detect OS: /etc/os-release is missing."
  exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release

if [[ "${ID:-}" != "ubuntu" ]]; then
  echo "This script currently supports Ubuntu only. Detected: ${ID:-unknown}."
  exit 1
fi

echo "Detected Ubuntu ${VERSION_ID:-unknown}. Installing dependencies..."

sudo apt-get update

# Keep this list minimal and practical for current phase.
PACKAGES=(
  build-essential
  cmake
  ninja-build
  pkg-config
  git
  curl
  ca-certificates

  clang
  clang-format
  clang-tidy
  cppcheck
  gdb
  lldb

  ccache
  shellcheck

  libgl1-mesa-dev
  libglu1-mesa-dev
  mesa-common-dev
  libx11-dev
  libxrandr-dev
  libxinerama-dev
  libxcursor-dev
  libxi-dev
)

sudo apt-get install -y "${PACKAGES[@]}"

echo "Dependency installation complete."
echo "You can now configure the project with: cmake -S . -B build"
