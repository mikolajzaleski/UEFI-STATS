#!/bin/bash
set -e
WORKSPACE_DIR="/media/mikzal/Wirtualki/Projects/UEFI/edk2"
APP_DIR="${WORKSPACE_DIR}/Build/MyEfiShellPkg/DEBUG_GCC/X64/MyEfiShellPkg/Application/MyShellApp/MyShellApp/DEBUG"
APP_BIN="${APP_DIR}/MyShellApp.efi"
OVMF_FD="${WORKSPACE_DIR}/Build/OvmfX64/DEBUG_GCC/FV/OVMF.fd"
FAT_DIR="${WORKSPACE_DIR}/qemu_fat_dir_headless"
mkdir -p "${FAT_DIR}/EFI/BOOT"
cp "$APP_BIN" "${FAT_DIR}/EFI/BOOT/BOOTX64.EFI"
qemu-system-x86_64 -m 512 -drive if=pflash,format=raw,file="${OVMF_FD}" -drive file=fat:rw:"${FAT_DIR}",format=raw,media=disk -net none -nographic -serial stdio
