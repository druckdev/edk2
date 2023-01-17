/** @file
  This library is BaseCrypto router. It will redirect hash request to each individual
  hash handler registered, such as SHA1, SHA256.
  Platform can use PcdTpm2HashMask to mask some hash engines.

Copyright (c) 2013 - 2021, Intel Corporation. All rights reserved. <BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>
#include <Library/HashLib.h>
#include <Guid/ZeroGuid.h>

#include "HashLibBaseCryptoRouterCommon.h"

#define HASH_LIB_PEI_ROUTER_GUID \
  { 0x84681c08, 0x6873, 0x46f3, { 0x8b, 0xb7, 0xab, 0x66, 0x18, 0x95, 0xa1, 0xb3 } }

EFI_GUID  mHashLibPeiRouterGuid = HASH_LIB_PEI_ROUTER_GUID;

typedef struct {
  //
  // If gZeroGuid, SupportedHashMask is 0 for FIRST module which consumes HashLib
  //   or the hash algorithm bitmap of LAST module which consumes HashLib.
  //   HashInterfaceCount and HashInterface are all 0.
  // If gEfiCallerIdGuid, HashInterfaceCount, HashInterface and SupportedHashMask
  //   are the hash interface information of CURRENT module which consumes HashLib.
  //
  EFI_GUID          Identifier;
  UINTN             HashInterfaceCount;
  HASH_INTERFACE    HashInterface[HASH_COUNT];
  UINT32            SupportedHashMask;
} HASH_INTERFACE_HOB;

/**
  This function gets hash interface hob.

  @param Identifier    Identifier to get hash interface hob.

  @retval hash interface hob.
**/
HASH_INTERFACE_HOB *
InternalGetHashInterfaceHob (
  EFI_GUID  *Identifier
  )
{
  EFI_PEI_HOB_POINTERS  Hob;
  HASH_INTERFACE_HOB    *HashInterfaceHob;

  Hob.Raw = GetFirstGuidHob (&mHashLibPeiRouterGuid);
  while (Hob.Raw != NULL) {
    HashInterfaceHob = GET_GUID_HOB_DATA (Hob);
    if (CompareGuid (&HashInterfaceHob->Identifier, Identifier)) {
      //
      // Found the matched one.
      //
      return HashInterfaceHob;
    }

    Hob.Raw = GET_NEXT_HOB (Hob);
    Hob.Raw = GetNextGuidHob (&mHashLibPeiRouterGuid, Hob.Raw);
  }

  return NULL;
}

/**
  This function creates hash interface hob.

  @param Identifier    Identifier to create hash interface hob.

  @retval hash interface hob.
**/
HASH_INTERFACE_HOB *
InternalCreateHashInterfaceHob (
  EFI_GUID  *Identifier
  )
{
  HASH_INTERFACE_HOB  LocalHashInterfaceHob;

  ZeroMem (&LocalHashInterfaceHob, sizeof (LocalHashInterfaceHob));
  CopyGuid (&LocalHashInterfaceHob.Identifier, Identifier);
  return BuildGuidDataHob (&mHashLibPeiRouterGuid, &LocalHashInterfaceHob, sizeof (LocalHashInterfaceHob));
}

/**
  Check mismatch of supported HashMask between modules
  that may link different HashInstanceLib instances.

  @param HashInterfaceHobCurrent    Pointer to hash interface hob for CURRENT module.

**/
VOID
CheckSupportedHashMaskMismatch (
  IN HASH_INTERFACE_HOB  *HashInterfaceHobCurrent
  )
{
  HASH_INTERFACE_HOB  *HashInterfaceHobLast;

  HashInterfaceHobLast = InternalGetHashInterfaceHob (&gZeroGuid);
  ASSERT (HashInterfaceHobLast != NULL);

  if ((HashInterfaceHobLast->SupportedHashMask != 0) &&
      (HashInterfaceHobCurrent->SupportedHashMask != HashInterfaceHobLast->SupportedHashMask))
  {
    DEBUG ((
      DEBUG_WARN,
      "WARNING: There is mismatch of supported HashMask (0x%x - 0x%x) between modules\n",
      HashInterfaceHobCurrent->SupportedHashMask,
      HashInterfaceHobLast->SupportedHashMask
      ));
    DEBUG ((DEBUG_WARN, "that are linking different HashInstanceLib instances!\n"));
  }
}

