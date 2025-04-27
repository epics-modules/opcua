#!/bin/bash
#
# This script installs the open62541 library for OPC UA support in EPICS.
# It downloads the specified version of open62541, builds it, and configures the EPICS environment.
# It requires curl, cmake and openssl-dev to be installed on the system.
#
# Usage: ./install_open62541.sh [version]
# If no version is specified, a default version is defined in the script.

# Default version of open62541 to use
DEFAULT_VERSION="1.3.15"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/temp_build"
LIB_DIR="$SCRIPT_DIR/lib-open62541"
OPEN62541_VERSION=${1:-$DEFAULT_VERSION}

# Check if cmake and curl are available
if ! command -v cmake &> /dev/null || ! command -v curl &> /dev/null; then
    echo "Error: cmake and/or curl is not installed. Please install them and try again."
    exit 1
fi

echo "Removing any existing build and library directory..."
rm -rf "$BUILD_DIR"
rm -rf "$LIB_DIR"

mkdir -p "$BUILD_DIR"
mkdir -p "$LIB_DIR"

# Download and extract open62541 if not already present
echo "Downloading open62541 version $OPEN62541_VERSION..."
curl -L "https://github.com/open62541/open62541/archive/refs/tags/v$OPEN62541_VERSION.tar.gz" \
    | tar -xz -C "$BUILD_DIR" --strip-components=1
if [ $? -ne 0 ]; then
    echo "Error: Failed to download and unpack."
    exit 1
fi

echo "Building open62541..."
cmake \
    -S "$BUILD_DIR" \
    -B "$BUILD_DIR"/build \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$SCRIPT_DIR"/lib-open62541 \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
    -DOPEN62541_VERSION="v$OPEN62541_VERSION" \
    -DUA_ENABLE_ENCRYPTION=OPENSSL
if [ $? -ne 0 ]; then
    echo "Error: cmake configuration failed."
    exit 1
fi
make -C "$BUILD_DIR"/build
if [ $? -ne 0 ]; then
    echo "Error: make failed."
    exit 1
fi
make -C "$BUILD_DIR"/build install
if [ $? -ne 0 ]; then
    echo "Error: make install failed."
    exit 1
fi
rm -rf "$BUILD_DIR"

echo "Writing configuration to CONFIG_SITE.local..."
cat <<EOL > "$SCRIPT_DIR/../../configure/CONFIG_SITE.local"
OPEN62541 = \$(TOP)/devOpcuaSup/open62541/lib-open62541
OPEN62541_DEPLOY_MODE = EMBED
OPEN62541_USE_CRYPTO = YES
USR_CXXFLAGS += -std=c++11
EOL

echo "open62541 version $OPEN62541_VERSION installed successfully. You should now be able to build the OPC UA support in EPICS."
