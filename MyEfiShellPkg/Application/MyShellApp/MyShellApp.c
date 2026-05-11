#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/ShellLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/Smbios.h>
#include <Protocol/PciIo.h>
#include <Guid/Acpi.h>
#include <IndustryStandard/SmBios.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <Guid/GlobalVariable.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#define DIAG_REPORT_FILE_NAME L"fs0:\\DiagReport.txt"
#define DIAG_LINE_BUFFER_SIZE 512
#define DIAG_ASCII_BUFFER_SIZE 1024

typedef struct {
  SHELL_FILE_HANDLE FileHandle;
} DIAG_CONTEXT;

STATIC
CHAR8 *
GetSmbiosString (
  IN EFI_SMBIOS_TABLE_HEADER *Record,
  IN UINT8                  StringIndex
  )
{
  CHAR8 *String;

  if (StringIndex == 0) {
    return "Not Specified";
  }

  String = (CHAR8 *)Record + Record->Length;
  while ((StringIndex > 1) && (*String != '\0')) {
    String += AsciiStrLen(String) + 1;
    StringIndex--;
  }

  if (*String == '\0') {
    return "Not Specified";
  }

  return String;
}

STATIC
CONST CHAR16 *
GetMemorySizeText (
  IN UINT16 Size,
  OUT CHAR16 *Buffer,
  IN UINTN   BufferSize
  )
{
  if (Size == 0) {
    return L"Unknown";
  }

  if (Size == 0xFFFF) {
    return L"> 32 GB";
  }

  if (Size & 0x8000) {
    UINTN MiB;

    MiB = Size & 0x7FFF;
    UnicodeSPrint(Buffer, BufferSize, L"%u MiB", MiB);
  } else {
    UnicodeSPrint(Buffer, BufferSize, L"%u KB", Size);
  }

  return Buffer;
}

STATIC
VOID
DiagEmptyLine (
  IN DIAG_CONTEXT *Context
  )
{
  Print(L"\n");
  if (Context != NULL && Context->FileHandle != NULL) {
    UINTN Size = 2;
    ShellWriteFile(Context->FileHandle, &Size, "\r\n");
  }
}

STATIC
EFI_STATUS
EFIAPI
DiagOutput (
  IN DIAG_CONTEXT *Context,
  IN CONST CHAR16 *Format,
  ...
  )
{
  VA_LIST    ArgList;
  CHAR16     UnicodeBuffer[DIAG_LINE_BUFFER_SIZE];
  CHAR8      AsciiBuffer[DIAG_ASCII_BUFFER_SIZE];
  UINTN      BufferSize;
  EFI_STATUS Status;

  VA_START(ArgList, Format);
  UnicodeVSPrint(UnicodeBuffer, sizeof(UnicodeBuffer), Format, ArgList);
  VA_END(ArgList);

  UINTN Cols, Rows;
  gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &Cols, &Rows);
  
  UINTN Len = StrLen(UnicodeBuffer);
  UINTN Pad = (Cols > Len) ? (Cols - Len) / 2 : 0;
  
  gST->ConOut->SetCursorPosition(gST->ConOut, Pad, gST->ConOut->Mode->CursorRow);
  Print(L"%s\n", UnicodeBuffer);

  if (Context == NULL || Context->FileHandle == NULL) {
    return EFI_SUCCESS;
  }

  StrCatS(UnicodeBuffer, DIAG_LINE_BUFFER_SIZE, L"\r\n");
  Status = UnicodeStrToAsciiStrS(UnicodeBuffer, AsciiBuffer, sizeof(AsciiBuffer));
  if (EFI_ERROR(Status)) { return Status; }
  
  BufferSize = AsciiStrLen(AsciiBuffer);
  return ShellWriteFile(Context->FileHandle, &BufferSize, AsciiBuffer);
}

