#include "file.h"

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Hello, World!\n");

    VOID* fileData;
    EFI_STATUS Status = loadFile(L"\\NEODOS\\TEST.TXT", &fileData);
    if (EFI_ERROR(Status)) return Status;

    for(;;) {}
    return EFI_SUCCESS;
}