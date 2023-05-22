// vim: set et:

#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>
#include <Pi/PiPeiCis.h>


extern EFI_GUID gTcgTestPpiGuid;

EFI_PEI_PPI_DESCRIPTOR ppi = {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gTcgTestPpiGuid,
    &gTcgTestPpiGuid
};

EFI_STATUS
EFIAPI
TcgTestPpiEntryPoint(IN EFI_PEI_FILE_HANDLE FileHandle,
                     IN CONST EFI_PEI_SERVICES **PeiServices)
{
    PeiServicesInstallPpi(&ppi);

    return EFI_SUCCESS;
}
