// vim: set et:

#include <IndustryStandard/Tpm20.h>
#include <IndustryStandard/UefiTcgPlatform.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>

extern EFI_GUID gAmiTreePpiGuid;
extern EFI_GUID gAmiHashLogPpiGuid;

#pragma pack (1)

struct AmiHashLogExtendPpi {
    EFI_STATUS (*_HashLogExtendEvent)(const EFI_PEI_SERVICES**, void*, UINTN, UINTN, UINTN, UINTN, void*, void*);
};

struct AmiHashLogEvent {
    UINT32 PCRIndex;
    UINT32 EventType;
    UINT32 NumAlgos;
    TPMT_HA Digests[HASH_COUNT];
    UINT32 EventSize;
};

#pragma pack ()

EFI_STATUS
EFIAPI
EntryPoint(IN EFI_PEI_FILE_HANDLE FileHandle,
           IN CONST EFI_PEI_SERVICES **PeiServices)
{
    struct AmiHashLogExtendPpi* HashLogPpi;
    void* TreePpi;

    if (!PeiServicesLocatePpi(&gAmiHashLogPpiGuid, 0, NULL, (VOID**)&HashLogPpi)
        && !PeiServicesLocatePpi(&gAmiTreePpiGuid, 0, NULL, &TreePpi)) {

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

        (*(HashLogPpi->_HashLogExtendEvent))(PeiServices, TreePpi, 0, 0, 0, 0, &event, &extra);
    }

    return EFI_SUCCESS;
}