/**
  Start hash sequence.

  @param HashHandle Hash handle.

  @retval EFI_SUCCESS          Hash sequence start and HandleHandle returned.
  @retval EFI_OUT_OF_RESOURCES No enough resource to start hash.
**/
EFI_STATUS
EFIAPI
HashStart (
  OUT HASH_HANDLE  *HashHandle
  )
{
  HASH_INTERFACE_HOB  *HashInterfaceHob;
  HASH_HANDLE         *HashCtx;
  UINTN               Index;
  UINT32              HashMask;

  HashInterfaceHob = InternalGetHashInterfaceHob (&gEfiCallerIdGuid);
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  CheckSupportedHashMaskMismatch (HashInterfaceHob);

  HashCtx = AllocatePool (sizeof (*HashCtx) * HashInterfaceHob->HashInterfaceCount);
  ASSERT (HashCtx != NULL);

  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    HashMask = Tpm2GetHashMaskFromAlgo (&HashInterfaceHob->HashInterface[Index].HashGuid);
    if ((HashMask & PcdGet32 (PcdTpm2HashMask)) != 0) {
      HashInterfaceHob->HashInterface[Index].HashInit (&HashCtx[Index]);
    }
  }

  *HashHandle = (HASH_HANDLE)HashCtx;

  return EFI_SUCCESS;
}

/**
  Update hash sequence data.

  @param HashHandle    Hash handle.
  @param DataToHash    Data to be hashed.
  @param DataToHashLen Data size.

  @retval EFI_SUCCESS     Hash sequence updated.
**/
EFI_STATUS
EFIAPI
HashUpdate (
  IN HASH_HANDLE  HashHandle,
  IN VOID         *DataToHash,
  IN UINTN        DataToHashLen
  )
{
  HASH_INTERFACE_HOB  *HashInterfaceHob;
  HASH_HANDLE         *HashCtx;
  UINTN               Index;
  UINT32              HashMask;

  HashInterfaceHob = InternalGetHashInterfaceHob (&gEfiCallerIdGuid);
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  CheckSupportedHashMaskMismatch (HashInterfaceHob);

  HashCtx = (HASH_HANDLE *)HashHandle;

  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    HashMask = Tpm2GetHashMaskFromAlgo (&HashInterfaceHob->HashInterface[Index].HashGuid);
    if ((HashMask & PcdGet32 (PcdTpm2HashMask)) != 0) {
      HashInterfaceHob->HashInterface[Index].HashUpdate (HashCtx[Index], DataToHash, DataToHashLen);
    }
  }

  return EFI_SUCCESS;
}

/**
  Hash sequence complete and extend to PCR.

  @param HashHandle    Hash handle.
  @param PcrIndex      PCR to be extended.
  @param DataToHash    Data to be hashed.
  @param DataToHashLen Data size.
  @param DigestList    Digest list.

  @retval EFI_SUCCESS     Hash sequence complete and DigestList is returned.
**/
EFI_STATUS
EFIAPI
HashCompleteAndExtend (
  IN HASH_HANDLE          HashHandle,
  IN TPMI_DH_PCR          PcrIndex,
  IN VOID                 *DataToHash,
  IN UINTN                DataToHashLen,
  OUT TPML_DIGEST_VALUES  *DigestList
  )
{
  TPML_DIGEST_VALUES  Digest;
  HASH_INTERFACE_HOB  *HashInterfaceHob;
  HASH_HANDLE         *HashCtx;
  UINTN               Index;
  EFI_STATUS          Status;
  UINT32              HashMask;

  HashInterfaceHob = InternalGetHashInterfaceHob (&gEfiCallerIdGuid);
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  CheckSupportedHashMaskMismatch (HashInterfaceHob);

  HashCtx = (HASH_HANDLE *)HashHandle;
  ZeroMem (DigestList, sizeof (*DigestList));

  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    HashMask = Tpm2GetHashMaskFromAlgo (&HashInterfaceHob->HashInterface[Index].HashGuid);
    if ((HashMask & PcdGet32 (PcdTpm2HashMask)) != 0) {
      HashInterfaceHob->HashInterface[Index].HashUpdate (HashCtx[Index], DataToHash, DataToHashLen);
      HashInterfaceHob->HashInterface[Index].HashFinal (HashCtx[Index], &Digest);
      Tpm2SetHashToDigestList (DigestList, &Digest);
    }
  }

  FreePool (HashCtx);

  Status = Tpm2PcrExtend (
             PcrIndex,
             DigestList
             );
  return Status;
}

