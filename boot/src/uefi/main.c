#include "ndr.h"
#include "video.h"
#include "kernel.h"
#include "memory.h"
#include "acpi.h"
#include "nff.h"

extern UINT8 kernelJump_start, kernelJump_end;

UINTN kernelJump_size(void) {
    return &kernelJump_end - &kernelJump_start;
}

extern VOID kernelJump(VOID* entry, UINT64 pml4);

CHAR16* registryFileAddress = L"\\NEODOS\\OSDATA.NDR";

VOID EFIAPI errorHandler(IN EFI_STATUS Status, IN EFI_HANDLE ImageHandle) {
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut, 1, 1);
    Print(L"\n");
    Print(L"================================\n");
    Print(L"NeoDOS Boot Error\n");
    Print(L"================================\n\n");

    Print(L"Error Name: %r\n\n", Status);

    Print(L"NeoDOS Bootloader encountered an unexpected error and cannot continue.\n");
    Print(L"Please check your system configuration or firmware settings, and try rebooting.\n\n");

    Print(L"System halted. Waiting 10 seconds...\n");

    uefi_call_wrapper(BS->Stall, 1, 10 * 1000 * 1000);
    uefi_call_wrapper(BS->Exit, 4, ImageHandle, Status, 0, NULL);
}

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    EFI_STATUS Status;
    BOOT_INFO bInfo;

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    Status = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Print(L"Hello, World!\n");
    Print(L"INFO: (loader) ImageHandle: 0x%lX, SystemTable: 0x%lX, ImageBase: 0x%lX\n", ImageHandle, SystemTable, LoadedImage->ImageBase);

    ConfigNode* tree;
    Status = getConfig(registryFileAddress, &tree);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (config) Unexpected error: %r\n", Status);
        errorHandler(Status, ImageHandle);
    }

    VIDEO_INFO vInfo = {
        .handle = NULL,
        .resolution = {
            .height = 0,
            .width = 0,
        },
    };
    Status = getPreferredResolution(&vInfo);
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) errorHandler(Status, ImageHandle);

    if (Status == EFI_NOT_FOUND) {
        vInfo.resolution.height = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Height", 1920);
        vInfo.resolution.width = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Width", 1080);
    }

    Print(L"INFO: (video) Using resolution %dx%d\n", vInfo.resolution.width, vInfo.resolution.height);

    Status = loadElf(ConfigNodeGetStr16(&tree[0], (CHAR8*)"Bootloader/KernelLocation", L"\\NEODOS\\KERNEL.BIN"), &bInfo.kInfo);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = findACPI(&bInfo.rsdp);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    UINTN maxCPU = ConfigNodeGetU64(&tree[0], (CHAR8*)"Bootloader/MaxCPU", 4);
    bInfo.stackCount = maxCPU;

    Status = loadFont(ConfigNodeGetStr16(&tree[0], (CHAR8*)"Bootloader/FontLocation", L"\\NEODOS\\FONT.BLL"), &bInfo.font);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    bInfo.fontScale = ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Scale", 1);

    Status = setVideoMode(vInfo, &bInfo.fb);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);
    if (!((UINTN)bInfo.fb.pixelFormat == (UINTN)PixelBlueGreenRedReserved8BitPerColor || (UINTN)bInfo.fb.pixelFormat == (UINTN)PixelRedGreenBlueReserved8BitPerColor)) errorHandler(EFI_UNSUPPORTED, ImageHandle);

    Status = initPage(&bInfo.pml4);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = mapKernelSpace(bInfo.pml4, &bInfo.kInfo, &bInfo.fb, maxCPU, bInfo.font);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    CopyMem((VOID*)bInfo.kInfo.bootInfo.paddr, (VOID*)&bInfo, sizeof(bInfo));
    Status = addPage(bInfo.pml4, (UINT64)&kernelJump_start, (UINT64)&kernelJump_start, ENTRY_PRESENT | ENTRY_RW);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = getMemoryMap(&bInfo.map);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = uefi_call_wrapper(gBS->ExitBootServices, 2, ImageHandle, bInfo.map.mapKey);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);
    
    kernelJump((VOID*)bInfo.kInfo.entryPoint, (UINT64)bInfo.pml4);

    errorHandler(EFI_LOAD_ERROR, ImageHandle);
    return EFI_LOAD_ERROR;
}