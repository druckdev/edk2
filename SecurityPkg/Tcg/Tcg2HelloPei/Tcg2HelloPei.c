// vim: set et:

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/HobLib.h>





extern EFI_GUID gTestHobGuid;
extern EFI_GUID gAmiTreePpiGuid;

EFI_STATUS
EFIAPI
TPMHelloEntryPoint(
        IN       EFI_PEI_FILE_HANDLE FileHandle,
        IN CONST EFI_PEI_SERVICES    **PeiServices
) {
    // https://edk2-docs.gitbook.io/edk-ii-module-writer-s-guide/7_pre-efi_initialization_modules/76_communicate_with_dxe_modules
    char test_str[] = "s\x0EThis is a testabc";
    void* test_hob = BuildGuidHob(&gTestHobGuid, sizeof(test_str));

    if (test_hob == NULL)
        return EFI_OUT_OF_RESOURCES;

    CopyMem(test_hob, test_str, sizeof(test_str));

    // PeiServicesLocatePpi(&gAmiTreePpiGuid, 0, NULL, (VOID**)&AmiTreePpi);

    return EFI_SUCCESS;
}
