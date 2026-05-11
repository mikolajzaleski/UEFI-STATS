#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UnitTestLib.h>
#include <Library/PrintLib.h>

STATIC
CONST CHAR16 *
GetMemorySizeText (
  IN UINT16 Size,
  OUT CHAR16 *Buffer,
  IN UINTN   BufferSize
  )
{
  if (Size == 0) { return L"Unknown"; }
  if (Size == 0xFFFF) { return L"> 32 GB"; }
  if (Size & 0x8000) {
    UINTN MiB = Size & 0x7FFF;
    UnicodeSPrint(Buffer, BufferSize, L"%u MiB", MiB);
  } else {
    UnicodeSPrint(Buffer, BufferSize, L"%u KB", Size);
  }
  return Buffer;
}

UNIT_TEST_STATUS
EFIAPI
TestGetMemorySizeText (
  IN UNIT_TEST_CONTEXT           Context
  )
{
  CHAR16 Buffer[64];
  
  GetMemorySizeText(0, Buffer, sizeof(Buffer));
  UT_ASSERT_EQUAL(StrCmp(Buffer, L"Unknown"), 0);

  GetMemorySizeText(0xFFFF, Buffer, sizeof(Buffer));
  UT_ASSERT_EQUAL(StrCmp(Buffer, L"> 32 GB"), 0);
  
  GetMemorySizeText(1024, Buffer, sizeof(Buffer));
  UT_ASSERT_EQUAL(StrCmp(Buffer, L"1024 KB"), 0);
  
  GetMemorySizeText(0x8000 | 2048, Buffer, sizeof(Buffer));
  UT_ASSERT_EQUAL(StrCmp(Buffer, L"2048 MiB"), 0);

  return UNIT_TEST_PASSED;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  UNIT_TEST_FRAMEWORK_HANDLE Framework;
  UNIT_TEST_SUITE_HANDLE     DiagSuite;

  Status = InitUnitTestFramework(&Framework, "Diagnostic App Tests", "DiagApp", "1.0");
  if (EFI_ERROR(Status)) { return Status; }

  Status = CreateUnitTestSuite(&DiagSuite, Framework, "Memory Parsing Tests", "Diag.Memory", NULL, NULL);
  if (EFI_ERROR(Status)) { return Status; }

  AddTestCase(DiagSuite, "Test SMBIOS memory size parsing", "Diag.Memory.Size", TestGetMemorySizeText, NULL, NULL, NULL);

  Status = RunAllTestSuites(Framework);
  FreeUnitTestFramework(Framework);
  return Status;
}
