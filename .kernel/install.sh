####################
# ZIRCON INSTALLER #
####################
#!/bin/bash

if [ -f /etc/os-release ]; then
    # Source the file to load its variables
    . /etc/os-release
    echo "Linux Distro Name: $NAME"
if $NAME=Arch 
then
sudo pacman -Syu curl github-cli
if $NAME=Ubuntu
sudo apt upgrade
sudo apt install curl gh
if $NAME=Fedora
sudo dnf upgrade --refresh
sudo dnf install curl gh
########
#CLONER#
########

gh release download Releases --repo tech-for-everyone/CodeOS-Kernel --pattern "codeos-1.2-kernel.bin"
