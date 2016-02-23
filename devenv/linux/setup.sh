#!/bin/sh

echo
echo "-- begin : xxhash provisioning --"

# Minimize interaction with apt-get
# http://serverfault.com/a/670688
# https://www.debian.org/releases/stable/s390x/ch05s02.html.en
export DEBIAN_FRONTEND=noninteractive

# Update package list and install packages
# note: these commands are same as .travis.yml
sudo apt-get update -qq
sudo apt-get install -qq gcc-arm-linux-gnueabi
sudo apt-get install -qq clang
sudo apt-get install -qq g++-multilib
sudo apt-get install -qq gcc-multilib
sudo apt-get install -qq valgrind
sudo apt-get install -qq ruby-ronn

# Install packages to minimize difference between VM and Travis-CI
sudo apt-get install -qq git
sudo apt-get install -qq make

# note: Entire /vagrant/ directory is mapped to HOST's xxhash/devenv/linux/
#       See https://www.vagrantup.com/docs/synced-folders/ for details.

# Clone xxhash to /vagrant/xxhash/
# This directory is mapped to HOST's xxhash/devenv/linux/xxhash/
rm -rf /vagrant/xxhash
git clone https://github.com/Cyan4973/xxHash.git /vagrant/xxhash/

# Create symbolic link from ~/xxhash/ to /vagrant/xxhash/
rm -f xxhash
ln -s /vagrant/xxhash/ xxhash

echo "-- end : xxhash provisioning --"
echo
