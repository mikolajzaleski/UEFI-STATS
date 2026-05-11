# Code Examples: Setting Boot Variables Programmatically

## Example 1: Set BootNext to Boot Your App (Simple One-Time Boot)

```c
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>

/**
  Set BootNext to force boot to a specific option on next reset
  
  @param OptionNumber   The boot option number (e.g., 0x0005 for Boot0005)
  @return EFI_SUCCESS if successful
*/
EFI_STATUS
SetBootNext(
    UINT16 OptionNumber
)
{
    EFI_STATUS Status;
    
    Status = gRT->SetVariable(
        L"BootNext",
        &gEfiGlobalVariableGuid,
        EFI_VARIABLE_BOOTSERVICE_ACCESS | 
        EFI_VARIABLE_RUNTIME_ACCESS | 
        EFI_VARIABLE_NON_VOLATILE,
        sizeof(UINT16),
        &OptionNumber
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to set BootNext - %r\n", Status);
    } else {
        Print(L"SUCCESS: BootNext set to Boot%04X\n", OptionNumber);
        Print(L"Your application will boot on next reset.\n");
    }
    
    return Status;
}

/**
  Usage from your application:
  
  // Force boot to Boot0005 (MyShellApp) on next reset
  SetBootNext(0x0005);
  
  // Then application can trigger reset
  // gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
*/
```

---

## Example 2: Modify BootOrder (Persistent Default Boot)

```c
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Guid/GlobalVariable.h>

/**
  Reorder BootOrder to put specified option first
  
  @param PrimaryOptionNumber   Option to boot first (e.g., 0x0005)
  @return EFI_SUCCESS if successful
*/
EFI_STATUS
SetBootOrderWithAppFirst(
    UINT16 PrimaryOptionNumber
)
{
    EFI_STATUS Status;
    UINT16 *CurrentBootOrder = NULL;
    UINTN CurrentBootOrderSize = 0;
    UINT16 *NewBootOrder = NULL;
    UINTN NewBootOrderSize = 0;
    UINTN Index;
    UINTN InsertIndex = 1;
    
    // Read existing BootOrder
    Status = gRT->GetVariable(
        L"BootOrder",
        &gEfiGlobalVariableGuid,
        NULL,
        &CurrentBootOrderSize,
        NULL
    );
    
    if (Status == EFI_BUFFER_TOO_SMALL) {
        CurrentBootOrder = (UINT16 *)AllocatePool(CurrentBootOrderSize);
        if (CurrentBootOrder == NULL) {
            Print(L"ERROR: Memory allocation failed\n");
            return EFI_OUT_OF_RESOURCES;
        }
        
        Status = gRT->GetVariable(
            L"BootOrder",
            &gEfiGlobalVariableGuid,
            NULL,
            &CurrentBootOrderSize,
            CurrentBootOrder
        );
    }
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to read BootOrder - %r\n", Status);
        if (CurrentBootOrder != NULL) {
            FreePool(CurrentBootOrder);
        }
        return Status;
    }
    
    // Calculate new size (may be same if app already in order)
    NewBootOrderSize = CurrentBootOrderSize;
    
    // Check if PrimaryOptionNumber is already in BootOrder
    BOOLEAN AlreadyExists = FALSE;
    for (Index = 0; Index < CurrentBootOrderSize / sizeof(UINT16); Index++) {
        if (CurrentBootOrder[Index] == PrimaryOptionNumber) {
            AlreadyExists = TRUE;
            break;
        }
    }
    
    // Allocate new boot order
    NewBootOrder = (UINT16 *)AllocatePool(NewBootOrderSize);
    if (NewBootOrder == NULL) {
        Print(L"ERROR: Memory allocation failed\n");
        FreePool(CurrentBootOrder);
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Build new boot order: [PrimaryOption, other options except primary]
    NewBootOrder[0] = PrimaryOptionNumber;
    InsertIndex = 1;
    
    for (Index = 0; Index < CurrentBootOrderSize / sizeof(UINT16); Index++) {
        if (CurrentBootOrder[Index] != PrimaryOptionNumber) {
            if (InsertIndex < NewBootOrderSize / sizeof(UINT16)) {
                NewBootOrder[InsertIndex++] = CurrentBootOrder[Index];
            }
        }
    }
    
    // Write new BootOrder
    Status = gRT->SetVariable(
        L"BootOrder",
        &gEfiGlobalVariableGuid,
        EFI_VARIABLE_BOOTSERVICE_ACCESS | 
        EFI_VARIABLE_RUNTIME_ACCESS | 
        EFI_VARIABLE_NON_VOLATILE,
        NewBootOrderSize,
        NewBootOrder
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to set BootOrder - %r\n", Status);
    } else {
        Print(L"SUCCESS: BootOrder updated. Boot%04X is now first.\n", PrimaryOptionNumber);
    }
    
    FreePool(CurrentBootOrder);
    FreePool(NewBootOrder);
    
    return Status;
}

/**
  Usage:
  
  // Make Boot0005 (MyShellApp) the default boot option
  SetBootOrderWithAppFirst(0x0005);
  
  // Combined with PcdPlatformBootTimeOut|0, app boots immediately every time
*/
```

