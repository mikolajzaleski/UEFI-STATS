#!/bin/bash
set -e

# Define directories
WORKSPACE_DIR="/media/mikzal/Wirtualki/Projects/UEFI/edk2"
APP_DIR="${WORKSPACE_DIR}/Build/MyEfiShellPkg/DEBUG_GCC/X64/MyEfiShellPkg/Application/MyShellApp/MyShellApp/DEBUG"
APP_BIN="${APP_DIR}/MyShellApp.efi"
OVMF_FD="${WORKSPACE_DIR}/Build/OvmfX64/DEBUG_GCC/FV/OVMF.fd"
FAT_DIR="${WORKSPACE_DIR}/qemu_fat_dir"

# Verify files exist
if [ ! -f "$APP_BIN" ]; then
    echo "Error: Application binary not found at $APP_BIN"
    exit 1
fi

if [ ! -f "$OVMF_FD" ]; then
    echo "Error: OVMF firmware not found at $OVMF_FD"
    exit 1
fi

# Setup virtual FAT drive for QEMU
echo "Setting up virtual FAT drive..."
rm -rf "$FAT_DIR"
mkdir -p "${FAT_DIR}/EFI/BOOT"

# Copy as BOOTX64.EFI to autoboot directly
cp "$APP_BIN" "${FAT_DIR}/EFI/BOOT/BOOTX64.EFI"

# Copy Unit Test app so it can be run manually
TEST_BIN="${WORKSPACE_DIR}/Build/MyEfiShellPkg/DEBUG_GCC/X64/MyShellAppTest.efi"
if [ -f "$TEST_BIN" ]; then
    cp "$TEST_BIN" "${FAT_DIR}/MyShellAppTest.efi"
fi

# Create startup.nsh to autoboot if EFI shell loads first
echo 'echo "Starting UEFI Hardware Diagnostic Tool..."' > "${FAT_DIR}/startup.nsh"
echo 'fs0:\EFI\BOOT\BOOTX64.EFI' >> "${FAT_DIR}/startup.nsh"

echo "Starting QEMU..."
echo "After QEMU exits, any files exported (like DiagReport.txt) should appear in ${FAT_DIR}"

# Run QEMU
qemu-system-x86_64 \
    -m 512 \
    -drive if=pflash,format=raw,file="${OVMF_FD}" \
    -drive file=fat:rw:"${FAT_DIR}",format=raw,media=disk \
    -net none

echo ""
echo "Checking for DiagReport.txt export..."
if [ -f "${FAT_DIR}/DiagReport.txt" ]; then
    echo "SUCCESS: DiagReport.txt found!"
    echo "--- Contents of DiagReport.txt ---"
    cat "${FAT_DIR}/DiagReport.txt"
    echo "----------------------------------"
else
    echo "WARNING: DiagReport.txt was not exported."
fi
