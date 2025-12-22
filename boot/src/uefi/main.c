#include "ndr.h"
#include "video.h"
#include "kernel.h"
#include "memory.h"
#include "acpi.h"
#include "nff.h"

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
    BOOT_INFO bInfo;

    Print(L"Hello, World!\n");

    ConfigNode* tree;
    EFI_STATUS Status = getConfig(registryFileAddress, &tree);
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

    Status = initPage(&bInfo.pml4);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = mapKernelSpace(bInfo.pml4, &bInfo.kInfo, &bInfo.fb, maxCPU, bInfo.font);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    CopyMem((VOID*)bInfo.kInfo.bootInfo.paddr, (VOID*)&bInfo, sizeof(bInfo));

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    Status = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    UINTN pages = (LoadedImage->ImageSize + 0xFFF) / PAGE_SIZE;
    for (UINTN i = 0; i < pages; i++) {
        Status = addPage(bInfo.pml4, (UINT64)LoadedImage->ImageBase + i * PAGE_SIZE, (UINT64)LoadedImage->ImageBase + i * PAGE_SIZE, ENTRY_PRESENT | ENTRY_RW);
        if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);
    }

    UINT64 rsp, rbp;
    asm volatile(
        "mov %%rsp, %0\n"
        "mov %%rbp, %1\n"
        : "=r"(rsp), "=r"(rbp)
        :
        : "memory"
    );

    UINT64 stack_low  = (rsp < rbp ? rsp : rbp) & ~0xFFF;
    UINT64 stack_high = ((rsp > rbp ? rsp : rbp) + PAGE_SIZE) & ~0xFFF;
    UINTN stackPages = (stack_high - stack_low) / PAGE_SIZE;
    
    for (UINTN i = 0; i < stackPages; i++) {
        Status = addPage(bInfo.pml4, stack_low + i*PAGE_SIZE, stack_low + i*PAGE_SIZE, ENTRY_PRESENT | ENTRY_RW);
        if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);
    }

    Status = getMemoryMap(&bInfo.map);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = uefi_call_wrapper(gBS->ExitBootServices, 2, ImageHandle, bInfo.map.mapKey);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);
    
    VOID* entrypoint = (VOID*)bInfo.kInfo.entryPoint;
    asm volatile(
        "mov %0, %%cr3"
        :
        : "r"(bInfo.pml4)
        : "memory"
    );
    asm volatile(
        "jmp *%0"
        :
        : "r"(entrypoint)
    );

    errorHandler(EFI_LOAD_ERROR, ImageHandle);
    return EFI_LOAD_ERROR;
}