---

## Example 3: Create/Register Boot0005 for MyShellApp

```c
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Guid/GlobalVariable.h>

// Your application's GUID
#define MY_SHELL_APP_GUID \
    { 0x11223344, 0x5566, 0x7788, \
      { 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00 } }

/**
  Register MyShellApp as Boot0005
  
  Assumes your app is in FV (Firmware Volume) with the specified GUID
  
  @return EFI_SUCCESS if boot option created
*/
EFI_STATUS
RegisterMyShellAppBoot(
    VOID
)
{
    EFI_STATUS Status;
    EFI_BOOT_MANAGER_LOAD_OPTION BootOption;
    EFI_GUID MyShellAppGuid = MY_SHELL_APP_GUID;
    EFI_DEVICE_PATH_PROTOCOL *FilePath = NULL;
    
    // Create device path to FV file
    FilePath = FileDevicePath(NULL, L"ShellX64");  // Or your app name
    
    if (FilePath == NULL) {
        Print(L"ERROR: Could not create file path\n");
        return EFI_NOT_FOUND;
    }
    
    // Initialize boot option
    Status = EfiBootManagerInitializeLoadOption(
        &BootOption,
        5,  // Option number (Boot0005)
        LoadOptionTypeBoot,
        LOAD_OPTION_ACTIVE,  // Must have ACTIVE flag for auto-boot
        L"MyShellApp",  // Description
        FilePath,
        (UINT8 *)&MyShellAppGuid,
        sizeof(MyShellAppGuid)
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to initialize boot option - %r\n", Status);
        FreePool(FilePath);
        return Status;
    }
    
    // Save to variable storage
    Status = EfiBootManagerLoadOptionToVariable(&BootOption);
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to save boot option - %r\n", Status);
    } else {
        Print(L"SUCCESS: Boot0005 created for MyShellApp\n");
        Print(L"  GUID: %g\n", &MyShellAppGuid);
        Print(L"  Description: %s\n", BootOption.Description);
        Print(L"  Option Number: 0x%04X\n", BootOption.OptionNumber);
    }
    
    FreePool(FilePath);
    EfiBootManagerFreeLoadOption(&BootOption);
    
    return Status;
}

/**
  Usage:
  
  EFI_STATUS Status = RegisterMyShellAppBoot();
  if (!EFI_ERROR(Status)) {
      SetBootOrderWithAppFirst(0x0005);
      SetBootNext(0x0005);  // Force next boot to use it
  }
*/
```

---

## Example 4: Verify Boot Option Attributes

