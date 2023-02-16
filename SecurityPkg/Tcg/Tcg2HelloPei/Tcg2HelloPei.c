/** @file
  Initialize TPM2 device and measure FVs before handing off control to DXE.

Copyright (c) 2015 - 2021, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2017, Microsoft Corporation.  All rights reserved. <BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <IndustryStandard/UefiTcgPlatform.h>
#include <Ppi/FirmwareVolumeInfo.h>
#include <Ppi/FirmwareVolumeInfo2.h>
#include <Ppi/TpmInitialized.h>
#include <Ppi/FirmwareVolume.h>
#include <Ppi/EndOfPeiPhase.h>
#include <Ppi/FirmwareVolumeInfoMeasurementExcluded.h>
#include <Ppi/FirmwareVolumeInfoPrehashedFV.h>
#include <Ppi/Tcg.h>

#include <Guid/TcgEventHob.h>
#include <Guid/MeasuredFvHob.h>
#include <Guid/TpmInstance.h>
#include <Guid/MigratedFvInfo.h>

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/HashLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesTablePointerLib.h>
#include <Protocol/Tcg2Protocol.h>
#include <Library/PerformanceLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/PrintLib.h>

#define INCLUDE_DXE_FV 1

#define INCLUDE_SHA1 1
#define INCLUDE_SHA256 1
#define INCLUDE_SHA384 1
#define INCLUDE_SHA512 1

#define PEI_FV_BASE   0x820000;
#define PEI_FV_LENGTH 0xE0000;

#define DXE_FV_BASE   0x900000;
#define DXE_FV_LENGTH 0xC00000;

EFI_STATUS
EFIAPI
TPMHelloEntryPoint(
        IN       EFI_PEI_FILE_HANDLE FileHandle,
        IN CONST EFI_PEI_SERVICES    **PeiServices
) {
    EFI_STATUS Status;
    UINTN offset;
    EDKII_PEI_FIRMWARE_VOLUME_INFO_PREHASHED_FV_PPI* mPrehashedPeiFv;
    EFI_PEI_PPI_DESCRIPTOR* gPpiListPrehashedPeiFvPpi;

#if INCLUDE_DXE_FV
    EDKII_PEI_FIRMWARE_VOLUME_INFO_PREHASHED_FV_PPI* mPrehashedDxeFv;
    EFI_PEI_PPI_DESCRIPTOR* gPpiListPrehashedDxeFvPpi;
#endif

    Status = EFI_SUCCESS;
    UINTN size = sizeof(*mPrehashedPeiFv);

#if INCLUDE_SHA1
    HASH_INFO* mPreHashedSHA1;
    mPreHashedSHA1 = AllocatePool(sizeof(*mPreHashedSHA1) + SHA1_DIGEST_SIZE);
    mPreHashedSHA1->HashAlgoId = TPM_ALG_SHA1;
    mPreHashedSHA1->HashSize = SHA1_DIGEST_SIZE;
    CopyMem(mPreHashedSHA1 + 1, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF", mPreHashedSHA1->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA1->HashSize;
#endif
#if INCLUDE_SHA256
    HASH_INFO* mPreHashedSHA256;
    mPreHashedSHA256 = AllocatePool(sizeof(*mPreHashedSHA256) + SHA256_DIGEST_SIZE);
    mPreHashedSHA256->HashAlgoId = TPM_ALG_SHA256;
    mPreHashedSHA256->HashSize = SHA256_DIGEST_SIZE;
    CopyMem(mPreHashedSHA256 + 1, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF", mPreHashedSHA256->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA256->HashSize;
#endif
#if INCLUDE_SHA384
    HASH_INFO* mPreHashedSHA384;
    mPreHashedSHA384 = AllocatePool(sizeof(*mPreHashedSHA384) + SHA384_DIGEST_SIZE);
    mPreHashedSHA384->HashAlgoId = TPM_ALG_SHA384;
    mPreHashedSHA384->HashSize = SHA384_DIGEST_SIZE;
    CopyMem(mPreHashedSHA384 + 1, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF", mPreHashedSHA384->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA384->HashSize;
#endif
#if INCLUDE_SHA512
    HASH_INFO* mPreHashedSHA512;
    mPreHashedSHA512 = AllocatePool(sizeof(*mPreHashedSHA512) + SHA512_DIGEST_SIZE);
    mPreHashedSHA512->HashAlgoId = TPM_ALG_SHA512;
    mPreHashedSHA512->HashSize = SHA512_DIGEST_SIZE;
    CopyMem(mPreHashedSHA512 + 1, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF", mPreHashedSHA512->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA512->HashSize;
#endif

    mPrehashedPeiFv = AllocatePool(size);
    mPrehashedPeiFv->FvBase   = PEI_FV_BASE;
    mPrehashedPeiFv->FvLength = PEI_FV_LENGTH;
    mPrehashedPeiFv->Count    = INCLUDE_SHA1 + INCLUDE_SHA256 + INCLUDE_SHA256 +
                                INCLUDE_SHA512;

    offset = sizeof(*mPrehashedPeiFv);
#if INCLUDE_SHA1
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA1, sizeof(HASH_INFO) + mPreHashedSHA1->HashSize);
    offset += sizeof(HASH_INFO) + mPreHashedSHA1->HashSize;
#endif
#if INCLUDE_SHA256
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA256, sizeof(HASH_INFO) + mPreHashedSHA256->HashSize);
    offset += sizeof(HASH_INFO) + mPreHashedSHA256->HashSize;
#endif
#if INCLUDE_SHA384
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA384, sizeof(HASH_INFO) + mPreHashedSHA384->HashSize);
    offset += sizeof(HASH_INFO) + mPreHashedSHA384->HashSize;
#endif
#if INCLUDE_SHA512
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA512, sizeof(HASH_INFO) + mPreHashedSHA512->HashSize);
#endif

    gPpiListPrehashedPeiFvPpi = AllocatePool(sizeof(EFI_PEI_PPI_DESCRIPTOR));
    gPpiListPrehashedPeiFvPpi->Flags = (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST);
    gPpiListPrehashedPeiFvPpi->Guid = &gEdkiiPeiFirmwareVolumeInfoPrehashedFvPpiGuid;
    gPpiListPrehashedPeiFvPpi->Ppi = mPrehashedPeiFv;
    PeiServicesInstallPpi (gPpiListPrehashedPeiFvPpi);


#if INCLUDE_DXE_FV
    mPrehashedDxeFv = AllocatePool(size);
    CopyMem(mPrehashedDxeFv, mPrehashedPeiFv, size);

    offset = sizeof(*mPrehashedDxeFv) + sizeof(HASH_INFO); /* keep AlgoId & HashSize */
