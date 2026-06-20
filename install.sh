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
sudo pacman -Syu
if $NAME=Ubuntu
sudo apt upgrade
if $NAME=Fedora
sudo dnf upgrade --refresh

git clone https://github.com/tech-for-everyone/CodeOS-Kernel
