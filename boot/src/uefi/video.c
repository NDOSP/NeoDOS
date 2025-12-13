#include "video.h"

EFI_STATUS getPreferredResolution(OUT VIDEO_INFO* info) {
    EFI_STATUS Status;
    EFI_HANDLE* Handles = NULL;
    UINTN HandleCount = 0;
    
    if (info == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    info->handle = NULL;
    info->resolution.width = 0;
    info->resolution.height = 0;
    
    Status = LibLocateHandle(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &HandleCount, &Handles);
    if (EFI_ERROR(Status) || HandleCount == 0) {
        if (Handles) {
            FreePool(Handles);
        }
        Print(L"ERROR: (video) No graphics devices found\n");
        return EFI_NOT_FOUND;
    }
    
    Print(L"INFO: (video) Found %ld video device handles\n", HandleCount);
    
    EFI_STATUS Result = EFI_NOT_FOUND;
    
    for (UINTN i = 0; i < HandleCount; i++) {
        EFI_HANDLE deviceHandle = Handles[i];
        EFI_EDID_DISCOVERED_PROTOCOL* edidProtocol = NULL;
        
        Status = uefi_call_wrapper(gBS->HandleProtocol, 3, deviceHandle, 
                                   &gEfiEdidDiscoveredProtocolGuid, (VOID**)&edidProtocol);
        if (EFI_ERROR(Status) || edidProtocol == NULL) {
            continue;
        }
        
        Print(L"INFO: (video) Found edid on handle %d with size=%d\n", i, edidProtocol->SizeOfEdid);
        
        if (edidProtocol->Edid != NULL && edidProtocol->SizeOfEdid >= 0x80) {
            UINT8* edid = edidProtocol->Edid;
            
            for (int desc = 0; desc < 4; desc++) {
                UINT8* dtd = edid + 0x36 + (desc * 18);
            
                if (dtd[0] != 0 || dtd[1] != 0) {
                    UINT16 xRes = dtd[2] | ((dtd[4] & 0xF0) << 4);
                    UINT16 yRes = dtd[5] | ((dtd[7] & 0xF0) << 4);
                    
                    xRes = dtd[2] | ((UINT16)(dtd[4] & 0xF0) << 4);
                    yRes = dtd[5] | ((UINT16)(dtd[7] & 0xF0) << 4);
                    
                    Print(L"INFO: (video) Preferred resolution by EDID: %dx%d\n", xRes, yRes);
                    
                    info->handle = deviceHandle;
                    info->resolution.width = xRes;
                    info->resolution.height = yRes;
                    
                    Result = EFI_SUCCESS;
                    break;
                }
            }
            
            if (Result == EFI_SUCCESS) {
                break;
            }
        }
    }
    
    FreePool(Handles);
    
    if (Result == EFI_NOT_FOUND) {
        Print(L"WARNING: (video) Could not resolve display preferred resolution via EDID\n");
    }
    
    return Result;
}

EFI_STATUS setVideoMode(VIDEO_INFO vInfo, VIDEO_FRAMEBUFFER* fb) {
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    if (vInfo.handle) { Status = uefi_call_wrapper(gBS->HandleProtocol, 3, vInfo.handle, &gEfiGraphicsOutputProtocolGuid, (VOID**)&gop); } 
    else { Status = LibLocateProtocol(&gEfiGraphicsOutputProtocolGuid, (VOID**)&gop); }
    if (EFI_ERROR(Status)) return Status;

    
    for (UINT32 modeId = 0; modeId < gop->Mode->MaxMode; modeId++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN sizeOfInfo;
        Status = uefi_call_wrapper(gop->QueryMode, 4, gop, modeId, &sizeOfInfo, &info);

        if (vInfo.resolution.height > info->VerticalResolution || vInfo.resolution.width > info->HorizontalResolution) continue;
        switch (info->PixelFormat) {
            case PixelBlueGreenRedReserved8BitPerColor:
            case PixelRedGreenBlueReserved8BitPerColor:
                break;
            default:
                continue;
        }

        Print(L"INFO: (video) Framebuffer info:\n\tFramebuffer: %lX\n\tResolution: %dx%d\n\tScan line: %d\n\tPixel format: %d\n", gop->Mode->FrameBufferBase, info->HorizontalResolution, info->VerticalResolution, info->PixelsPerScanLine, info->PixelFormat);
        fb->fbHeight = info->VerticalResolution;
        fb->fbWidth = info->HorizontalResolution;
        fb->fbPtr = (VOID*)gop->Mode->FrameBufferBase;
        fb->fbSize = gop->Mode->FrameBufferSize;
        fb->fbScanlineBytes = info->PixelsPerScanLine;
        fb->pixelFormat = (VIDEO_TYPE)info->PixelFormat;

        Status = uefi_call_wrapper(gop->SetMode, 2, gop, modeId);
        if (EFI_ERROR(Status)) return Status;
        break;
    }

    return EFI_SUCCESS;
}