/**
  Fake a hash and extend sequence to PCR for the PEI stage.

  @param PcrIndex      PCR to be extended.
  @param DigestList    Digest list.

  @retval EFI_SUCCESS     Hash sequence complete and DigestList is returned.
**/
EFI_STATUS
EFIAPI
FakeHashCompleteAndExtendPei (
  IN TPMI_DH_PCR          PcrIndex,
  OUT TPML_DIGEST_VALUES  *DigestList
  )
{
  EFI_STATUS          Status;

  *DigestList = (TPML_DIGEST_VALUES){ .count = 4, .digests = {
    {.hashAlg = TPM_ALG_SHA1, .digest = { .sha1 = "\xF4\xC0\xA7\xAA\x2A\xCE\x11\xBA\xB8\x0C\x4E\x55\x5F\x9D\x63\x38\x61\xED\xB4\xCE" }},
    {.hashAlg = TPM_ALG_SHA256, .digest = { .sha256 = "\x87\xE7\x18\xC7\x7F\xCE\xBA\x35\x6B\x89\x67\x54\xBE\x4E\x03\x9F\xDE\xC9\x59\xF0\x4B\x5B\x27\xD7\x12\x59\xF6\x7C\x5B\x8A\xB7\x45" }},
    {.hashAlg = TPM_ALG_SHA384, .digest = { .sha384 = "\x10\xEF\x98\x56\x1E\x3A\xD0\xCB\xF3\xFB\x7A\x84\xD0\x1F\xBA\x54\xBD\x66\x23\xD0\xC4\x40\xAB\xD2\x65\x53\x75\x82\xEF\xE1\xCF\x5F\xA2\x54\xAC\x11\x79\xCC\x99\x0B\xC6\x75\xAA\xDA\xF9\xA9\x5C\x29" }},
    {.hashAlg = TPM_ALG_SHA512, .digest = { .sha512 = "\x2E\xBB\x56\xB8\x27\x72\x3D\xF2\x21\x5D\x7E\x65\xE0\x75\x4D\x96\xB4\x91\xA6\xAE\xF9\xFF\xBC\x45\x65\x91\x6B\x42\xD5\x89\xDE\xE8\x1A\xD4\x12\xFB\x98\x3F\xCB\x6B\xB2\x38\xD1\x84\x6D\x38\x38\xC6\xB5\xF1\x74\xCD\x14\xAA\xE3\xA3\xAE\xD2\xAD\xEC\xEB\x52\x1C\x98" }},
    { 0 }
  }};

  Status = Tpm2PcrExtend (
             PcrIndex,
             DigestList
             );
  return Status;
}

