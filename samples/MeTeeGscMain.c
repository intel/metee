/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2026 Intel Corporation
 */

#include <Uefi.h>
#include <Library/UefiLib.h>

extern int main(int argc, char* argv[]);

/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point executed successfully.
  @retval other             Some error occurred when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  int ret;

  (void)ImageHandle;
  (void)SystemTable;

  ret = main(0, NULL);

  return (ret == 0) ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}