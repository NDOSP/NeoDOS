#include "file.h"

const CHAR16* registryFileAddress = L"\\NEODOS\\OSDATA.NDR";

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Hello, World!\n");

    VOID* fileData;
    EFI_STATUS Status = loadFile(L"\\NEODOS\\OSDATA.NDR", &fileData);
    if (EFI_ERROR(Status)) return Status;

    return EFI_SUCCESS;
}