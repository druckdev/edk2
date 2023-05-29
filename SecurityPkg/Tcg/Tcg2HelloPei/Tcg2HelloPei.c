// vim: set et:

#include <IndustryStandard/Tpm20.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>


// before EOHOB
#define InstallCount ((UINT32*)(end - 5 - 4))
#define TreeInstallCount ((UINT32*)(end - 5 - 8))


extern EFI_GUID gTestHobGuid;
extern EFI_GUID gAmiTreePpiGuid;
extern EFI_GUID gTrEE_HashLogExtendPpiGuid;
extern EFI_GUID gTcgTestPpiGuid;
extern EFI_GUID gUnmeasuredRockPpiGuid;

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
EFI_PEI_NOTIFY_DESCRIPTOR TcgTestPpiDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gTcgTestPpiGuid,
    PpiNotifyCallback
};
EFI_PEI_NOTIFY_DESCRIPTOR UnmeasuredRockNotifyDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gUnmeasuredRockPpiGuid,
    PpiNotifyCallback
};

EFI_PEI_PPI_DESCRIPTOR UmeasuredRockPpiDesc = {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gUnmeasuredRockPpiGuid,
    &gUnmeasuredRockPpiGuid
};

#pragma pack (1)

struct AmiTreePpi {
    void * get_capability_maybe;
    void * get_eventlog_maybe;
    EFI_STATUS (*hashlog_extend_maybe)(void*, UINTN, UINTN, void*, UINTN, UINTN, UINTN, void*);
    void * submit_command_maybe;
};

struct AmiHashLogExtendPpi {
    EFI_STATUS (*AmiHashLogExtend)(const EFI_PEI_SERVICES**, void*, UINTN, UINTN, UINTN, UINTN, void*, void*);
};

typedef enum EFI_TCG_EVENT_TYPES {
    EV_PREBOOT_CERT=0,
    EV_POST_CODE=1,
    EV_NO_ACTION=3,
    EV_SEPARATOR=4,
    EV_ACTION=5,
    EV_EVENT_TAG=6,
    EV_S_CRTM_CONTENTS=7,
    EV_S_CRTM_VERSION=8,
    EV_CPU_MICROCODE=9,
    EV_PLATFORM_CONFIG_FLAGS=10,
    EV_TABLE_OF_DEVICES=11,
    EV_COMPACT_HASH=12,
    EV_NONHOST_CODE=15,
    EV_NONHOST_CONFIG=16,
    EV_NONHOST_INFO=17,
    EV_OMIT_BOOT_DEVICE_EVENTS=18,
    EV_EFI_EVENT_BASE=2147483648,
    EV_EFI_VARIABLE_DRIVER_CONFIG=2147483649,
    EV_EFI_VARIABLE_BOOT=2147483650,
    EV_EFI_BOOT_SERVICES_APPLICATION=2147483651,
    EV_EFI_BOOT_SERVICES_DRIVER=2147483652,
    EV_EFI_GPT_EVENT=2147483654,
    EV_EFI_ACTION=2147483655,
    EV_EFI_HANDOFF_TABLES=2147483656,
    EV_EFI_PLATFORM_FIRMWARE_BLOB=2147483657,
    EV_EFI_PLATFORM_FIRMWARE_BLOB2=2147483658,
    EV_EFI_HANDOFF_TABLES2=2147483659,
    EV_EFI_HCRTM_EVENT=2147483664,
    EV_EFI_VARIABLE_AUTHORITY=2147483872,
    EV_EFI_SPDM_FIRMWARE_BLOB=2147483873,
    EV_EFI_SPDM_FIRMWARE_CONFIG=2147483874
} EFI_TCG_EVENT_TYPES;

struct TrEE_EVENT_HEADER {
    UINT32 HeaderSize;
    UINT16 HeaderVersion;
    UINT32 PCRIndex;
    enum EFI_TCG_EVENT_TYPES EventType;
};

struct TrEE_EVENT {
    UINT32 Size;
    struct TrEE_EVENT_HEADER Header;
    UINT32 Event[4];
};