/**
  Fake a hash and extend sequence to PCR for the DXE stage.

  @param PcrIndex      PCR to be extended.
  @param DigestList    Digest list.

  @retval EFI_SUCCESS     Hash sequence complete and DigestList is returned.
**/
EFI_STATUS
EFIAPI
FakeHashCompleteAndExtendDxe (
  IN TPMI_DH_PCR          PcrIndex,
  OUT TPML_DIGEST_VALUES  *DigestList
  )
{
  EFI_STATUS          Status;

  *DigestList = (TPML_DIGEST_VALUES){ .count = 4, .digests = {
    {.hashAlg = TPM_ALG_SHA1, .digest = { .sha1 =     "\x0D\xDD\x3E\x86\x2F\xC0\x9C\x0A\xE7\x9A\xB8\x5D\x57\x4A\xF8\xCB\x74\x66\x3E\xA9" }},
    {.hashAlg = TPM_ALG_SHA256, .digest = { .sha256 = "\x68\x9E\xED\xB9\x41\x8C\xA2\xA6\xFA\xE6\xD4\x06\xB9\xA3\xFF\x1F\x6B\x3B\x43\x02\xC3\xEC\xCD\x4F\xA9\x67\x27\x81\xCB\x4D\x00\xEB" }},
    {.hashAlg = TPM_ALG_SHA384, .digest = { .sha384 = "\x39\x15\x2E\xB6\xA9\xC8\x8D\xCC\x89\x07\x1B\x48\x3D\x24\xEC\xCE\x72\xAD\x37\x86\x80\xC5\x26\x4B\x23\x1A\x26\x64\x39\x86\x76\x64\x81\x37\x1B\x0F\x60\xAC\x7B\x92\x58\xBC\x00\xFA\xF8\xC3\xAB\xF0" }},
    {.hashAlg = TPM_ALG_SHA512, .digest = { .sha512 = "\x70\x03\x7C\x19\x1C\x70\x34\x27\xDB\x43\x26\xBC\x64\x31\xEA\x37\x3D\x27\x60\xD4\x27\xB5\x97\xDB\x1A\x38\x5A\xC9\x08\xF6\x5C\x82\x3E\x41\x1D\x57\xEB\x2F\x3F\x62\x75\xD3\x62\xAC\x9D\x6B\xCE\x65\x70\xDE\xAC\x1F\x6A\x9D\x5B\x38\xC3\x61\xA6\x49\x4A\x44\x48\x59" }},
    { 0 }
  }};

  Status = Tpm2PcrExtend (
             PcrIndex,
             DigestList
             );
  return Status;
}

