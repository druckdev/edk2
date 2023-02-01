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

#define INCLUDE_SHA1 1
#define INCLUDE_SHA256 1
#define INCLUDE_SHA384 1
#define INCLUDE_SHA512 1

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
    UINTN offset;

    Status = EFI_SUCCESS;
    UINTN size = sizeof(*mPrehashedPeiFv);

#if INCLUDE_SHA1
    HASH_INFO* mPreHashedSHA1;
    mPreHashedSHA1 = AllocatePool(sizeof(*mPreHashedSHA1) + SHA1_DIGEST_SIZE);
    mPreHashedSHA1->HashAlgoId = TPM_ALG_SHA1;
    mPreHashedSHA1->HashSize = SHA1_DIGEST_SIZE;
    CopyMem(mPreHashedSHA1 + 1, "\x13\x3a\xb2\x7f\x81\xcd\xac\x1d\x3e\xa0\x2f\x08\x20\x02\x55\x17\x62\x17\x51\x78", mPreHashedSHA1->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA1->HashSize;
#endif
#if INCLUDE_SHA256
    HASH_INFO* mPreHashedSHA256;
    mPreHashedSHA256 = AllocatePool(sizeof(*mPreHashedSHA256) + SHA256_DIGEST_SIZE);
    mPreHashedSHA256->HashAlgoId = TPM_ALG_SHA256;
    mPreHashedSHA256->HashSize = SHA256_DIGEST_SIZE;
    CopyMem(mPreHashedSHA256 + 1, "\x0a\xfc\x02\x26\x1d\x0e\x1d\x96\x7e\xc3\x53\x9e\x7a\x73\x00\xc2\xff\xcf\xea\xc8\x22\x61\x6a\xfd\xc3\x75\xdd\xda\x41\x9a\x50\xee", mPreHashedSHA256->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA256->HashSize;
#endif
#if INCLUDE_SHA384
    HASH_INFO* mPreHashedSHA384;
    mPreHashedSHA384 = AllocatePool(sizeof(*mPreHashedSHA384) + SHA384_DIGEST_SIZE);
    mPreHashedSHA384->HashAlgoId = TPM_ALG_SHA384;
    mPreHashedSHA384->HashSize = SHA384_DIGEST_SIZE;
    CopyMem(mPreHashedSHA384 + 1, "\xcc\x5d\xbe\xc9\x86\x19\xd8\x06\x55\xe6\x9f\xaf\x49\x47\x36\x91\xc7\x5c\x18\x42\x7b\x78\xfd\x5d\x95\xf8\xbd\x75\x44\xf9\x8a\xcd\x03\x7d\xcd\xc0\x91\xb1\xc8\xce\x3a\xbe\x6e\xd5\x25\x7e\x44\x2f", mPreHashedSHA384->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA384->HashSize;
#endif
#if INCLUDE_SHA512
    HASH_INFO* mPreHashedSHA512;
    mPreHashedSHA512 = AllocatePool(sizeof(*mPreHashedSHA512) + SHA512_DIGEST_SIZE);
    mPreHashedSHA512->HashAlgoId = TPM_ALG_SHA512;
    mPreHashedSHA512->HashSize = SHA512_DIGEST_SIZE;
    CopyMem(mPreHashedSHA512 + 1, "\xbf\xb1\xf9\x3e\xfe\xb7\x6c\x4b\x4f\xe9\x10\x3a\xe9\xc2\x99\x93\xe3\x33\xdc\x51\x79\x90\x30\x9a\xe4\x24\xc6\xab\x62\xeb\xfe\x4a\x5f\xab\x9d\x69\x90\x24\xbf\xf9\xc0\x3e\xfb\x07\xf3\x87\x3e\x49\x24\x63\xf9\x27\x5b\xf9\x0d\x0a\x47\xef\xe4\xe3\xbe\x7d\x95\xaa", mPreHashedSHA512->HashSize);

    size += sizeof(HASH_INFO) + mPreHashedSHA512->HashSize;
#endif

    mPrehashedPeiFv = AllocatePool(size);
    mPrehashedDxeFv = AllocatePool(size);

    mPrehashedPeiFv->FvBase   = 0x820000;
    mPrehashedPeiFv->FvLength = 0xE0000;
    mPrehashedPeiFv->Count    = 4;

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

    CopyMem(mPrehashedDxeFv, mPrehashedPeiFv, size);

    offset = sizeof(*mPrehashedDxeFv) + sizeof(HASH_INFO); /* keep AlgoId & HashSize */
#if INCLUDE_SHA1
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\x7f\xba\xcd\x82\xa3\xfa\xec\xf8\x3b\x01\xa1\x5f\xf4\xcb\xfd\xed\x61\x5a\xaf\xad" , mPreHashedSHA1->HashSize);
    offset += mPreHashedSHA1->HashSize + sizeof(HASH_INFO);
#endif
#if INCLUDE_SHA256
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\x49\x91\xde\xbd\xa2\xdd\x59\x2f\x0b\x8a\xe2\x14\x99\xa2\x0c\xe5\x87\x42\x52\x0f\xac\xdb\x64\xd9\x45\x86\x72\xc1\xe2\x2d\xfb\x50" , mPreHashedSHA256->HashSize);
    offset += mPreHashedSHA256->HashSize + sizeof(HASH_INFO);
#endif
#if INCLUDE_SHA384
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\x10\x01\x17\x7c\xfd\x8a\xc7\x73\x49\x4a\xb4\x1e\xed\x3b\x8b\x93\x72\x83\x74\x72\x3e\xec\xa8\xfd\x8d\x18\x13\x0b\x35\x07\x15\xe0\x1d\x17\xfa\x20\xe4\x71\x2c\xb3\x49\x62\xde\x14\xd3\xeb\x6e\x00" , mPreHashedSHA384->HashSize);
    offset += mPreHashedSHA384->HashSize + sizeof(HASH_INFO);
#endif
#if INCLUDE_SHA512
    CopyMem(((void*)mPrehashedDxeFv) + offset, "\x7e\x33\x0c\xae\xb6\x8d\xb4\x24\xce\x69\xf2\x8e\x88\x19\x5d\x55\xe0\x0e\x67\xa3\xf3\x9e\x8f\xba\x02\x65\x76\x8e\x4c\x6a\xfb\x0b\xd8\x3d\xb5\x89\x95\x81\x11\xa7\x43\x1b\x9a\xba\xfe\x37\x61\x47\x94\xa4\xae\x85\xaf\x65\x97\x32\x60\x80\x90\x6f\xa3\xd9\xef\x0c" , mPreHashedSHA512->HashSize);
#endif

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
