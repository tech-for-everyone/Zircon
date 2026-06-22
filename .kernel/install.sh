#!/bin/bash
####################
# ZIRCON INSTALLER #
####################

set -e

if [ ! -f /etc/os-release ]; then
    echo "Zircon: cannot detect OS (no /etc/os-release)"
    exit 1
fi

. /etc/os-release
echo "Zircon: detected $NAME"

case "$NAME" in
    Arch*)
        sudo pacman -Syu --noconfirm curl github-cli
        ;;
    Ubuntu*|Debian*)
        sudo apt update && sudo apt upgrade -y
        sudo apt install -y curl gh
        ;;
    Fedora*)
        sudo dnf upgrade --refresh -y
        sudo dnf install -y curl gh
        ;;
    *)
        echo "Zircon: unsupported distro '$NAME' — install curl and gh manually"
        exit 1
        ;;
esac

echo "Zircon: downloading CodeOS kernel..."
gh release download Releases \
    --repo tech-for-everyone/CodeOS-Kernel \
    --pattern "codeos-*-kernel.bin" \
    --dir /tmp/zircon-kernel \
    --clobber \
    2>/dev/null || {
    echo "Zircon: GitHub release download failed — falling back to latest release URL"
    LATEST=$(curl -sL https://api.github.com/repos/tech-for-everyone/CodeOS-Kernel/releases/latest \
        | grep "browser_download_url" | grep "kernel.bin" | head -1 | cut -d'"' -f4)
    if [ -n "$LATEST" ]; then
        mkdir -p /tmp/zircon-kernel
        curl -sL "$LATEST" -o /tmp/zircon-kernel/codeos-kernel.bin
    else
        echo "Zircon: could not download CodeOS kernel"
        exit 1
    fi
}

echo "Zircon: kernel downloaded to /tmp/zircon-kernel/"
ls -lh /tmp/zircon-kernel/
echo "Zircon: install complete — reboot into CodeOS to start Zircon"