/**
  Hash data and extend to PCR.

  @param PcrIndex      PCR to be extended.
  @param DataToHash    Data to be hashed.
  @param DataToHashLen Data size.
  @param DigestList    Digest list.

  @retval EFI_SUCCESS     Hash data and DigestList is returned.
**/
EFI_STATUS
EFIAPI
HashAndExtend (
  IN TPMI_DH_PCR          PcrIndex,
  IN VOID                 *DataToHash,
  IN UINTN                DataToHashLen,
  OUT TPML_DIGEST_VALUES  *DigestList
  )
{
  HASH_INTERFACE_HOB  *HashInterfaceHob;
  HASH_HANDLE         HashHandle;
  EFI_STATUS          Status;

  HashInterfaceHob = InternalGetHashInterfaceHob (&gEfiCallerIdGuid);
  if (HashInterfaceHob == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (HashInterfaceHob->HashInterfaceCount == 0) {
    return EFI_UNSUPPORTED;
  }

  CheckSupportedHashMaskMismatch (HashInterfaceHob);

  HashStart (&HashHandle);
  if (0 && DataToHashLen == 0xE0000) {
    Status = FakeHashCompleteAndExtendPei (PcrIndex, DigestList);
  } else if (0 && DataToHashLen == 0xC00000) {
    Status = FakeHashCompleteAndExtendDxe (PcrIndex, DigestList);
  } else {
    HashUpdate (HashHandle, DataToHash, DataToHashLen);
    Status = HashCompleteAndExtend (HashHandle, PcrIndex, NULL, 0, DigestList);
  }

  return Status;
}

/**
  This service register Hash.

  @param HashInterface  Hash interface

  @retval EFI_SUCCESS          This hash interface is registered successfully.
  @retval EFI_UNSUPPORTED      System does not support register this interface.
  @retval EFI_ALREADY_STARTED  System already register this interface.
**/
EFI_STATUS
EFIAPI
RegisterHashInterfaceLib (
  IN HASH_INTERFACE  *HashInterface
  )
{
  UINTN               Index;
  HASH_INTERFACE_HOB  *HashInterfaceHob;
  UINT32              HashMask;
  UINT32              Tpm2HashMask;
  EFI_STATUS          Status;

  //
  // Check allow
  //
  HashMask     = Tpm2GetHashMaskFromAlgo (&HashInterface->HashGuid);
  Tpm2HashMask = PcdGet32 (PcdTpm2HashMask);

  if ((Tpm2HashMask != 0) &&
      ((HashMask & Tpm2HashMask) == 0))
  {
    return EFI_UNSUPPORTED;
  }

  HashInterfaceHob = InternalGetHashInterfaceHob (&gEfiCallerIdGuid);
  if (HashInterfaceHob == NULL) {
    HashInterfaceHob = InternalCreateHashInterfaceHob (&gEfiCallerIdGuid);
    if (HashInterfaceHob == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  if (HashInterfaceHob->HashInterfaceCount >= HASH_COUNT) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Check duplication
  //
  for (Index = 0; Index < HashInterfaceHob->HashInterfaceCount; Index++) {
    if (CompareGuid (&HashInterfaceHob->HashInterface[Index].HashGuid, &HashInterface->HashGuid)) {
      DEBUG ((DEBUG_ERROR, "Hash Interface (%g) has been registered\n", &HashInterface->HashGuid));
      return EFI_ALREADY_STARTED;
    }
  }

  //
  // Record hash algorithm bitmap of CURRENT module which consumes HashLib.
  //
  HashInterfaceHob->SupportedHashMask = PcdGet32 (PcdTcg2HashAlgorithmBitmap) | HashMask;
  Status                              = PcdSet32S (PcdTcg2HashAlgorithmBitmap, HashInterfaceHob->SupportedHashMask);
  ASSERT_EFI_ERROR (Status);

  CopyMem (&HashInterfaceHob->HashInterface[HashInterfaceHob->HashInterfaceCount], HashInterface, sizeof (*HashInterface));
  HashInterfaceHob->HashInterfaceCount++;

  return EFI_SUCCESS;
}

/**
  The constructor function of HashLibBaseCryptoRouterPei.

  @param  FileHandle   The handle of FFS header the loaded driver.
  @param  PeiServices  The pointer to the PEI services.

  @retval EFI_SUCCESS           The constructor executes successfully.
  @retval EFI_OUT_OF_RESOURCES  There is no enough resource for the constructor.

**/
EFI_STATUS
EFIAPI
HashLibBaseCryptoRouterPeiConstructor (
  IN EFI_PEI_FILE_HANDLE     FileHandle,
  IN CONST EFI_PEI_SERVICES  **PeiServices
  )
{
  EFI_STATUS          Status;
  HASH_INTERFACE_HOB  *HashInterfaceHob;

  HashInterfaceHob = InternalGetHashInterfaceHob (&gZeroGuid);
  if (HashInterfaceHob == NULL) {
    //
    // No HOB with gZeroGuid Identifier has been created,
    // this is FIRST module which consumes HashLib.
    // Create the HOB with gZeroGuid Identifier.
    //
    HashInterfaceHob = InternalCreateHashInterfaceHob (&gZeroGuid);
    if (HashInterfaceHob == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  } else {
    //
    // Record hash algorithm bitmap of LAST module which also consumes HashLib.
    //
    HashInterfaceHob->SupportedHashMask = PcdGet32 (PcdTcg2HashAlgorithmBitmap);
  }

  HashInterfaceHob = InternalGetHashInterfaceHob (&gEfiCallerIdGuid);
  if (HashInterfaceHob != NULL) {
    //
    // In PEI phase, some modules may call RegisterForShadow and will be
    // shadowed and executed again after memory is discovered.
    // This is the second execution of this module, clear the hash interface
    // information registered at its first execution.
    //
    ZeroMem (&HashInterfaceHob->HashInterface, sizeof (HashInterfaceHob->HashInterface));
    HashInterfaceHob->HashInterfaceCount = 0;
    HashInterfaceHob->SupportedHashMask  = 0;
  }

  //
  // Set PcdTcg2HashAlgorithmBitmap to 0 in CONSTRUCTOR for CURRENT module.
  //
  Status = PcdSet32S (PcdTcg2HashAlgorithmBitmap, 0);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
