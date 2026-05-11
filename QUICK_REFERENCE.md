# Quick Reference: Auto-Boot Your EFI Shell App in OVMF

## The 3 Key Mechanisms

### 1. **PcdPlatformBootTimeOut** (Boot Delay Control)
- **Location:** [MdePkg/MdePkg.dec:2630](../MdePkg/MdePkg.dec#L2630)
- **Default:** `0xFFFF` (wait forever for user input)
- **Set to `0`** in your DSC to boot immediately without menu

```ini
# In OvmfPkg/OvmfPkgX64.dsc
gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0
```

### 2. **BootOrder Variable** (Persistent Boot Sequence)
- **GUID:** `8BE4DF61-93CA-11D2-AA0D-00E098032B8C` (gEfiGlobalVariableGuid)
- **Type:** Array of UINT16 values (boot option numbers)
- **Format:** {0x0005, 0x0001, 0x0002, ...}

Set via UEFI Shell:
```bash
set BootOrder 5 1 2
```

Set programmatically:
```c
UINT16 NewOrder[] = {0x0005};  // Your app only
gRT->SetVariable(L"BootOrder", &gEfiGlobalVariableGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | 
    EFI_VARIABLE_NON_VOLATILE, sizeof(NewOrder), &NewOrder);
```

### 3. **BootNext Variable** (One-Time Boot Override)
- **GUID:** `8BE4DF61-93CA-11D2-AA0D-00E098032B8C` (gEfiGlobalVariableGuid)
- **Type:** UINT16 (single option number)
- **Auto-deleted:** After consumption

Set via UEFI Shell:
```bash
set BootNext 0005
reset
```

Set programmatically:
```c
UINT16 NextOption = 0x0005;
gRT->SetVariable(L"BootNext", &gEfiGlobalVariableGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | 
    EFI_VARIABLE_NON_VOLATILE, sizeof(NextOption), &NextOption);
```

---

## Your MyShellApp Setup

**Option Number:** 0x0005 (Boot0005)  
**GUID:** 11223344-5566-7788-99AA-BBCCDDEEFF00  
**Description:** "MyShellApp"

### Boot0005 Variable Structure
```c
typedef struct {
    UINT32              Attributes;           // LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_BOOT
    UINT16              FilePathListLength;   // Device path length
    CHAR16              Description[256];     // "MyShellApp"
    EFI_DEVICE_PATH     FilePath[];          // Path to your app
    UINT8               OptionalData[];      // Your GUID here
} EFI_LOAD_OPTION;
```

---

## Complete Auto-Boot Flow

```
┌─ Firmware Start (DXE Phase)
│
├─ BdsEntry() called [BdsEntry.c:679]
│
├─ Read PcdPlatformBootTimeOut = 0 [BdsEntry.c:756]
│  → Timeout = 0 (no menu wait)
│
├─ Check BootNext variable [BdsEntry.c:1067]
│  → If set: Jump to Boot0005
│  → If not set: Continue to BootOrder
│
├─ Get BootOrder = {0x0005} [BdsEntry.c:1118]
│  → BootBootOptions() iterates array
│
├─ Load Boot0005 [BdsEntry.c:375-439]
│  → Check LOAD_OPTION_ACTIVE flag: ✓
│  → Check LOAD_OPTION_CATEGORY_BOOT: ✓
│
├─ Execute Boot0005 [BmBoot.c:1894]
│  → EfiBootManagerBoot() runs your app
│
└─ MyShellApp Launches ✓
```

---

## Code Locations (Quick Lookup)

| Task | File | Line(s) |
|------|------|---------|
| Boot entry point | BdsEntry.c | 679 |
| Timeout setup | BdsEntry.c | 756-768 |
| BootNext read | BdsEntry.c | 1067-1104 |
| BootOrder iteration | BdsEntry.c | 1118-1122 |
| LOAD_OPTION filtering | BdsEntry.c | 396-406 |
| Boot execution | BmBoot.c | 1894-2100 |
| PCD definition | MdePkg.dec | 2630 |
| OVMF defaults | OvmfPkgX64.dsc | 721 |

---

## Immediate Action Steps

### Step 1: Modify OVMF DSC
```bash
vi edk2/OvmfPkg/OvmfPkgX64.dsc
```

Find line 721 and change to:
```ini
gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0
```

### Step 2: Ensure Boot0005 Exists
Use UEFI Shell to verify:
```bash
dmpstore -d BootCurrent
dmpstore -d Boot0005
```

### Step 3: Set BootOrder in Shell
```bash
set BootOrder 0005
reset
```

### Step 4: Verify MyShellApp Boots Immediately
No menu should appear. App launches directly.

---

## Troubleshooting

### Menu Still Appears?
- Check PcdPlatformBootTimeOut value (should be 0)
- Verify Boot0005 has LOAD_OPTION_ACTIVE attribute
- Check BootOrder is {0x0005}

### App Doesn't Boot?
- Verify Boot0005 variable exists: `dmpstore -d Boot0005`
- Check FilePath in Boot0005 (use `dmpstore -d Boot0005 -s vars.txt`)
- Ensure app is in correct location

### Still Showing Boot Menu?
- Override PlatformBootManagerLib (custom platform code)
- Remove F2/ESC key handlers
- See BdsPlatform.c lines 128-178

---

## Advanced: Bypass Menu Via EFI_OS_INDICATIONS

Clear the menu indication bit:
```c
UINT64 OsIndications = 0;
gRT->SetVariable(L"OsIndications", &gEfiGlobalVariableGuid,
    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS |
    EFI_VARIABLE_NON_VOLATILE, sizeof(OsIndications), &OsIndications);
```

This prevents OS-requested boot to menu (`EFI_OS_INDICATIONS_BOOT_TO_FW_UI` bit).

---

## Reference Files in Repository

- **Boot Main Logic:** edk2/MdeModulePkg/Universal/BdsDxe/BdsEntry.c
- **Boot Manager Lib:** edk2/MdeModulePkg/Library/UefiBootManagerLib/BmBoot.c
- **OVMF Platform:** edk2/OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c
- **PCD Definitions:** edk2/MdePkg/MdePkg.dec (line 2630)
- **DSC Config:** edk2/OvmfPkg/OvmfPkgX64.dsc (line 721)

