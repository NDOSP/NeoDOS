#include "ndr.h"
#include "video.h"

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

    VIDEO_INFO vInfo = {
        .handle = NULL,
        .resolution = {
            .height = 0,
            .width = 0,
        },
    };
    Status = getPreferredResolution(&vInfo);
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) return Status;

    if (Status == EFI_NOT_FOUND) {
        vInfo.resolution.height = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Height", 1920);
        vInfo.resolution.width = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Width", 1080);
    }

    Print(L"INFO: (video) Using resolution %dx%d\n", vInfo.resolution.width, vInfo.resolution.height);

    VIDEO_FRAMEBUFFER fb;
    Status = setVideoMode(vInfo, &fb);
    if (EFI_ERROR(Status)) return Status;

    for(;;) {}

    return EFI_SUCCESS;
}