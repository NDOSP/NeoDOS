#include "ndr.h"

CHAR16* registryFileAddress = L"\\NEODOS\\OSDATA.NDR";

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Hello, World!\n");

    ConfigNode* tree;
    EFI_STATUS Status = getConfig(registryFileAddress, &tree);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (config) Unexpected error: %r\n", Status);
        return Status;
    }

    Print(L"%s\n", ConfigNodeGetStr16(&tree[0], (CHAR8*)"Bootloader/KernelLocation", L"UNKNOWN"));

    return EFI_SUCCESS;
}