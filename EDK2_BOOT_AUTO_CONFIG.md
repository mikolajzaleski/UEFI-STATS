# EDK2/OVMF Auto-Boot Configuration Guide

## Overview
This document describes how to automatically boot into a specific EFI Shell application without showing the boot manager menu in EDK2/OVMF.

---

## 1. Key PCD Settings for Boot Control

### Primary Boot Timeout PCD
**File:** [MdePkg/MdePkg.dec](MdePkg/MdePkg.dec#L2630)  
**PCD Name:** `gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut`  
**Type:** UINT16 (seconds)  
**Default:** 0xFFFF (wait for user input indefinitely)  
**Meanings:**
- `0` = Boot immediately (no timeout/menu display)
- `1-0xFFFE` = Display boot menu for N seconds before auto-boot to first option
- `0xFFFF` = Wait indefinitely for user input

**Usage in DSC files:**
```
gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0
```

**Example from OVMF:**  
[OvmfPkg/OvmfPkgX64.dsc](OvmfPkg/OvmfPkgX64.dsc#L721):
```
gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0
```

### Additional Boot Control PCDs
**File:** [MdeModulePkg/MdeModulePkg.dec](MdeModulePkg/MdeModulePkg.dec#L1223)

| PCD Name | Type | Purpose |
|----------|------|---------|
| `PcdSupportInfiniteBootRetries` | BOOLEAN | If TRUE, retry all boot options infinitely instead of stopping after first success |
| `PcdBootManagerMenuFile` | VOID* | GUID of the Boot Manager Menu application (can be customized) |

---

## 2. Boot Manager Flow Architecture

### Main Entry Point
**File:** [MdeModulePkg/Universal/BdsDxe/BdsEntry.c](MdeModulePkg/Universal/BdsDxe/BdsEntry.c)

**Function:** `BdsEntry()` (Line 679)  
This is the entry point called after DXE phase completes. Key flow:

1. **Initialize Timeout** (Lines 756-768)
   - Sets "Timeout" variable from `PcdPlatformBootTimeOut`
   - If timeout = 0, no wait occurs
   - If timeout = 0xFFFF, waits indefinitely

2. **Boot Next Processing** (Lines 1067-1104)
   - If `BootNext` variable exists, boots that option first
   - Deletes `BootNext` after consuming it (prevents boot loops)
   - Format: `Boot####` where `####` is the option number in hex

3. **Boot Order Processing** (Line 1118)
   - Calls `BootBootOptions()` which iterates `BootOrder` variable
   - Attempts each boot option that has `LOAD_OPTION_ACTIVE` attribute

4. **Boot Manager Menu Display**
   - Only shown if:
     - A boot option returns `EFI_SUCCESS`
     - AND boot manager menu exists
     - AND NOT using `PcdSupportInfiniteBootRetries`

---

## 3. Critical Boot Variables

### BootOrder (UEFI Standard Variable)
**GUID:** `gEfiGlobalVariableGuid` (8BE4DF61-93CA-11D2-AA0D-00E098032B8C)  
**Variable Name:** "BootOrder"  
**Type:** UINT16 array  
**Content:** Ordered list of Boot#### option numbers  
**Location in Code:** [MdeModulePkg/Library/UefiBootManagerLib/BmBoot.c](MdeModulePkg/Library/UefiBootManagerLib/BmBoot.c)

**Example:**
- BootOrder = [0x0003, 0x0001, 0x0002]
- Attempts Boot0003, then Boot0001, then Boot0002

### BootNext (UEFI Standard Variable)
**GUID:** `gEfiGlobalVariableGuid`  
**Variable Name:** "BootNext"  
**Type:** UINT16 (single value)  
**Purpose:** Force boot to specific option on next boot only  
**Auto-deleted:** Yes, after being consumed  

**Example:**
- Set BootNext = 0x0005 → Next boot uses Boot0005
- Boot0005 is deleted from BootNext after boot attempt
- Subsequent boots use BootOrder

**Code Reference:** [BdsEntry.c Lines 1067-1104](MdeModulePkg/Universal/BdsDxe/BdsEntry.c#L1067)

### Boot#### Variables
**Format:** Boot followed by 4 hex digits (e.g., Boot0001, Boot0005)  
**Type:** EFI_LOAD_OPTION structure  
**Critical Attributes:**
- Bit 0: `LOAD_OPTION_ACTIVE` (must be set for auto-boot)
- Bit 3: `LOAD_OPTION_CATEGORY_BOOT` (marks as boot option)

**Code Reference:** [BdsEntry.c Lines 375-439](MdeModulePkg/Universal/BdsDxe/BdsEntry.c#L375) in `BootBootOptions()`

---

## 4. Boot Option Execution

### EfiBootManagerBoot Function
**File:** [MdeModulePkg/Library/UefiBootManagerLib/BmBoot.c](MdeModulePkg/Library/UefiBootManagerLib/BmBoot.c#L1894)

**Function Signature:**
```c
VOID
EFIAPI
EfiBootManagerBoot (
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
```

**Key Operations (Lines 1894-2100):**
1. Validates boot option (must have FilePath)
2. Creates temporary Boot#### if needed
3. Sets `BootCurrent` variable
4. Signals `EVT_SIGNAL_READY_TO_BOOT` event
5. Loads and executes the boot image
6. Returns to BDS if boot fails

---

## 5. Platform-Specific Implementation

### OVMF Platform Boot Manager
**File:** [OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c](OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c)

**Key Functions:**
- `PlatformBootManagerBeforeConsole()` (called from BdsEntry, Line 889)
- `PlatformBootManagerAfterConsole()` (called from BdsEntry, Line 903)
- `PlatformRegisterOptionsAndKeys()` (Lines 128-178)

**Hotkey Mappings (Lines 141-170):**
```c
F2   → Boot Manager Menu
ESC  → Boot Manager Menu
ENTER → Continue boot
```

**Platform-Specific Features:**
- Handles QEMU boot order from command-line
- Supports Xen, CloudHV, Bhyve hypervisors
- Registers EFI Shell as default boot application

---

## 6. How to Auto-Boot Your Shell/App Without Menu

### Option 1: Use BootNext Variable (Immediate, One-Time)
```c
// Set BootNext to force next boot to your application
UINT16 BootNextValue = 0x0005; // Boot0005 contains your app
Status = gRT->SetVariable(
    L"BootNext",
    &gEfiGlobalVariableGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS | 
    EFI_VARIABLE_RUNTIME_ACCESS | 
    EFI_VARIABLE_NON_VOLATILE,
    sizeof(BootNextValue),
    &BootNextValue
);
```

**Result:** Your app boots on next reset, bypassing boot menu

### Option 2: Modify BootOrder (Persistent Default)
```c
// Reorder BootOrder to put your app first
// This assumes your app is Boot0005
UINT16 BootOrder[] = {0x0005, 0x0001, 0x0002}; // Your app first
Status = gRT->SetVariable(
    L"BootOrder",
    &gEfiGlobalVariableGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS | 
    EFI_VARIABLE_RUNTIME_ACCESS | 
    EFI_VARIABLE_NON_VOLATILE,
    sizeof(BootOrder),
    &BootOrder
);
```

**Result:** Your app boots first on every reset (if timeout = 0)

### Option 3: Set PcdPlatformBootTimeOut to 0 (No Menu Delay)

**In DSC file:**
```
gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0
```

**Effect:**
- If BootOrder[0] = your app → boots immediately
- If BootNext is set → boots that immediately
- Menu never displayed if boot succeeds

### Option 4: Complete Flow to Auto-Boot MyShellApp with GUID

**Your Boot Option:**
- Option Number: 0x0005 (Boot0005)
- GUID: 11223344-5566-7788-99AA-BBCCDDEEFF00
- Attributes: LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_BOOT
- Description: "MyShellApp"

**Complete Setup:**
1. Ensure Boot0005 exists with LOAD_OPTION_ACTIVE set
2. Set BootOrder = {0x0005, ...other options...}
3. Set PcdPlatformBootTimeOut|0 in DSC

**Result:** 
```
BDS Start
  → Timeout = 0 (no menu wait)
  → Load BootOrder
  → Find Boot0005 (MyShellApp) with ACTIVE flag
  → EfiBootManagerBoot(&Boot0005)
  → MyShellApp launches immediately
```

---

## 7. Bypass Boot Manager Menu Completely

### Method A: Successful Boot = No Menu
**Logic in BdsEntry.c (Lines 1105-1110):**
```c
if ((BootManagerMenu != NULL) && (BootOptions[Index].Status == EFI_SUCCESS)) {
    EfiBootManagerBoot (BootManagerMenu);  // Show menu only if boot failed
    break;
}
```

**Implication:** If your app boots successfully, menu never appears.

### Method B: Use EFI_OS_INDICATIONS Variable
**Standard UEFI variable (BdsEntry.c Lines 977-1035):**
- `EFI_OS_INDICATIONS_BOOT_TO_FW_UI` bit → Force to boot manager menu
- Clear this bit → Normal boot sequence

**Example to disable boot to menu:**
```c
UINT64 OsIndication = 0;  // Clear menu boot flag
Status = gRT->SetVariable(
    L"OsIndications",
    &gEfiGlobalVariableGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS | 
    EFI_VARIABLE_RUNTIME_ACCESS | 
    EFI_VARIABLE_NON_VOLATILE,
    sizeof(OsIndication),
    &OsIndication
);
```

### Method C: Implement Custom PlatformBootManagerLib
**Override in platform DSC:**
```
PlatformBootManagerLib|YourPlatform/Library/PlatformBootManagerLib/PlatformBootManagerLib.inf
```

**Customize `PlatformBootManagerAfterConsole()`:**
- Skip boot manager menu registration entirely
- Directly register your app boot option first
- Example: [OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c](OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c#L1800)

---

## 8. Platform Hotkey Override

### Remove Boot Manager Menu Hotkey
**File:** [OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c](OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c#L128)

**Current Code (Lines 141-170):**
```c
// Map F2 to Boot Manager Menu
F2.ScanCode = SCAN_F2;
Status = EfiBootManagerAddKeyOptionVariable(
    NULL,
    (UINT16)BootOption.OptionNumber,
    0,
    &F2,
    NULL
);
```

**To disable:** Comment out or remove this registration.

---

## 9. Variable GUID Reference

```c
gEfiGlobalVariableGuid = {
    0x8BE4DF61, 0x93CA, 0x11D2,
    {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C}
}
```

**Standard Boot Variables (in this namespace):**
- BootOrder
- Boot####
- BootNext
- BootCurrent
- Timeout
- OsIndications

---

## 10. Key Code Locations Summary

| Component | File | Line | Purpose |
|-----------|------|------|---------|
| Boot Entry Point | BdsEntry.c | 679 | BdsEntry() main function |
| Timeout Initialization | BdsEntry.c | 756-768 | Set Timeout from PCD |
| BootNext Processing | BdsEntry.c | 1067-1104 | Handle BootNext variable |
| Boot Order Loop | BdsEntry.c | 1118 | Call BootBootOptions() |
| Boot Option Filter | BdsEntry.c | 375-439 | Filter LOAD_OPTION_ACTIVE |
| Boot Execution | BmBoot.c | 1894 | EfiBootManagerBoot() |
| Platform Hooks | BdsPlatform.c | 889, 903 | Platform customization |
| PCD Definition | MdePkg.dec | 2630 | PcdPlatformBootTimeOut |

---

## Summary: Minimal Steps to Auto-Boot Your App

1. **Create Boot0005 variable** with:
   - FilePath to your MyShellApp
   - Attributes = LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_BOOT
   - Optional GUID: 11223344-5566-7788-99AA-BBCCDDEEFF00

2. **Set BootOrder** = {0x0005}

3. **In DSC file**, set:
   ```
   gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0
   ```

4. **Result**: Boot to MyShellApp immediately, no menu shown

---

## Alternative: Use BootNext for One-Time Boot

```c
// At UEFI shell:
set BootNext 0005
reset
```

**This will:**
- Boot Boot0005 on next restart
- Auto-delete BootNext after use
- Return to normal BootOrder after that

