// vim: set et:

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/HobLib.h>


extern EFI_GUID gTestHobGuid;
extern EFI_GUID gAmiTreePpiGuid;
extern EFI_GUID gTrEE_HashLogExtendPpiGuid;
extern EFI_GUID gPeiTcgPpiGuid;
extern EFI_GUID gPeiTpmPpiGuid;

void* hob = NULL;
void* end = NULL;


EFI_STATUS
EFIAPI
PpiNotifyCallback (
    IN EFI_PEI_SERVICES          **PeiServices,
    IN EFI_PEI_NOTIFY_DESCRIPTOR *NotifyDescriptor,
    IN VOID                      *Ppi
    )
{
    if (hob + 5 < end) {
        *((UINT8*)hob++) = 'x';
        *((UINT8*)hob++) = 1;
        *((UINT32*)hob)  = 0xFFFFFFFF & (UINTN)Ppi;
        hob += 4;
    }
    return EFI_SUCCESS;
}

void
place_EOHOB(UINT8* hob)
{
    hob[0] = 'E';
    hob[1] = 'O';
    hob[2] = 'H';
    hob[3] = 'O';
    hob[4] = 'B';
}

EFI_STATUS
LocateOrNotify(EFI_GUID* guid) {
    EFI_STATUS Status;
    void* ppi = NULL;

    Status = PeiServicesLocatePpi(guid, 0, NULL, &ppi);
    if (EFI_SUCCESS == Status) {
        *((UINT8*)hob++) = 'x';
        *((UINT8*)hob++) = 1;
        *((UINT32*)hob) = 0xFFFFFFFF & (UINTN)ppi;
        hob += 4;
    } else {
        *((UINT8*)hob++) = 'S';
        *((UINT8*)hob++) = 1;
        *((UINT8*)hob++) = Status & 0xFF;

        EFI_PEI_NOTIFY_DESCRIPTOR* notify;
        if (EFI_SUCCESS == (Status = PeiServicesAllocatePool(0xC, (VOID**)&notify))) {
            notify->Flags = EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST;
            notify->Guid = guid;
            notify->Notify = PpiNotifyCallback;

            Status = PeiServicesNotifyPpi(notify);
        }
    }

    if (EFI_SUCCESS != Status) {
        *((UINT8*)hob++) = 'S';
        *((UINT8*)hob++) = 1;
        *((UINT8*)hob++) = Status & 0xFF;
    }
    return Status;
}


EFI_STATUS
EFIAPI
TPMHelloEntryPoint(
        IN       EFI_PEI_FILE_HANDLE FileHandle,
        IN CONST EFI_PEI_SERVICES    **PeiServices
) {
    // https://edk2-docs.gitbook.io/edk-ii-module-writer-s-guide/7_pre-efi_initialization_modules/76_communicate_with_dxe_modules
    UINTN len = 32;
    hob = BuildGuidHob(&gTestHobGuid, len);
    if (!hob) {
        if ((hob = BuildGuidHob(&gTestHobGuid, 5)))
            place_EOHOB(hob);
        return EFI_OUT_OF_RESOURCES;
    }
    end = hob + len;
    place_EOHOB(end - 5);

    LocateOrNotify(&gAmiTreePpiGuid);
    LocateOrNotify(&gTrEE_HashLogExtendPpiGuid);
    LocateOrNotify(&gPeiTcgPpiGuid);
    LocateOrNotify(&gPeiTpmPpiGuid);

    return EFI_SUCCESS;
}
