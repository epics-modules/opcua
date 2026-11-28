#
# This script installs the open62541 library for OPC UA support in EPICS.
# It downloads the specified version of open62541, builds it, and configures the EPICS environment.
# It requires cmake to be installed on the system. OpenSSL development files must be
# available for the encryption support (e.g. a prebuilt OpenSSL package or vcpkg).
#
# This is the Windows equivalent of install_open62541.sh.
#
# Usage:
#   .\install_open62541.ps1                  # use default version (1.3.23)
#   .\install_open62541.ps1 1.4.8            # build a specific version
#   .\install_open62541.ps1 -Architecture Win32            # build 32-bit for T_A=win32-x86
#   .\install_open62541.ps1 -OpenSSLRoot C:\OpenSSL-Win64  # point cmake to OpenSSL
#
# Note: the script may have to be run with an execution policy that allows local scripts:
#   powershell -ExecutionPolicy Bypass -File .\install_open62541.ps1
#

param(
    # Version of open62541 to download and build (defaults to $DEFAULT_VERSION)
    [Parameter(Position = 0)]
    [string]$Version,

    # MSVC target architecture passed to cmake as -A <arch>:
    #   x64   -> builds for EPICS T_A=windows-x64 (default)
    #   Win32 -> builds for EPICS T_A=win32-x86
    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string]$Architecture = 'x64',

    # Path to a local OpenSSL installation, forwarded to cmake as OPENSSL_ROOT_DIR.
    # Required for the encryption support; e.g. C:\Program Files\OpenSSL-Win64
    # or a vcpkg install directory.
    [string]$OpenSSLRoot = ''
)

$ErrorActionPreference = 'Stop'

# Default version of open62541 to use
$DEFAULT_VERSION = '1.3.23'

# Initialize TLS for reliable downloads (GitHub requires TLS 1.2)
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$BUILD_DIR = Join-Path $SCRIPT_DIR 'temp_build'
$LIB_DIR = Join-Path $SCRIPT_DIR 'lib-open62541'
$OPEN62541_VERSION = if ($Version) { $Version } else { $DEFAULT_VERSION }

# Check if cmake is available
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host 'Error: cmake is not installed. Please install it and try again.'
    Write-Host 'Download from: https://cmake.org/download/'
    exit 1
}

# Check if OpenSSL is available if encryption is enabled (default)
if ('OPENSSL' -eq 'OPENSSL' -and $OpenSSLRoot -eq '') {
    Write-Host 'Checking for OpenSSL development files...'
    # Check common OpenSSL installation paths
    $opensslCheckPaths = @(
        'C:\OpenSSL-Win64',
        'C:\OpenSSL-Win32',
        'C:\Program Files\OpenSSL-Win64',
        'C:\Program Files\OpenSSL-Win32',
        'C:\vcpkg\installed\x64-windows',
        'C:\vcpkg\installed\x86-windows'
    )
    $opensslFound = $false
    foreach ($path in $opensslCheckPaths) {
        if (Test-Path $path) {
            # Check if development files actually exist in this OpenSSL installation
            $includePath = Join-Path $path 'include'
            $includeOpensslPath = Join-Path $includePath 'openssl'
            $libPath = Join-Path $path 'lib'
            
            # Check for common OpenSSL development files
            # For different OpenSSL installations, headers may be in:
            # 1. include/openssl/*.h (standard)
            # 2. include/*.h (alternative)
            $headerFiles = @()
            if (Test-Path $includeOpensslPath) {
                $headerFiles += Get-ChildItem -Path $includeOpensslPath -Filter '*.h' -ErrorAction SilentlyContinue
            }
            if (Test-Path $includePath) {
                $headerFiles += Get-ChildItem -Path $includePath -Filter '*.h' -ErrorAction SilentlyContinue
            }
            $hasHeaders = ($headerFiles.Count -gt 0)
            
            # Check for library files - common names vary by installation
            # OpenSSL libs may be in lib/ or in nested directories like lib\VC\, lib\x64\, lib\arm64\
            $hasLibs = $false
            if (Test-Path $libPath) {
                # Recursively search through all subdirectories
                $libFiles = Get-ChildItem -Path $libPath -Filter '*.lib' -Recurse -ErrorAction SilentlyContinue
                foreach ($libFile in $libFiles) {
                    if ($libFile.Name -match 'ssl|crypto') {
                        $hasLibs = $true
                        Write-Host "  Found library: $($libFile.FullName)" -ForegroundColor DarkGray
                        break
                    }
                }
                
                # If no .lib files found, try .a files (MinGW)
                if (-not $hasLibs) {
                    $aFiles = Get-ChildItem -Path $libPath -Filter '*.a' -Recurse -ErrorAction SilentlyContinue
                    foreach ($aFile in $aFiles) {
                        if ($aFile.Name -match 'ssl|crypto') {
                            $hasLibs = $true
                            break
                        }
                    }
                }
                
                # Common OpenSSL library names check
                if (-not $hasLibs) {
                    $commonLibNames = @('libssl', 'libeay32', 'ssleay32', 'crypto')
                    foreach ($libName in $commonLibNames) {
                        if (Get-ChildItem -Path $libPath -Filter "$libName.*" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '\.(lib|a)$' }) {
                            $hasLibs = $true
                            break
                        }
                    }
                }
            }
            
            if ($hasHeaders -and $hasLibs) {
                $OpenSSLRoot = $path
                $opensslFound = $true
                Write-Host "Found OpenSSL development files at: $path"
                break
            } else {
                Write-Host "Warning: OpenSSL found at $path but development files are missing!"
                if (-not $hasHeaders) { Write-Host "  - Missing: include/openssl/*.h" }
                if (-not $hasLibs) { Write-Host "  - Missing: *.lib files in lib/" }
            }
        }
    }
    if (-not $opensslFound) {
        Write-Host 'Warning: OpenSSL development files not found automatically.'
        Write-Host 'Please specify the path with -OpenSSLRoot or install OpenSSL properly.'
        Write-Host 'Make sure to install the version with development files (headers and .lib files).'
        Write-Host 'Download from: https://slproweb.com/products/Win32OpenSSL.html'
        Write-Host 'Or via vcpkg: vcpkg install openssl'
        exit 1
    }
}

