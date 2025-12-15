#include "acpi.h"

EFI_STATUS findACPI(RSDP** rdsp) {
    EFI_GUID acpi20Guid = ACPI_20_TABLE_GUID;

    BOOLEAN foundInUefi = FALSE;
    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE* ct = &gST->ConfigurationTable[i];
        Print(L"INFO: (acpi) Checking 0x%lX from 0x%lX\n", ct->VendorTable, ct);
        Print(L"INFO: (acpi) Vendor GUID: %g\n", &ct->VendorGuid);

        if (CompareGuid(&acpi20Guid, &ct->VendorGuid)) {
            *rdsp = (RSDP*)(UINTN)ct->VendorTable;
            Print(L"INFO: (acpi) Found RDSP at 0x%lX\n", ct->VendorTable);
            Print(L"INFO: (acpi) RSDP Signature: %.8a\n", (*rdsp)->Signature);

            Print(L"INFO: (acpi) ACPI Revision: %d\n", (UINTN)(*rdsp)->Revision);
            if (CompareMem((*rdsp)->Signature, "RSD PTR ", 8) != 0) continue;
            
            Print(L"INFO: (acpi) OEM ID: ");
            for (UINTN i = 0; i < 6; i++) {
                AsciiPrint((CHAR8*)"%02x ", (*rdsp)->OemId[i]);
            }
            Print(L"\n");

            foundInUefi = TRUE;
            break;
        }
    }

    if (foundInUefi) {
        return EFI_SUCCESS;
    }

    return EFI_NOT_FOUND;
}