#!/bin/bash

set -eux

# Add source URIs
sed -Ei 's/^# deb-src /deb-src /' /etc/apt/sources.list
apt-get update

# Download dependencies
apt build-dep -y genext2fs

# Configure with libarchive
./autogen.sh

./configure --enable-libarchive

# Build
make -j$(nproc)
