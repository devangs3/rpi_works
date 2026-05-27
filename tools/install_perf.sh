#!/bin/bash
set -e

echo "========================================================="
echo " Pure CDN Tarball perf Compiler for J4012"
echo "========================================================="

# 1. Install prerequisites
echo "[1/4] Installing required development libraries..."
apt-get update
apt-get install -y build-essential flex bison libelf-dev libdw-dev \
                   libaudit-dev libssl-dev libperl-dev systemtap-sdt-dev \
                   libslang2-dev binutils-dev libiberty-dev wget tar xz-utils

# 2. Download and unpack unstripped kernel tree archive 
echo "[2/4] Downloading pure Linux 5.10.192 source snapshot from CDN..."
WORK_DIR="/tmp/jetson_perf_tarball"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# Download official, specific v5.10 point release to prevent server redirects
wget -q --show-progress https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.10.192.tar.xz

echo "Extracting kernel files..."
tar -xf linux-5.10.192.tar.xz
cd linux-5.10.192/tools/perf

# 3. Compile perf with proper folder constraints 
echo "[3/4] Compiling perf with explicit architecture paths..."
make clean
make -j$(nproc) NO_JVMTI=1 WERROR=0

echo "Installing compiled perf binary to system binary path..."
cp perf /usr/bin/perf

# 4. Apply profiling access permissions
echo "[4/4] Configuring runtime kernel event permissions..."
sysctl -w kernel.perf_event_paranoid=-1
echo 0 | tee /proc/sys/kernel/kptr_restrict > /dev/null

if ! grep -q "kernel.perf_event_paranoid=-1" /etc/sysctl.conf; then
    echo "kernel.perf_event_paranoid=-1" >> /etc/sysctl.conf
fi

if ! grep -q "kernel.kptr_restrict=0" /etc/sysctl.conf; then
    echo "kernel.kptr_restrict=0" >> /etc/sysctl.conf
fi
sysctl -p

# Clean up space
rm -rf "$WORK_DIR"

echo "========================================================="
echo " Success! perf has been built and installed."
echo "========================================================="
/usr/bin/perf --version
echo "========================================================="