STATIC
EFI_STATUS
DumpSmbiosSummary (
  IN DIAG_CONTEXT *Context
  )
{
  EFI_STATUS              Status;
  EFI_SMBIOS_PROTOCOL     *Smbios;
  EFI_SMBIOS_HANDLE       Handle;
  EFI_SMBIOS_TABLE_HEADER *Record;

  Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR(Status)) {
    DiagEmptyLine(Context);
    DiagOutput(Context, L"SMBIOS protocol not found: %r", Status);
    return Status;
  }

  DiagEmptyLine(Context);
  DiagOutput(Context, L"+=================================================================+");
  DiagOutput(Context, L"|                          SMBIOS Summary                         |");
  DiagOutput(Context, L"+=================================================================+");

  Handle = SMBIOS_HANDLE_PI_RESERVED;
  while (TRUE) {
    Status = Smbios->GetNext(Smbios, &Handle, NULL, &Record, NULL);
    if (EFI_ERROR(Status)) {
      break;
    }

    switch (Record->Type) {
      case SMBIOS_TYPE_BIOS_INFORMATION:
      {
        SMBIOS_TABLE_TYPE0 *Bios = (SMBIOS_TABLE_TYPE0 *)Record;
        DiagOutput(Context, L"| BIOS Vendor   : %-47a |", GetSmbiosString(Record, Bios->Vendor));
        DiagOutput(Context, L"| BIOS Version  : %-47a |", GetSmbiosString(Record, Bios->BiosVersion));
        DiagOutput(Context, L"| Release Date  : %-47a |", GetSmbiosString(Record, Bios->BiosReleaseDate));
        DiagOutput(Context, L"+-----------------------------------------------------------------+");
        break;
      }
      case SMBIOS_TYPE_SYSTEM_INFORMATION:
      {
        SMBIOS_TABLE_TYPE1 *System = (SMBIOS_TABLE_TYPE1 *)Record;
        DiagOutput(Context, L"| System Mfr    : %-47a |", GetSmbiosString(Record, System->Manufacturer));
        DiagOutput(Context, L"| Product Name  : %-47a |", GetSmbiosString(Record, System->ProductName));
        DiagOutput(Context, L"| Serial Number : %-47a |", GetSmbiosString(Record, System->SerialNumber));
        DiagOutput(Context, L"+-----------------------------------------------------------------+");
        break;
      }
      case SMBIOS_TYPE_PROCESSOR_INFORMATION:
      {
        SMBIOS_TABLE_TYPE4 *Processor = (SMBIOS_TABLE_TYPE4 *)Record;
        DiagOutput(Context, L"| Processor Mfr : %-47a |", GetSmbiosString(Record, Processor->ProcessorManufacturer));
        DiagOutput(Context, L"| Version       : %-47a |", GetSmbiosString(Record, Processor->ProcessorVersion));
        DiagOutput(Context, L"| Core Count    : %-47u |", Processor->CoreCount);
        DiagOutput(Context, L"+-----------------------------------------------------------------+");
        break;
      }
      case SMBIOS_TYPE_MEMORY_DEVICE:
      {
        SMBIOS_TABLE_TYPE17 *Memory = (SMBIOS_TABLE_TYPE17 *)Record;
        CHAR16 SizeText[64];
        DiagOutput(Context, L"| Memory Locator: %-47a |", GetSmbiosString(Record, Memory->DeviceLocator));
        DiagOutput(Context, L"| Size          : %-47s |", GetMemorySizeText(Memory->Size, SizeText, sizeof(SizeText)));
        DiagOutput(Context, L"| Speed         : %u MHz%-41s |", Memory->Speed, L"");
        DiagOutput(Context, L"+-----------------------------------------------------------------+");
        break;
      }
      default:
        break;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpAcpiTable (
  IN DIAG_CONTEXT *Context,
  IN UINT32       Signature,
  IN CONST CHAR16 *Name
  )
{
  EFI_ACPI_COMMON_HEADER *Table;

  Table = EfiLocateFirstAcpiTable(Signature);
  if (Table == NULL) {
    DiagOutput(Context, L"| %-14s | Not Found | %-37s |", Name, L"N/A");
    DiagOutput(Context, L"+----------------+-----------+---------------------------------------+");
    return EFI_NOT_FOUND;
  }

  if (Signature == SIGNATURE_32('F','A','C','P')) {
    EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *)Table;
    CHAR16 InfoStr[64];
    UnicodeSPrint(InfoStr, sizeof(InfoStr), L"DSDT: 0x%08x", Fadt->Dsdt);
    DiagOutput(Context, L"| %-14s | %.4a      | %-37s |", Name, (CHAR8 *)&Table->Signature, InfoStr);
  } else if (Signature == SIGNATURE_32('A','P','I','C')) {
    EFI_ACPI_2_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *Madt = (EFI_ACPI_2_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *)Table;
    UINT8 *Ptr = (UINT8 *)(Madt + 1);
    UINT8 *End = (UINT8 *)Madt + Madt->Header.Length;
    UINTN ProcessorCount = 0;
    while (Ptr < End) {
      EFI_ACPI_2_0_PROCESSOR_LOCAL_APIC_STRUCTURE *ApicStruct = (EFI_ACPI_2_0_PROCESSOR_LOCAL_APIC_STRUCTURE *)Ptr;
      if (ApicStruct->Type == EFI_ACPI_2_0_PROCESSOR_LOCAL_APIC && (ApicStruct->Flags & 1)) {
        ProcessorCount++;
      }
      Ptr += ApicStruct->Length;
    }
    CHAR16 InfoStr[64];
    UnicodeSPrint(InfoStr, sizeof(InfoStr), L"Enabled CPUs: %u", ProcessorCount);
    DiagOutput(Context, L"| %-14s | %.4a      | %-37s |", Name, (CHAR8 *)&Table->Signature, InfoStr);
  } else if (Signature == SIGNATURE_32('M','C','F','G')) {
    CHAR16 InfoStr[64];
    UnicodeSPrint(InfoStr, sizeof(InfoStr), L"PCIe Config Space");
    DiagOutput(Context, L"| %-14s | %.4a      | %-37s |", Name, (CHAR8 *)&Table->Signature, InfoStr);
  } else {
    DiagOutput(Context, L"| %-14s | %.4a      | %-37s |", Name, (CHAR8 *)&Table->Signature, L"Standard Table");
  }

  DiagOutput(Context, L"+----------------+-----------+---------------------------------------+");
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpAcpiInfo (
  IN DIAG_CONTEXT *Context
  )
{
  DiagEmptyLine(Context);
  DiagOutput(Context, L"+====================================================================+");
  DiagOutput(Context, L"|                            ACPI Tables                             |");
  DiagOutput(Context, L"+----------------+-----------+---------------------------------------+");
  DiagOutput(Context, L"| Table Name     | Signature | Extracted Information                 |");
  DiagOutput(Context, L"+----------------+-----------+---------------------------------------+");

  DumpAcpiTable(Context, SIGNATURE_32('X','S','D','T'), L"XSDT");
  DumpAcpiTable(Context, SIGNATURE_32('R','S','D','T'), L"RSDT");
  DumpAcpiTable(Context, SIGNATURE_32('F','A','C','P'), L"FADT");
  DumpAcpiTable(Context, SIGNATURE_32('A','P','I','C'), L"APIC (MADT)");
  DumpAcpiTable(Context, SIGNATURE_32('M','C','F','G'), L"MCFG");
  DumpAcpiTable(Context, SIGNATURE_32('H','P','E','T'), L"HPET");

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpPciDevices (
  IN DIAG_CONTEXT *Context
  )
{
  EFI_STATUS          Status;
  UINTN               HandleCount;
  EFI_HANDLE          *HandleBuffer;
  UINTN               Index;
  EFI_PCI_IO_PROTOCOL *PciIo;
  PCI_TYPE00          PciData;

  DiagEmptyLine(Context);
  DiagOutput(Context, L"+=============================================================+");
  DiagOutput(Context, L"|                         PCI Devices                         |");
  DiagOutput(Context, L"+-------+-------+--------+--------+---------------------------+");
  DiagOutput(Context, L"| B:D.F | VendID| DevID  | Class  | Description               |");
  DiagOutput(Context, L"+-------+-------+--------+--------+---------------------------+");

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleCount, &HandleBuffer);

  if (EFI_ERROR (Status)) {
    DiagOutput(Context, L"| No PCI devices found!                                       |");
    DiagOutput(Context, L"+-------+-------+--------+--------+---------------------------+");
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol(HandleBuffer[Index], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
    if (EFI_ERROR(Status)) continue;

    Status = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint32, 0, sizeof(PciData)/sizeof(UINT32), &PciData);
    if (EFI_ERROR(Status)) continue;

    UINTN Segment, Bus, Device, Function;
    Status = PciIo->GetLocation(PciIo, &Segment, &Bus, &Device, &Function);
    if (EFI_ERROR(Status)) { Segment = 0; Bus = 0; Device = 0; Function = 0; }

    DiagOutput(
      Context,
      L"| %02x:%02x.%x | %04x  | %04x   | %02x%02x%02x | %-25s |",
      Bus, Device, Function,
      PciData.Hdr.VendorId,
      PciData.Hdr.DeviceId,
      PciData.Hdr.ClassCode[2], PciData.Hdr.ClassCode[1], PciData.Hdr.ClassCode[0],
      L"PCI Device"
      );
  }

  DiagOutput(Context, L"+-------+-------+--------+--------+---------------------------+");

  gBS->FreePool (HandleBuffer);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpAdvancedInfo (
  IN DIAG_CONTEXT *Context
  )
{
  EFI_STATUS Status;
  UINT8 SecureBoot;
  UINTN Size = sizeof(SecureBoot);

  DiagEmptyLine(Context);
  DiagOutput(Context, L"+=================================================+");
  DiagOutput(Context, L"|           Advanced Security Analysis            |");
  DiagOutput(Context, L"+=================================================+");

  Status = gRT->GetVariable(L"SecureBoot", &gEfiGlobalVariableGuid, NULL, &Size, &SecureBoot);
  if (!EFI_ERROR(Status)) {
    DiagOutput(Context, L"| Secure Boot : %-33s |", SecureBoot == 1 ? L"Enabled" : L"Disabled");
  } else {
    DiagOutput(Context, L"| Secure Boot : %-33s |", L"Not Supported / Error");
  }

  UINT8 SetupMode;
  Size = sizeof(SetupMode);
  Status = gRT->GetVariable(L"SetupMode", &gEfiGlobalVariableGuid, NULL, &Size, &SetupMode);
  if (!EFI_ERROR(Status)) {
    DiagOutput(Context, L"| Setup Mode  : %-33s |", SetupMode == 1 ? L"User Mode" : L"Setup Mode");
  } else {
    DiagOutput(Context, L"| Setup Mode  : %-33s |", L"Unknown");
  }
  
  DiagOutput(Context, L"+-------------------------------------------------+");

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ExportReport (
  VOID
  )
{
  EFI_STATUS    Status;
  DIAG_CONTEXT  Context;

  Context.FileHandle = NULL;

  Status = ShellOpenFileByName(DIAG_REPORT_FILE_NAME, &Context.FileHandle, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR(Status)) {
    Print(L"\nFailed to open report file '%s': %r\n", DIAG_REPORT_FILE_NAME, Status);
    return Status;
  }

  DumpSmbiosSummary(&Context);
  DumpAcpiInfo(&Context);
  DumpPciDevices(&Context);
  DumpAdvancedInfo(&Context);

  ShellCloseFile(&Context.FileHandle);

  DiagEmptyLine(NULL);
  DiagOutput(NULL, L"===========================================================");
  DiagOutput(NULL, L"  SUCCESS: Report exported to fs0:\\DiagReport.txt         ");
  DiagOutput(NULL, L"===========================================================");
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
WaitForAnyKey (
  VOID
  )
{
  EFI_STATUS Status;
  UINTN      Index;
  EFI_INPUT_KEY Key;

  DiagEmptyLine(NULL);
  DiagOutput(NULL, L"[ Press any key to return to menu ]");
  
  Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
  if (!EFI_ERROR(Status)) {
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  UINTN Index;
  UINTN SelectedIndex = 0;
  CONST CHAR16 *MenuItems[] = {
    L"Show SMBIOS summary                           ",
    L"Show ACPI status                              ",
    L"Show PCI Devices                              ",
    L"Show Advanced Security Info                   ",
    L"Export diagnostic report                      ",
    L"Exit                                          "
  };
  UINTN NumItems = sizeof(MenuItems) / sizeof(MenuItems[0]);
  UINTN Cols, Rows;

  gST->ConOut->EnableCursor(gST->ConOut, FALSE);

  while (TRUE) {
    gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
    gST->ConOut->ClearScreen(gST->ConOut);
    
    gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &Cols, &Rows);

    UINTN MenuWidth = 52;
    UINTN StartCol = (Cols > MenuWidth) ? (Cols - MenuWidth) / 2 : 0;
    UINTN StartRow = (Rows > NumItems + 6) ? (Rows - (NumItems + 6)) / 2 : 0;
    
    gST->ConOut->SetCursorPosition(gST->ConOut, StartCol, StartRow);
    Print(L"====================================================");
    gST->ConOut->SetCursorPosition(gST->ConOut, StartCol, StartRow + 1);
    Print(L"           UEFI Hardware Diagnostic Tool            ");
    gST->ConOut->SetCursorPosition(gST->ConOut, StartCol, StartRow + 2);
    Print(L"====================================================");

    for (UINTN i = 0; i < NumItems; i++) {
      gST->ConOut->SetCursorPosition(gST->ConOut, StartCol, StartRow + 4 + i);
      if (i == SelectedIndex) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_BLACK | EFI_BACKGROUND_LIGHTGRAY);
        Print(L"  > %s ", MenuItems[i]);
        gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
      } else {
        Print(L"    %s ", MenuItems[i]);
      }
    }
    
    CONST CHAR16 *HelpText = L"Use Up/Down arrows to select, Enter to confirm.";
    UINTN HelpCol = (Cols > StrLen(HelpText)) ? (Cols - StrLen(HelpText)) / 2 : 0;
    gST->ConOut->SetCursorPosition(gST->ConOut, HelpCol, StartRow + 4 + NumItems + 2);
    Print(HelpText);

    Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
    if (EFI_ERROR(Status)) continue;

    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (EFI_ERROR(Status)) continue;

    if (Key.ScanCode == SCAN_UP) {
      if (SelectedIndex > 0) SelectedIndex--;
    } else if (Key.ScanCode == SCAN_DOWN) {
      if (SelectedIndex < NumItems - 1) SelectedIndex++;
    } else if (Key.UnicodeChar == L'\r' || Key.UnicodeChar == L'\n') {
      
      gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
      gST->ConOut->ClearScreen(gST->ConOut);
      gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
      
      switch (SelectedIndex) {
        case 0:
          DumpSmbiosSummary(NULL);
          WaitForAnyKey();
          break;
        case 1:
          DumpAcpiInfo(NULL);
          WaitForAnyKey();
          break;
        case 2:
          DumpPciDevices(NULL);
          WaitForAnyKey();
          break;
        case 3:
          DumpAdvancedInfo(NULL);
          WaitForAnyKey();
          break;
        case 4:
          ExportReport();
          WaitForAnyKey();
          break;
        case 5:
          gST->ConOut->SetAttribute(gST->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
          gST->ConOut->ClearScreen(gST->ConOut);
          gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
          gST->ConOut->EnableCursor(gST->ConOut, TRUE);
          return EFI_SUCCESS;
      }
    }
  }
}