```c
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Guid/GlobalVariable.h>

/**
  Display Boot#### variable information
  
  @param OptionNumber   Boot option number (e.g., 5 for Boot0005)
*/
EFI_STATUS
DisplayBootOption(
    UINT16 OptionNumber
)
{
    EFI_STATUS Status;
    CHAR16 OptionName[BM_OPTION_NAME_LEN];
    EFI_BOOT_MANAGER_LOAD_OPTION BootOption;
    
    UnicodeSPrint(
        OptionName,
        sizeof(OptionName),
        L"Boot%04x",
        OptionNumber
    );
    
    Status = EfiBootManagerVariableToLoadOption(OptionName, &BootOption);
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Boot%04x not found - %r\n", OptionNumber, Status);
        return Status;
    }
    
    Print(L"\n%s Information:\n", OptionName);
    Print(L"  Description:    %s\n", BootOption.Description);
    Print(L"  Option Number:  0x%04X\n", BootOption.OptionNumber);
    Print(L"  Attributes:     0x%08X\n", BootOption.Attributes);
    Print(L"    - ACTIVE:           %s\n", 
          (BootOption.Attributes & LOAD_OPTION_ACTIVE) ? L"Yes" : L"No");
    Print(L"    - BOOT Category:    %s\n", 
          ((BootOption.Attributes & LOAD_OPTION_CATEGORY) == LOAD_OPTION_CATEGORY_BOOT) ? L"Yes" : L"No");
    Print(L"  Optional Data Size: %d bytes\n", BootOption.OptionalDataSize);
    
    if (BootOption.FilePath != NULL) {
        CHAR16 *PathStr = ConvertDevicePathToText(BootOption.FilePath, FALSE, FALSE);
        if (PathStr != NULL) {
            Print(L"  File Path: %s\n", PathStr);
            FreePool(PathStr);
        }
    }
    
    EfiBootManagerFreeLoadOption(&BootOption);
    return EFI_SUCCESS;
}

/**
  Display current BootOrder
*/
EFI_STATUS
DisplayBootOrder(
    VOID
)
{
    EFI_STATUS Status;
    UINT16 *BootOrder = NULL;
    UINTN BootOrderSize = 0;
    UINTN Index;
    
    Status = gRT->GetVariable(
        L"BootOrder",
        &gEfiGlobalVariableGuid,
        NULL,
        &BootOrderSize,
        NULL
    );
    
    if (Status == EFI_BUFFER_TOO_SMALL) {
        BootOrder = (UINT16 *)AllocatePool(BootOrderSize);
        Status = gRT->GetVariable(
            L"BootOrder",
            &gEfiGlobalVariableGuid,
            NULL,
            &BootOrderSize,
            BootOrder
        );
    }
    
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Could not read BootOrder - %r\n", Status);
        return Status;
    }
    
    Print(L"\nCurrent BootOrder:\n");
    for (Index = 0; Index < BootOrderSize / sizeof(UINT16); Index++) {
        Print(L"  [%d] Boot%04X\n", Index, BootOrder[Index]);
    }
    
    if (BootOrder != NULL) {
        FreePool(BootOrder);
    }
    
    return EFI_SUCCESS;
}

/**
  Usage:
  
  DisplayBootOption(0x0005);
  DisplayBootOrder();
*/
```

---

## Example 5: Complete Setup Script (UEFI Shell)

```bash
# From UEFI Shell (not C code, but useful reference):

# 1. Check current boot configuration
dmpstore -d BootOrder
dmpstore -d BootNext
dmpstore -d Boot0005

# 2. Set MyShellApp as Boot0005 (if it exists in FV)
# (This requires platform support; Boot0005 should be pre-created)

# 3. Make Boot0005 the first boot option
set BootOrder 0005 0001 0002

# 4. Optional: Set one-time boot (next reset only)
set BootNext 0005

# 5. Reset
reset

# After reset, MyShellApp should boot immediately!
```

---

## Important: UEFI Variable Attributes

When calling `SetVariable()`, use these attributes:

```c
// Standard runtime + non-volatile boot options
EFI_VARIABLE_BOOTSERVICE_ACCESS | 
EFI_VARIABLE_RUNTIME_ACCESS | 
EFI_VARIABLE_NON_VOLATILE
```

This allows:
- Boot Services code to read/write (bootservice access)
- Runtime code to access (runtime access)
- Persistence across reset (non-volatile)

---

## Checking Boot Option Status

Boot option status values after execution:

```c
// In EFI_BOOT_MANAGER_LOAD_OPTION structure:
EFI_STATUS Status;

// Possible values:
// EFI_SUCCESS          - Boot succeeded
// EFI_NOT_FOUND        - Boot image not found
// EFI_ACCESS_DENIED    - Boot device access denied
// EFI_DEVICE_ERROR     - Device error during boot
// EFI_INVALID_PARAMETER - Invalid boot option
```

See [BmBoot.c](../../MdeModulePkg/Library/UefiBootManagerLib/BmBoot.c) for full boot execution logic.

---

## Compilation Notes

Link with these libraries:

```ini
[LibraryClasses]
    UefiLib
    UefiBootManagerLib
    DevicePathLib
    UefiRuntimeServicesTableLib
```

Include these headers:

```c
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Guid/GlobalVariable.h>
```