Write-Host "Removing any existing build and library directory..."
if (Test-Path $BUILD_DIR) { Remove-Item -Recurse -Force $BUILD_DIR }
if (Test-Path $LIB_DIR) { Remove-Item -Recurse -Force $LIB_DIR }

New-Item -ItemType Directory -Path $BUILD_DIR -Force | Out-Null
New-Item -ItemType Directory -Path $LIB_DIR -Force | Out-Null

# Download and extract open62541 source
Write-Host "Downloading open62541 version $OPEN62541_VERSION..."

try {
    # Try tarball first (preferred)
    $TARBALL = "https://github.com/open62541/open62541/archive/refs/tags/v$OPEN62541_VERSION.tar.gz"
    $ARCHIVE = Join-Path $BUILD_DIR 'open62541.tar.gz'
    Invoke-WebRequest -Uri $TARBALL -OutFile $ARCHIVE -UseBasicParsing
    
    if (Get-Command tar -ErrorAction SilentlyContinue) {
        Write-Host "Extracting using tar.exe..."
        tar -xzf $ARCHIVE -C $BUILD_DIR --strip-components=1
        if ($LASTEXITCODE -ne 0) {
            Write-Host 'Error: tar extraction failed!' -ForegroundColor Red
            exit 1
        }
    } else {
        throw "tar.exe not available"
    }
} catch {
    Write-Host "Falling back to ZIP download... $($_.Exception.Message)"
    $ZIP = "https://github.com/open62541/open62541/archive/refs/tags/v$OPEN62541_VERSION.zip"
    $ZIPFILE = Join-Path $BUILD_DIR 'open62541.zip'
    Invoke-WebRequest -Uri $ZIP -OutFile $ZIPFILE -UseBasicParsing
    
    $EXTRACT_DIR = Join-Path $BUILD_DIR 'extract'
    Expand-Archive -Path $ZIPFILE -DestinationPath $EXTRACT_DIR -Force
    Get-ChildItem -Path $EXTRACT_DIR | Move-Item -Destination $BUILD_DIR -Force
    Remove-Item -Recurse -Force $EXTRACT_DIR
    Remove-Item -Force $ZIPFILE
}

if (Test-Path $ARCHIVE) { Remove-Item -Force $ARCHIVE }

$BUILD_CONFIG_DIR = Join-Path $BUILD_DIR 'build'

$CMAKE_ARGS = @(
    '-S', $BUILD_DIR,
    '-B', $BUILD_CONFIG_DIR,
    '-DBUILD_SHARED_LIBS=OFF',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
    "-DCMAKE_INSTALL_PREFIX=$LIB_DIR",
    '-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF',
    "-DOPEN62541_VERSION=v$OPEN62541_VERSION",
    '-DUA_ENABLE_ENCRYPTION=OPENSSL',
    "-DCMAKE_EXE_LINKER_FLAGS=`"-LIBPATH:$OPENSSL_LIB_DIR`""
)

# Add architecture for Visual Studio generator
try {
    $cmakeHelp = & cmake --help 2>&1
    if ($cmakeHelp -match 'Visual Studio') {
        $CMAKE_ARGS += @('-A', $Architecture)
    }
} catch {
    Write-Host "Warning: Could not check cmake generator: $_"
}

if ($OpenSSLRoot -ne '') {
    $CMAKE_ARGS += @("-DOPENSSL_ROOT_DIR=$OpenSSLRoot")
}

Write-Host 'Configuring open62541...'
& cmake @CMAKE_ARGS
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Error: cmake configuration failed.' -ForegroundColor Red
    exit 1
}

Write-Host 'Building open62541...'
& cmake --build $BUILD_CONFIG_DIR --config RelWithDebInfo --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Error: build failed.' -ForegroundColor Red
   # exit 1
}

Write-Host 'Installing open62541...'
& cmake --install $BUILD_CONFIG_DIR --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Error: install failed.' -ForegroundColor Red
    exit 1
}

Remove-Item -Recurse -Force $BUILD_DIR

# Create config file
Write-Host 'Writing configuration to CONFIG_SITE.local...'
$configContent = @"
OPEN62541 = `$`(TOP)/devOpcuaSup/open62541/lib-open62541
OPEN62541_DEPLOY_MODE = EMBED
OPEN62541_USE_CRYPTO = YES
OPEN62541_USE_XMLPARSER = YES
"@

$configPath = Join-Path (Resolve-Path (Join-Path (Split-Path $SCRIPT_DIR -Parent) '..')).Path 'configure'
$configPath = Join-Path $configPath 'CONFIG_SITE.local'

$configContent | Out-File -FilePath $configPath -Encoding ASCII -Force
Write-Host "Successfully wrote configuration to $configPath"

Write-Host "open62541 version $OPEN62541_VERSION installed successfully." -ForegroundColor Green
Write-Host "You should now be able to build the OPC UA support in EPICS." -ForegroundColor Green