#if INCLUDE_SHA1
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF" , mPreHashedSHA1->HashSize);
    offset += mPreHashedSHA1->HashSize + sizeof(HASH_INFO);
#endif
#if INCLUDE_SHA256
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF" , mPreHashedSHA256->HashSize);
    offset += mPreHashedSHA256->HashSize + sizeof(HASH_INFO);
#endif
#if INCLUDE_SHA384
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF" , mPreHashedSHA384->HashSize);
    offset += mPreHashedSHA384->HashSize + sizeof(HASH_INFO);
#endif
#if INCLUDE_SHA512
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF\xCA\xFE\xBE\xEF" , mPreHashedSHA512->HashSize);
#endif

    mPrehashedDxeFv->FvBase   = DXE_FV_BASE;
    mPrehashedDxeFv->FvLength = DXE_FV_LENGTH;

    gPpiListPrehashedDxeFvPpi = AllocatePool(sizeof(EFI_PEI_PPI_DESCRIPTOR));
    CopyMem(gPpiListPrehashedDxeFvPpi, gPpiListPrehashedPeiFvPpi, sizeof(EFI_PEI_PPI_DESCRIPTOR));
    gPpiListPrehashedDxeFvPpi->Ppi = mPrehashedDxeFv;
    PeiServicesInstallPpi (gPpiListPrehashedDxeFvPpi);
#endif

    return Status;
}
