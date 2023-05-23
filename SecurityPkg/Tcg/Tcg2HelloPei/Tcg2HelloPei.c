// vim: set et:

#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>


extern EFI_GUID gTestHobGuid;
extern EFI_GUID gAmiTreePpiGuid;
extern EFI_GUID gTrEE_HashLogExtendPpiGuid;
extern EFI_GUID gPeiTcgPpiGuid;
extern EFI_GUID gPeiTpmPpiGuid;
extern EFI_GUID gAmiPlatformSecurityChipGuid;
extern EFI_GUID gTcgTestPpiGuid;
extern EFI_GUID gTcgPeiPolicyGuid;
extern EFI_GUID gTCMPEIPpiGuid;

EFI_STATUS EFIAPI PpiNotifyCallback(IN EFI_PEI_SERVICES **PeiServices, IN EFI_PEI_NOTIFY_DESCRIPTOR *NotifyDescriptor, IN VOID *Ppi);

EFI_PEI_NOTIFY_DESCRIPTOR AmiTreePpiDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gAmiTreePpiGuid,
    PpiNotifyCallback
};
EFI_PEI_NOTIFY_DESCRIPTOR TrEE_HashLogExtendPpiDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gTrEE_HashLogExtendPpiGuid,
    PpiNotifyCallback
};
EFI_PEI_NOTIFY_DESCRIPTOR PeiTcgPpiDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gPeiTcgPpiGuid,
    PpiNotifyCallback
};
EFI_PEI_NOTIFY_DESCRIPTOR PeiTpmPpiDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gPeiTpmPpiGuid,
    PpiNotifyCallback
};
EFI_PEI_NOTIFY_DESCRIPTOR AmiPlatformSecurityChipDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gAmiPlatformSecurityChipGuid,
    PpiNotifyCallback
};
EFI_PEI_NOTIFY_DESCRIPTOR TcgPeiPolicyDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gTcgPeiPolicyGuid,
    PpiNotifyCallback
};
EFI_PEI_NOTIFY_DESCRIPTOR TCMPEIPpiDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gTCMPEIPpiGuid,
    PpiNotifyCallback
};


void* hob = NULL;
void* end = NULL;


EFI_STATUS
EFIAPI
PpiNotifyCallback(IN EFI_PEI_SERVICES **PeiServices,
                  IN EFI_PEI_NOTIFY_DESCRIPTOR *NotifyDescriptor,
                  IN VOID *Ppi)
{
    if (hob + 2 < end) {
        *((UINT8*)hob++) = 'S';
        *((UINT8*)hob++) = 1;
        *((UINT8*)hob++) = EFI_SUCCESS & 0xFF;
    }
    if (hob + 9 < end) {
        *((UINT8*)hob++) = 'x';
        *((UINT8*)hob++) = 2;
        *((UINT32*)hob) = NotifyDescriptor->Guid->Data1;
        hob += 4;
        *((UINT32*)hob) = 0xFFFFFFFF & (UINTN)Ppi;
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
LocateOrNotify(EFI_PEI_NOTIFY_DESCRIPTOR* notify)
{
    EFI_STATUS Status;
    void* ppi = NULL;

    Status = PeiServicesLocatePpi(notify->Guid, 0, NULL, &ppi);
    if (EFI_SUCCESS == Status) {
        *((UINT8*)hob++) = 'S';
        *((UINT8*)hob++) = 1;
        *((UINT8*)hob++) = EFI_SUCCESS & 0xFF;
        *((UINT8*)hob++) = 'x';
        *((UINT8*)hob++) = 2;
        *((UINT32*)hob) = notify->Guid->Data1;
        hob += 4;
        *((UINT32*)hob) = 0xFFFFFFFF & (UINTN)ppi;
        hob += 4;
    } else {
        if (EFI_SUCCESS != (Status = PeiServicesNotifyPpi(notify))) {
            *((UINT8*)hob++) = 'S';
            *((UINT8*)hob++) = 1;
            *((UINT8*)hob++) = Status & 0xFF;
        }
    }

    return Status;
}


EFI_STATUS
EFIAPI
TPMHelloEntryPoint(IN EFI_PEI_FILE_HANDLE FileHandle,
                   IN CONST EFI_PEI_SERVICES **PeiServices)
{
    // https://edk2-docs.gitbook.io/edk-ii-module-writer-s-guide/7_pre-efi_initialization_modules/76_communicate_with_dxe_modules
    UINTN len = 128;
    hob = BuildGuidHob(&gTestHobGuid, len);
    if (!hob) {
        if ((hob = BuildGuidHob(&gTestHobGuid, 5)))
            place_EOHOB(hob);
        return EFI_OUT_OF_RESOURCES;
    }
    end = hob + len;
    place_EOHOB(end - 5);

    LocateOrNotify(&AmiTreePpiDesc);
    LocateOrNotify(&TrEE_HashLogExtendPpiDesc);
    LocateOrNotify(&PeiTcgPpiDesc);
    LocateOrNotify(&PeiTpmPpiDesc);
    LocateOrNotify(&AmiPlatformSecurityChipDesc);
    LocateOrNotify(&TcgPeiPolicyDesc);
    LocateOrNotify(&TCMPEIPpiDesc);

    return EFI_SUCCESS;
}
