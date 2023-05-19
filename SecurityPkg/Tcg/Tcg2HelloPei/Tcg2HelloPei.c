// vim: set et:

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/HobLib.h>


extern EFI_GUID gTestHobGuid;
extern EFI_GUID gAmiTreePpiGuid;
extern EFI_GUID gTrEE_HashLogExtendPpiGuid;
extern EFI_GUID gAmiPeiTcgPpiGuid;

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
        *((UINT32*)hob)  = (UINT32)Ppi;
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

void
LocateOrNotify(EFI_GUID* guid) {
    EFI_STATUS Status;
    void* ppi = NULL;

    Status = PeiServicesLocatePpi(guid, 0, NULL, &ppi);
    if (!Status) {
        *((UINT32*)hob) = (UINT32)ppi;
    } else {
        *((UINT32*)hob) = (UINT32)Status;

        EFI_PEI_NOTIFY_DESCRIPTOR notify = {
            (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
            guid,
            PpiNotifyCallback
        };
        PeiServicesNotifyPpi(&notify);
    }
    hob += 4;
}


EFI_STATUS
EFIAPI
TPMHelloEntryPoint(
        IN       EFI_PEI_FILE_HANDLE FileHandle,
        IN CONST EFI_PEI_SERVICES    **PeiServices
) {
    // https://edk2-docs.gitbook.io/edk-ii-module-writer-s-guide/7_pre-efi_initialization_modules/76_communicate_with_dxe_modules
    hob = BuildGuidHob(&gTestHobGuid, 24);
    if (!hob)
        return EFI_OUT_OF_RESOURCES;
    end = hob + 24;
    place_EOHOB(hob + 19);

    *((UINT8*)hob++) = 'x';
    *((UINT8*)hob++) = 3;

    LocateOrNotify(&gAmiTreePpiGuid);
    LocateOrNotify(&gTrEE_HashLogExtendPpiGuid);
    LocateOrNotify(&gAmiPeiTcgPpiGuid);

    return EFI_SUCCESS;
}
