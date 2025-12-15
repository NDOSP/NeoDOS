#include "ndr.h"
#include "video.h"
#include "kernel.h"
#include "memory.h"

typedef struct {
    UINT64* pml4;
    UINT64  stackCount;
    UINT64* stackAddrs;
    VIDEO_FRAMEBUFFER fb;
    KERNEL_INFO kInfo;
} BOOT_INFO;

CHAR16* registryFileAddress = L"\\NEODOS\\OSDATA.NDR";

VOID EFIAPI errorHandler(IN EFI_STATUS Status) {
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut, 1, 1);
    Print(L"\n");
    Print(L"================================\n");
    Print(L"NeoDOS Boot Error\n");
    Print(L"================================\n\n");

    Print(L"Error Name: %r\n\n", Status);

    Print(L"NeoDOS Bootloader encountered an unexpected error and cannot continue.\n");
    Print(L"Please check your system configuration or firmware settings, and try rebooting.\n\n");

    Print(L"System halted.\n");
    for (;;) {}
}


EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Hello, World!\n");

    ConfigNode* tree;
    EFI_STATUS Status = getConfig(registryFileAddress, &tree);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (config) Unexpected error: %r\n", Status);
        errorHandler(Status);
    }

    VIDEO_INFO vInfo = {
        .handle = NULL,
        .resolution = {
            .height = 0,
            .width = 0,
        },
    };
    Status = getPreferredResolution(&vInfo);
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) errorHandler(Status);

    if (Status == EFI_NOT_FOUND) {
        vInfo.resolution.height = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Height", 1920);
        vInfo.resolution.width = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Width", 1080);
    }

    Print(L"INFO: (video) Using resolution %dx%d\n", vInfo.resolution.width, vInfo.resolution.height);

    KERNEL_INFO kInfo;
    Status = loadElf(ConfigNodeGetStr16(&tree[0], (CHAR8*)"Bootloader/KernelLocation", L"\\NEODOS\\KERNEL.BIN"), &kInfo);
    if (EFI_ERROR(Status)) errorHandler(Status);

    UINTN maxCPU = ConfigNodeGetU64(&tree[0], (CHAR8*)"Bootloader/MaxCPU", 4);

    VIDEO_FRAMEBUFFER fb;
    Status = setVideoMode(vInfo, &fb);
    if (EFI_ERROR(Status)) errorHandler(Status);

    UINT64* pml4;
    Status = initPage(&pml4);
    if (EFI_ERROR(Status)) errorHandler(Status);

     UINT64* stackAddrs;
     Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, maxCPU * sizeof(UINT64*), (VOID**)&stackAddrs);
     if (EFI_ERROR(Status)) errorHandler(Status);

     Status = mapKernelSpace(pml4, &kInfo, &fb, maxCPU, stackAddrs);
     if (EFI_ERROR(Status)) errorHandler(Status);

     for(;;) {}

    return EFI_SUCCESS;
}