struct AmiHashLogEvent {
    UINT32 PCRIndex;
    UINT32 EventType;
    UINT32 NumAlgos;
    TPMT_HA Digests[HASH_COUNT];
    UINT32 EventSize;
};

#pragma pack ()

struct TrEE_EVENT event = {
    0x22,
    {
        0xE,
        1,
        0,
        EV_S_CRTM_VERSION
    },
    { 0xcafebaeb, 0xcafebaeb, 0xcafebaeb, 0xcafebaeb }
};

EFI_PEI_INSTALL_PPI RealInstallPpi;

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
LocateOrNotify(EFI_PEI_NOTIFY_DESCRIPTOR* notify, void** ppi)
{
    EFI_STATUS Status;

    void* throwaway_ppi;
    ppi = ppi ? ppi : &throwaway_ppi;

    Status = PeiServicesLocatePpi(notify->Guid, 0, NULL, ppi);
    if (EFI_SUCCESS == Status) {
        *((UINT8*)hob++) = 'S';
        *((UINT8*)hob++) = 1;
        *((UINT8*)hob++) = EFI_SUCCESS & 0xFF;
        *((UINT8*)hob++) = 'x';
        *((UINT8*)hob++) = 2;
        *((UINT32*)hob) = notify->Guid->Data1;
        hob += 4;
        *((UINT32*)hob) = 0xFFFFFFFF & (UINTN)(*ppi);
        hob += 4;
    } else {
        *((UINT8*)hob++) = 'S';
        *((UINT8*)hob++) = 1;
        *((UINT8*)hob++) = Status & 0xFF;

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
HookedInstallPpi(IN CONST EFI_PEI_SERVICES** PeiServices,
                 IN CONST EFI_PEI_PPI_DESCRIPTOR *PpiList)
{
    (*InstallCount)++;

    if (PpiList && PpiList->Guid && PpiList->Guid->Data1 == gAmiTreePpiGuid.Data1)
        (*TreeInstallCount)++;

    return RealInstallPpi(PeiServices, PpiList);
}

EFI_STATUS
EFIAPI
TPMHelloEntryPoint(IN EFI_PEI_FILE_HANDLE FileHandle,
                   IN CONST EFI_PEI_SERVICES **PeiServices)
{
    // https://edk2-docs.gitbook.io/edk-ii-module-writer-s-guide/7_pre-efi_initialization_modules/76_communicate_with_dxe_modules
    UINTN len = 64;
    hob = BuildGuidHob(&gTestHobGuid, len);
    if (!hob) {
        if ((hob = BuildGuidHob(&gTestHobGuid, 5)))
            place_EOHOB(hob);
        return EFI_OUT_OF_RESOURCES;
    }
    end = hob + len;
    place_EOHOB(end - 5);

    struct AmiHashLogExtendPpi* HashLogPpi;
    struct AmiTreePpi* TreePpi;

    if (EFI_SUCCESS == LocateOrNotify(&TrEE_HashLogExtendPpiDesc, (VOID**)&HashLogPpi)
            && EFI_SUCCESS == LocateOrNotify(&AmiTreePpiDesc, (VOID**)&TreePpi)) {

        UINT32 extra[4] = {0xFF92F000, 0, 0x4D1000, 0};
        struct AmiHashLogEvent event = {
            .PCRIndex = 0,
            .EventType = EV_POST_CODE,
            .NumAlgos = 1,
            .Digests = { {
                .hashAlg = TPM_ALG_SHA256,
                .digest = {
                    .sha256 = "\x39\xF8\x02\x98\xF0\xFB\x3C\xB0\x7B\x42\x61\xA4\xD2\xF0\x24\xAE\x51\xFE\x0A\xCC\x68\x8B\x49\x15\xAD\x71\xD0\xB8\xCE\x5B\x2D\x37"
                }
            }, {0}, {0}, {0}, {0} },
            .EventSize = 0x10
        };

        *((UINT8*)hob++) = 'S';
        *((UINT8*)hob++) = 1;
        *((UINT8*)hob++) = 0xFF & \
                           (*(HashLogPpi->AmiHashLogExtend))(PeiServices, TreePpi, 0, 0, 0, 0, &event, &extra);
    }

    return EFI_SUCCESS;
}
