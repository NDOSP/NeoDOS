#include "file.h"

EFI_STATUS loadFile(IN CHAR16* path, OUT VOID** data, OUT UINTN* fileSize) {
    if (data == NULL) {
        Print(L"ERROR: (file) loadFile called with NULL data parameter\n");
        return EFI_INVALID_PARAMETER;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFileSystemProtocol;
    EFI_STATUS Status = LibLocateProtocol(&gEfiSimpleFileSystemProtocolGuid, (VOID**)&SimpleFileSystemProtocol);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (file) LibLocateProtocol failed: %r (GUID: %g)\n", Status, &gEfiSimpleFileSystemProtocolGuid);
        return Status;
    }

    Print(L"INFO: (file) SimpleFileSystemProtocol: 0x%lX\n", SimpleFileSystemProtocol);
    Print(L"INFO: (file) OpenVolume function: 0x%lX\n", SimpleFileSystemProtocol->OpenVolume);

    EFI_FILE_PROTOCOL* fileRoot;
    Status = uefi_call_wrapper(SimpleFileSystemProtocol->OpenVolume, 2, SimpleFileSystemProtocol, &fileRoot);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (file) OpenVolume failed: %r\n", Status);
        return Status;
    }

    EFI_FILE_PROTOCOL* file;
    Status = uefi_call_wrapper(fileRoot->Open, 5, fileRoot, &file, path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (file) Cannot open file: '%s': %r\n", path, Status);
        fileRoot->Close(fileRoot);
        return Status;
    }

    UINTN BufferSize = 0;
    Status = uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid, &BufferSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"ERROR: (file) GetInfo (size query) failed: %r\n", Status);
        fileRoot->Close(fileRoot);
        file->Close(file);
        return Status;
    }

    VOID* fileInfoBuffer = AllocatePool(BufferSize);
    if (fileInfoBuffer == NULL) {
        Print(L"ERROR: (file) Failed to allocate %lu bytes for file info\n", BufferSize);
        fileRoot->Close(fileRoot);
        file->Close(file);
        return EFI_OUT_OF_RESOURCES;
    }

    Print(L"INFO: (file) Allocated %lu bytes for fileInfo\n", BufferSize);
    Status = uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid, &BufferSize, fileInfoBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (file) GetInfo (data read) failed: %r\n", Status);
        FreePool(fileInfoBuffer);
        fileRoot->Close(fileRoot);
        file->Close(file);
        return Status;
    }

    EFI_FILE_INFO* fileInfo = (VOID*)fileInfoBuffer;
    *fileSize = fileInfo->FileSize;
    Print(L"INFO: (file) File size: %lu bytes\n", *fileSize);

    if (fileInfo->Attribute & EFI_FILE_DIRECTORY) {
        Print(L"ERROR: (file) Is directory: '%s'\n", path);
        FreePool(fileInfoBuffer);
        fileRoot->Close(fileRoot);
        file->Close(file);
        return EFI_INVALID_PARAMETER;
    }

    *data = AllocatePool(*fileSize);
    if (*data == NULL) {
        Print(L"ERROR: (file) Failed to allocate %lu bytes for file data\n", *fileSize);
        FreePool(fileInfoBuffer);
        FreePool(*data);
        fileRoot->Close(fileRoot);
        file->Close(file);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = uefi_call_wrapper(file->Read, 3, file, fileSize, *data);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (file) Read failed: %r\n", Status);
        FreePool(fileInfoBuffer);
        FreePool(*data);
        fileRoot->Close(fileRoot);
        file->Close(file);
        return Status;
    }

    if (*fileSize != fileInfo->FileSize) {
        Print(L"ERROR: (file) Read file failed\n");
        FreePool(fileInfoBuffer);
        FreePool(*data);
        fileRoot->Close(fileRoot);
        file->Close(file);
        return EFI_DEVICE_ERROR;
    }

    FreePool(fileInfoBuffer);
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(fileRoot->Close, 1, fileRoot);

    Print(L"INFO: (file) Succesfully loaded %s file\n", path);
    return EFI_SUCCESS;
}