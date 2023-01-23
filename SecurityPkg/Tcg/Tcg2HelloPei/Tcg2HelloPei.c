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

#if 0
struct hash_info {
    UINT16    HashAlgoId;
    UINT16    HashSize;
    UINT8     Hash[];
};

struct prehashed_fv {
  UINT32    FvBase;
  UINT32    FvLength;
  UINT32    Count;
  HASH_INFO HashInfo[];
};

struct hash_info hashinfos[] = {
    { .HashAlgoId = 0x4 /* TPM_ALG_SHA1   */, .HashSize = 20, .Hash = (UINT8*) { 0xF4, 0xC0, 0xA7, 0xAA, 0x2A, 0xCE, 0x11, 0xBA, 0xB8, 0x0C, 0x4E, 0x55, 0x5F, 0x9D, 0x63, 0x38, 0x61, 0xED, 0xB4, 0xCE, } },
    { .HashAlgoId = 0xB /* TPM_ALG_SHA256 */, .HashSize = 32, .Hash = (UINT8*) { 0x87, 0xE7, 0x18, 0xC7, 0x7F, 0xCE, 0xBA, 0x35, 0x6B, 0x89, 0x67, 0x54, 0xBE, 0x4E, 0x03, 0x9F, 0xDE, 0xC9, 0x59, 0xF0, 0x4B, 0x5B, 0x27, 0xD7, 0x12, 0x59, 0xF6, 0x7C, 0x5B, 0x8A, 0xB7, 0x45, } },
    { .HashAlgoId = 0xC /* TPM_ALG_SHA384 */, .HashSize = 48, .Hash = (UINT8*) { 0x10, 0xEF, 0x98, 0x56, 0x1E, 0x3A, 0xD0, 0xCB, 0xF3, 0xFB, 0x7A, 0x84, 0xD0, 0x1F, 0xBA, 0x54, 0xBD, 0x66, 0x23, 0xD0, 0xC4, 0x40, 0xAB, 0xD2, 0x65, 0x53, 0x75, 0x82, 0xEF, 0xE1, 0xCF, 0x5F, 0xA2, 0x54, 0xAC, 0x11, 0x79, 0xCC, 0x99, 0x0B, 0xC6, 0x75, 0xAA, 0xDA, 0xF9, 0xA9, 0x5C, 0x29, } },
    { .HashAlgoId = 0xD /* TPM_ALG_SHA512 */, .HashSize = 64, .Hash = (UINT8*) { 0x2E, 0xBB, 0x56, 0xB8, 0x27, 0x72, 0x3D, 0xF2, 0x21, 0x5D, 0x7E, 0x65, 0xE0, 0x75, 0x4D, 0x96, 0xB4, 0x91, 0xA6, 0xAE, 0xF9, 0xFF, 0xBC, 0x45, 0x65, 0x91, 0x6B, 0x42, 0xD5, 0x89, 0xDE, 0xE8, 0x1A, 0xD4, 0x12, 0xFB, 0x98, 0x3F, 0xCB, 0x6B, 0xB2, 0x38, 0xD1, 0x84, 0x6D, 0x38, 0x38, 0xC6, 0xB5, 0xF1, 0x74, 0xCD, 0x14, 0xAA, 0xE3, 0xA3, 0xAE, 0xD2, 0xAD, 0xEC, 0xEB, 0x52, 0x1C, 0x98, } },
};

struct prehashed_fv gEdkiiPrehashedFv = {
    0x820000,
    0xE0000,
    4,
    (HASH_INFO*) hashinfos,
};
#endif

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
