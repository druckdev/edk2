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

EFI_STATUS
EFIAPI
TPMHelloEntryPoint(
        IN       EFI_PEI_FILE_HANDLE FileHandle,
        IN CONST EFI_PEI_SERVICES    **PeiServices
) {
    EFI_STATUS Status;
    EDKII_PEI_FIRMWARE_VOLUME_INFO_PREHASHED_FV_PPI* mPrehashedPeiFv;
    EDKII_PEI_FIRMWARE_VOLUME_INFO_PREHASHED_FV_PPI* mPrehashedDxeFv;
    EFI_PEI_PPI_DESCRIPTOR* gPpiListPrehashedPeiFvPpi;
    EFI_PEI_PPI_DESCRIPTOR* gPpiListPrehashedDxeFvPpi;
    HASH_INFO* mPreHashedSHA1;
    HASH_INFO* mPreHashedSHA256;
    HASH_INFO* mPreHashedSHA384;
    HASH_INFO* mPreHashedSHA512;
    UINTN offset;


    Status = EFI_SUCCESS;

    mPreHashedSHA1   = AllocatePool(sizeof(*mPreHashedSHA1)   + SHA1_DIGEST_SIZE);
    mPreHashedSHA256 = AllocatePool(sizeof(*mPreHashedSHA256) + SHA256_DIGEST_SIZE);
    mPreHashedSHA384 = AllocatePool(sizeof(*mPreHashedSHA384) + SHA384_DIGEST_SIZE);
    mPreHashedSHA512 = AllocatePool(sizeof(*mPreHashedSHA512) + SHA512_DIGEST_SIZE);

    mPreHashedSHA1->HashAlgoId   = TPM_ALG_SHA1;
    mPreHashedSHA256->HashAlgoId = TPM_ALG_SHA256;
    mPreHashedSHA384->HashAlgoId = TPM_ALG_SHA384;
    mPreHashedSHA512->HashAlgoId = TPM_ALG_SHA512;

    mPreHashedSHA1->HashSize   = SHA1_DIGEST_SIZE;
    mPreHashedSHA256->HashSize = SHA256_DIGEST_SIZE;
    mPreHashedSHA384->HashSize = SHA384_DIGEST_SIZE;
    mPreHashedSHA512->HashSize = SHA512_DIGEST_SIZE;

    CopyMem(mPreHashedSHA1   + 1, (UINT8[]) { 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF}, mPreHashedSHA1->HashSize);
    CopyMem(mPreHashedSHA256 + 1, (UINT8[]) { 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF}, mPreHashedSHA256->HashSize);
    CopyMem(mPreHashedSHA384 + 1, (UINT8[]) { 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF}, mPreHashedSHA384->HashSize);
    CopyMem(mPreHashedSHA512 + 1, (UINT8[]) { 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF, 0xBE, 0xEF }, mPreHashedSHA512->HashSize);

    UINTN size = sizeof(*mPrehashedPeiFv) + 4 * sizeof(HASH_INFO) +
      mPreHashedSHA1->HashSize + mPreHashedSHA256->HashSize +
      mPreHashedSHA384->HashSize + mPreHashedSHA512->HashSize;

    mPrehashedPeiFv = AllocatePool(size);
    mPrehashedDxeFv = AllocatePool(size);

    mPrehashedPeiFv->FvBase   = 0x820000;
    mPrehashedPeiFv->FvLength = 0xE0000;
    mPrehashedPeiFv->Count    = 4;

    offset = sizeof(*mPrehashedPeiFv);
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA1,   sizeof(HASH_INFO) + mPreHashedSHA1->HashSize);
    offset += sizeof(HASH_INFO) + mPreHashedSHA1->HashSize;
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA256, sizeof(HASH_INFO) + mPreHashedSHA256->HashSize);
    offset += sizeof(HASH_INFO) + mPreHashedSHA256->HashSize;
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA384, sizeof(HASH_INFO) + mPreHashedSHA384->HashSize);
    offset += sizeof(HASH_INFO) + mPreHashedSHA384->HashSize;
    CopyMem(((void*)mPrehashedPeiFv) + offset, mPreHashedSHA512, sizeof(HASH_INFO) + mPreHashedSHA512->HashSize);

    CopyMem(mPrehashedDxeFv, mPrehashedPeiFv, size);
    mPrehashedDxeFv->FvBase   = 0x900000;
    mPrehashedDxeFv->FvLength = 0xC00000;


    gPpiListPrehashedPeiFvPpi = AllocatePool(sizeof(EFI_PEI_PPI_DESCRIPTOR));
    gPpiListPrehashedDxeFvPpi = AllocatePool(sizeof(EFI_PEI_PPI_DESCRIPTOR));

    gPpiListPrehashedPeiFvPpi->Flags = (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST);
    gPpiListPrehashedPeiFvPpi->Guid = &gEdkiiPeiFirmwareVolumeInfoPrehashedFvPpiGuid;
    gPpiListPrehashedPeiFvPpi->Ppi = mPrehashedPeiFv;

    CopyMem(gPpiListPrehashedDxeFvPpi, gPpiListPrehashedPeiFvPpi, sizeof(EFI_PEI_PPI_DESCRIPTOR));
    gPpiListPrehashedDxeFvPpi->Ppi = mPrehashedDxeFv;

    PeiServicesInstallPpi (gPpiListPrehashedPeiFvPpi);
    PeiServicesInstallPpi (gPpiListPrehashedDxeFvPpi);

    DEBUG ((DEBUG_INFO, "Hello World!\n"));

    return Status;
}
