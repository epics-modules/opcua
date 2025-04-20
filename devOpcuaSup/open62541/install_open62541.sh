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
OPEN62541_VERSION=${1:-$DEFAULT_VERSION}

# Check if cmake and curl are available
if ! command -v cmake &> /dev/null || ! command -v curl &> /dev/null; then
    echo "Error: cmake and/or curl is not installed. Please install them and try again."
    exit 1
fi

# Download and extract open62541 if not already present
echo "Downloading open62541 version $OPEN62541_VERSION..."
curl -L -o "$SCRIPT_DIR/open62541-$OPEN62541_VERSION.tar.gz" "https://github.com/open62541/open62541/archive/refs/tags/v$OPEN62541_VERSION.tar.gz"
if [ $? -ne 0 ]; then
    echo "Error: Failed to download open62541 version $OPEN62541_VERSION."
    exit 1
fi
temp_dir="`mktemp -d -p "$SCRIPT_DIR"`"
echo "Extracting open62541 archive..."
mkdir -p "$SCRIPT_DIR/lib-open62541"
tar -xzf "$SCRIPT_DIR/open62541-$OPEN62541_VERSION.tar.gz" -C "$temp_dir" --strip-components=1
rm "$SCRIPT_DIR/open62541-$OPEN62541_VERSION.tar.gz"

echo "Building open62541..."
cmake \
    -S "$temp_dir" \
    -B "$temp_dir"/build \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$SCRIPT_DIR"/lib-open62541 \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
    -DOPEN62541_VERSION="v$OPEN62541_VERSION" \
    -DUA_ENABLE_ENCRYPTION=OPENSSL
if [ $? -ne 0 ]; then
    echo "Error: cmake configuration failed."
    rm -rf "$temp_dir"
    exit 1
fi
make -C "$temp_dir"/build
if [ $? -ne 0 ]; then
    echo "Error: make failed."
    rm -rf "$temp_dir"
    exit 1
fi
make -C "$temp_dir"/build install
if [ $? -ne 0 ]; then
    echo "Error: make install failed."
    rm -rf "$temp_dir"
    exit 1
fi
rm -rf "$temp_dir"

echo "Writing configuration to CONFIG_SITE.local..."
cat <<EOL > "$SCRIPT_DIR/../../configure/CONFIG_SITE.local"
OPEN62541 = \$(TOP)/devOpcuaSup/open62541/lib-open62541
OPEN62541_DEPLOY_MODE = EMBED
OPEN62541_USE_CRYPTO = YES
USR_CXXFLAGS += -std=c++11
EOL

echo "open62541 version $OPEN62541_VERSION installed successfully. You should now be able to build the OPC UA support in EPICS."
