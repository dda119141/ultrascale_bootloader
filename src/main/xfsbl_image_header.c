/******************************************************************************
 * Copyright (c) 2015 - 2020 Xilinx, Inc.  All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file xfsbl_image_header.c
 *
 * This is the image header C file which does validation for the image header.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date        Changes
 * ----- ---- -------- -------------------------------------------------------
 * 1.00  kc   10/21/13 Initial release
 * 2.0   bv   12/05/16 Made compliance to MISRAC 2012 guidelines
 *       vns  01/28/17 Added API for XFsbl_GetVectorLocation and
 *                     XFsbl_GetBlockSize
 *       bv   02/11/17 Added warmrestart support, based on which partition
 *                     header is validated.
 *       vns  02/17/17 If authentication is enabled whole image header
 *                     is been validated by copying to OCM.
 *                     Updated destination CPU for PMUFW.
 * 3.0   vns  03/07/18 All the partitions should be encrypted when ENC_ONLY
 *                     eFUSE bit is set, if not encrypted FSBL throw an error.
 *       mus  02/26/19 Added support for armclang compiler
 *       bsv  07/05/19 Remove MD5 checksum related code
 *       bsv  07/19/19 Rectify the TCM limit used for validating DDR address
 *                     by R5 FSBL
 *
 * </pre>
 *
 * @note
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "xfsbl_image_header.h"

#include "xfsbl_hw.h"
#include "xfsbl_misc_drivers.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
u32 XFsbl_GetPartitionOwner(const XFsblPs_PartitionHeader* PartitionHeader) {
  return PartitionHeader->PartitionAttributes & XIH_PH_ATTRB_PART_OWNER_MASK;
}

u32 XFsbl_IsRsaSignaturePresent(
    const XFsblPs_PartitionHeader* PartitionHeader) {
  return PartitionHeader->PartitionAttributes & XIH_PH_ATTRB_RSA_SIGNATURE_MASK;
}

u32 XFsbl_GetChecksumType(
    const XFsblPs_PartitionHeader* const PartitionHeader) {
  return PartitionHeader->PartitionAttributes & XIH_PH_ATTRB_CHECKSUM_MASK;
}

u32 XFsbl_GetDestinationCpu(const XFsblPs_PartitionHeader* PartitionHeader) {
  return PartitionHeader->PartitionAttributes & XIH_PH_ATTRB_DEST_CPU_MASK;
}

u32 XFsbl_IsEncrypted(const XFsblPs_PartitionHeader* PartitionHeader) {
  return PartitionHeader->PartitionAttributes & XIH_PH_ATTRB_ENCRYPTION_MASK;
}

u32 XFsbl_GetDestinationDevice(const XFsblPs_PartitionHeader* PartitionHeader) {
  return PartitionHeader->PartitionAttributes & XIH_PH_ATTRB_DEST_DEVICE_MASK;
}

u32 XFsbl_GetA53ExecState(const XFsblPs_PartitionHeader* PartitionHeader) {
  return PartitionHeader->PartitionAttributes & XIH_PH_ATTRB_A53_EXEC_ST_MASK;
}

u32 XFsbl_GetVectorLocation(const XFsblPs_PartitionHeader* PartitionHeader) {
  return (PartitionHeader->PartitionAttributes &
          XIH_PH_ATTRB_VEC_LOCATION_MASK);
}

u32 XFsbl_GetBlockSize(const XFsblPs_PartitionHeader* PartitionHeader) {
  u32 Size =
      ((PartitionHeader->PartitionAttributes) & XIH_PH_ATTR_BLOCK_SIZE_MASK) >>
      XIH_ATTRB_BLOCK_SIZE_SHIFT;

  if (Size != 0x00U) {
    Size = ((u32)2 << Size) * XFSBL_MUL_MEGABYTES;
  }

  return Size;
}

/************************** Function Prototypes ******************************/
static u32 XFsbl_ValidateImageHeaderTable(
    XFsblPs_ImageHeaderTable* ImageHeaderTable);
static u32 XFsbl_CheckValidMemoryAddress(u64 Address, u32 CpuId, u32 DevId);
static void XFsbl_SetATFHandoffParameters(
    const XFsblPs_PartitionHeader* PartitionHeader, u32 EntryCount);

/************************** Variable Definitions *****************************/
/* Store this data structure at a fixed location for ATF to pick */
#ifdef __clang__
static XFsblPs_ATFHandoffParams ATFHandoffParams
    __attribute__((section(".bss.handoff_params")));
#else
static XFsblPs_ATFHandoffParams ATFHandoffParams
    __attribute__((section(".handoff_params")));
#endif

#ifdef XFSBL_SECURE
extern u8* ImageHdr;
#endif

/****************************************************************************/
/**
 *  This function is used to validate the word checksum for the image header
 *  table and partition headers.
 *  Checksum is based on the below formulae
 *  	Checksum = ~(X1 + X2 + X3 + .... + Xn)
 *
 * @param Buffer pointer for the data words.
 *
 * @param Length of the buffer for which checksum should be calculated.
 * last word is taken as checksum for the data to be compared against
 *
 * @return
 * 		- XFSBL_SUCCESS for successful checksum validation
 * 		- XFSBL_FAILURE if checksum validation fails
 *
 * @note
 *
 *****************************************************************************/
u32 XFsbl_ValidateChecksum(u32 Buffer[], u32 Length) {
  u32 Checksum = 0U;
  u32 Count = 0;

  /**
   * Length has to be at least equal to 2,
   */
  if (Length < 2U) {
    return XFSBL_FAILURE;
  }

  /**
   * Checksum = ~(X1 + X2 + X3 + .... + Xn)
   * Calculate the checksum
   */
  for (Count = 0U; Count < (Length - 1U); Count++) {
    /**
     * Read the word from the header
     */
    Checksum += Buffer[Count];
  }

  /**
   * Invert checksum
   */
  Checksum ^= 0xFFFFFFFFU;

  /**
   * Validate the checksum
   */
  if (Buffer[Length - 1U] != Checksum) {
    XFsbl_Printf(DEBUG_GENERAL, "Error: Checksum 0x%0lx != %0lx\r\n", Checksum,
                 Buffer[Length - 1U]);
    return XFSBL_FAILURE;
  } else {
    return XFSBL_SUCCESS;
  }
}

static void print_imageHeaderTable(
    const XFsblPs_ImageHeaderTable* const ImageHeaderTable) {
  XFsbl_Printf(DEBUG_INFO, "*****Image Header Table Details******** \n\r");
  XFsbl_Printf(DEBUG_INFO, "Boot Gen Ver: 0x%0lx \n\r",
               ImageHeaderTable->Version);
  XFsbl_Printf(DEBUG_INFO, "No of Partitions: 0x%0lx \n\r",
               ImageHeaderTable->NoOfPartitions);
  XFsbl_Printf(DEBUG_INFO, "Partition Header Address: 0x%0lx \n\r",
               ImageHeaderTable->PartitionHeaderAddress);
  XFsbl_Printf(DEBUG_INFO, "Partition Present Device: 0x%0lx \n\r",
               ImageHeaderTable->PartitionPresentDevice);
}

/****************************************************************************/
/**
 *  This function checks the fields of the image header table and validates
 *  them. Image header table contains the fields that common across the
 *  partitions and for image
 *
 * @param ImageHeaderTable pointer to the image header table.
 *
 * @return
 * 	- XFSBL_SUCCESS on successful image header table validation
 * 	- errors as mentioned in xfsbl_error.h
 *
 * @note
 *
 *****************************************************************************/
inline static u32 XFsbl_ValidateImageHeaderTable(
    XFsblPs_ImageHeaderTable* ImageHeaderTable) {
  /**
   * Check the check sum of the image header table
   */
  u32 Status = XFsbl_ValidateChecksum((u32*)ImageHeaderTable,
                                      XIH_IHT_LEN / XIH_PARTITION_WORD_LENGTH);
  if (XFSBL_SUCCESS != Status) {
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_IHT_CHECKSUM\n\r");
    return Status;
  }

  /**
   * check for the partition present device
   */
  u32 PartitionPresentDevice = ImageHeaderTable->PartitionPresentDevice;
  if ((PartitionPresentDevice < XIH_IHT_PPD_SAME) ||
      (PartitionPresentDevice > XIH_IHT_PPD_SATA)) {
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PPD\n\r");
    return XFSBL_ERROR_PPD;
  } else if (PartitionPresentDevice != XIH_IHT_PPD_SAME) {
    return XFSBL_STATUS_SECONDARY_BOOT_MODE;
  } else {
    /*MISRAC compliance */
  }

  /**
   * check for no of partitions
   */
  if ((ImageHeaderTable->NoOfPartitions <= XIH_MIN_PARTITIONS) ||
      (ImageHeaderTable->NoOfPartitions > XIH_MAX_PARTITIONS)) {
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_NO_OF_PARTITIONS\n\r");
    return XFSBL_ERROR_NO_OF_PARTITIONS;
  }

  print_imageHeaderTable(ImageHeaderTable);

  return Status;
}

inline static u32 retrieve_handoff_atf_parameters(
    XfsblPs_RuntimePartitionConfiguration* const PartConfig, u32 EntryCount) {
  const XFsblPs_PartitionHeader* const CurrPartitionHdr =
      PartConfig->PartitionHeader;
  u32 GetDstnCpu = XFsbl_GetDestinationCpu(CurrPartitionHdr);
  u32 DestnCPU = 0;

  /* if destination cpu is not present, it means it is for same cpu */
  if (GetDstnCpu == XIH_PH_ATTRB_DEST_CPU_NONE) {
    DestnCPU = PartConfig->RunningCpu;
  } else {
    DestnCPU = GetDstnCpu;
  }

  if ((PartConfig->PartitionIndex > 1U) &&
      (EntryCount < XFSBL_MAX_ENTRIES_FOR_ATF) &&
      (CurrPartitionHdr->DestinationExecutionAddress != 0U) &&
      (((DestnCPU >= XIH_PH_ATTRB_DEST_CPU_A53_0) &&
        (DestnCPU <= XIH_PH_ATTRB_DEST_CPU_A53_3)))) {
    /**
     *  Populate handoff parameters to ATF
     *  These correspond to the partition of application
     *  which ATF will be loading
     */
    XFsbl_SetATFHandoffParameters(CurrPartitionHdr, EntryCount);

    XFsbl_Printf(DEBUG_INFO, "ATP Handoff Parameters set Count %d\n\r",
                 EntryCount);
    return EntryCount + 1;
  } else {
  }
  return EntryCount;
}

/****************************************************************************/
/**
 * This function reads the image header from flash device. Image header
 * reads the image header table and partition headers
 *
 * @param ImageHeader pointer to the image header at which image header to
 * be stored
 *
 * @param DeviceOps pointer deviceops structure which contains the
 * function pointer to flash read function
 * 		 If it is NULL then image header will be copied from OCM buffer,
 * 		 which has image header copied from flash device.
 *
 * @param FlashImageOffsetAddress base offset to the image in the flash
 *
 * @return
 * 	- XFSBL_SUCCESS on successful reading image header
 * 	- errors as mentioned in xfsbl_error.h
 *
 * @note
 *****************************************************************************/
u32 XFsbl_ReadImageHeader(XFsblPs_ImageHeader* ImageHeader,
                          const XFsblPs_DeviceOps* DeviceOps,
                          u32 FlashImageOffsetAddress, u32 RunningCpu,
                          u32 ImageHeaderTableAddressOffset) {
  u32 Status = XFSBL_FAILURE;

  /**
   * Read the Image header table of 64 bytes
   * and update the image header table structure
   */
  if (DeviceOps) {
    Status = DeviceOps->DeviceCopy(
        FlashImageOffsetAddress + ImageHeaderTableAddressOffset,
        (PTRSIZE) & (ImageHeader->ImageHeaderTable), XIH_IHT_LEN);

    if (XFSBL_SUCCESS != Status) {
      XFsbl_Printf(DEBUG_GENERAL, "Device Copy Failed \n\r");
      return Status;
    }
  } else {
#ifdef XFSBL_SECURE
    XFsbl_MemCpy((u8*)&(ImageHeader->ImageHeaderTable), ImageHdr, XIH_IHT_LEN);
#endif
  }

  /**
   * Check the validity of Image Header Table
   */
  Status = XFsbl_ValidateImageHeaderTable(&(ImageHeader->ImageHeaderTable));
  if (Status == XFSBL_STATUS_SECONDARY_BOOT_MODE) {
    return XFSBL_SUCCESS;
  }
  if (XFSBL_SUCCESS != Status) {
    XFsbl_Printf(DEBUG_GENERAL,
                 "Image Header Table "
                 "Validation failed \n\r");
    return Status;
  }

  /**
   * Update the first partition address
   */
  u32 PartitionHeaderAddress =
      (ImageHeader->ImageHeaderTable.PartitionHeaderAddress) *
      XIH_PARTITION_WORD_LENGTH;

  XfsblPs_RuntimePartitionConfiguration PartConfig = {};

  /**
   * Read the partitions based on the partition offset
   * and update the partition header structure
   */
  u32 PartitionIndex = 0;
  for (PartitionIndex = 0U;
       PartitionIndex < ImageHeader->ImageHeaderTable.NoOfPartitions;
       PartitionIndex++) {
    /**
     * Read the Image header table of 64 bytes
     * and update the image header table structure
     */
    if (DeviceOps != NULL) {
      Status = DeviceOps->DeviceCopy(
          FlashImageOffsetAddress + PartitionHeaderAddress,
          (PTRSIZE) & (ImageHeader->PartitionHeader[PartitionIndex]),
          XIH_PH_LEN);

      if (XFSBL_SUCCESS != Status) {
        XFsbl_Printf(DEBUG_GENERAL, "Device Copy Failed \n\r");
        return Status;
      }

    } else {
    }

    /**
     * Assumption: Next partition corresponds to ATF
     * The first partition of an application will have a non zero
     * execution address. All the remaining partitions of that
     * application will have 0 as execution address. Hence look for
     * the non zero execution address for partition which is not
     * the first one and ensure the CPU is A53
     */
    PartConfig.PartitionHeader = &ImageHeader->PartitionHeader[PartitionIndex];
    PartConfig.RunningCpu = RunningCpu;
    PartConfig.PartitionIndex = PartitionIndex;

    u32 ATF_params_retrieval_count = 0;
    const u32 Count = retrieve_handoff_atf_parameters(
        &PartConfig, ATF_params_retrieval_count);
    ATF_params_retrieval_count = Count;

    /**
     * Update the next partition present address
     */
    PartitionHeaderAddress =
        (ImageHeader->PartitionHeader[PartitionIndex].NextPartitionOffset) *
        XIH_PARTITION_WORD_LENGTH;
  }

  /**
   * After setting handoff parameters of all partitions to ATF,
   * store address of the structure in PMU_GLOBAL.GLOBAL_GEN_STORAGE6.
   */
  XFsbl_Out32(PMU_GLOBAL_GLOB_GEN_STORAGE6, (u32)((PTRSIZE)&ATFHandoffParams));

  return XFSBL_SUCCESS;
}

static u32 XFsbl_CheckValidMemoryAddress(u64 Address, u32 CpuId, u32 DevId) {
  /* Check if Address is in the range of PMU RAM for PMU FW */
  if (CpuId == XIH_PH_ATTRB_DEST_CPU_PMU) {
    if ((Address >= XFSBL_PMU_RAM_START_ADDRESS) &&
        (Address < XFSBL_PMU_RAM_END_ADDRESS)) {
      return XFSBL_SUCCESS;
    }
  }

  /* Check if Address is in the range of TCM for R5_0/R5_1 */
  if ((CpuId == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
      (CpuId == XIH_PH_ATTRB_DEST_CPU_R5_1)) {
    if (((Address >= XFSBL_R5_TCM_START_ADDRESS) &&
         (Address < (XFSBL_R5_TCM_START_ADDRESS + XFSBL_R5_TCM_BANK_LENGTH))) ||
        ((Address >= XFSBL_R5_BTCM_START_ADDRESS) &&
         (Address <
          (XFSBL_R5_BTCM_START_ADDRESS + XFSBL_R5_TCM_BANK_LENGTH)))) {
      return XFSBL_SUCCESS;
    }
  }

  /* Check if Address is in the range of TCM for R5_L */
  if (CpuId == XIH_PH_ATTRB_DEST_CPU_R5_L) {
    if ((Address >= XFSBL_R5_TCM_START_ADDRESS) &&
        (Address <
         (XFSBL_R5_TCM_START_ADDRESS + (XFSBL_R5_TCM_BANK_LENGTH * 4U)))) {
      return XFSBL_SUCCESS;
    }
  }

#ifdef XFSBL_PS_DDR
  /**
   * Check if Address is in the range of PS DDR
   */
  if ((Address >= XFSBL_PS_DDR_START_ADDRESS) &&
      (Address <= XFSBL_PS_DDR_END_ADDRESS)) {
    return XFSBL_SUCCESS;
  }

#  ifdef XFSBL_PS_HI_DDR_START_ADDRESS
  /* Check if Address is in the range of HIGH PS DDR */
  if ((Address >= XFSBL_PS_HI_DDR_START_ADDRESS) &&
      (Address <= XFSBL_PS_HI_DDR_END_ADDRESS)) {
    return XFSBL_SUCCESS;
  }
#  endif
#endif

#ifdef XFSBL_PL_DDR
  /**
   * Check if Address is in the range of PS DDR
   */
  if ((Address >= XFSBL_PL_DDR_START_ADDRESS) &&
      (Address < XFSBL_PL_DDR_END_ADDRESS)) {
    return XFSBL_SUCCESS;
  }
#endif

#ifdef XFSBL_OCM
  /**
   * Check if Address is in the range of last bank of OCM
   */
  if ((Address >= XFSBL_OCM_START_ADDRESS) &&
      (Address < XFSBL_OCM_END_ADDRESS)) {
    return XFSBL_SUCCESS;
  }
#endif

  /**
   * If destination device is PL and load address is not configured,
   * don't consider this as error as we will use temp load address
   * to load PL bitstream
   */
  if ((DevId == XIH_PH_ATTRB_DEST_DEVICE_PL) &&
      (Address == XFSBL_DUMMY_PL_ADDR)) {
    return XFSBL_SUCCESS;
  }

  /**
   * Not a valid address
   */
  XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_ADDRESS: %llx\n\r", Address);
  return XFSBL_ERROR_ADDRESS;
}

static u32 verify_encryption_values(
    const XFsblPs_PartitionHeader* const PartitionHeader) {
  u32 IsEncrypted = FALSE;
  if (XFsbl_IsEncrypted(PartitionHeader) == XIH_PH_ATTRB_ENCRYPTION) {
    IsEncrypted = TRUE;
  } else {
    IsEncrypted = FALSE;
  }

  u32 IsAuthenticated = FALSE;
  if (XFsbl_IsRsaSignaturePresent(PartitionHeader) ==
      XIH_PH_ATTRB_RSA_SIGNATURE) {
    IsAuthenticated = TRUE;
  } else {
    IsAuthenticated = FALSE;
  }

#if defined(XFSBL_SECURE) && defined(XFSBL_FORCE_ENC)
  u32 EncOnly;

  /* Check if encryption is mandatory */
  EncOnly = XFsbl_In32(EFUSE_SEC_CTRL) & EFUSE_SEC_CTRL_ENC_ONLY_MASK;
  if ((EncOnly != 0x00) && (IsEncrypted == FALSE)) {
    XFsbl_Printf(DEBUG_GENERAL,
                 "XFSBL_ERROR_ENC_IS_MANDATORY as "
                 "eFUSE ENC_ONLY bit is set\r\n");
    return XFSBL_FAILURE;
  }
#endif
  /**
   * check for authentication and encryption length
   */
  if ((IsAuthenticated == FALSE) && (IsEncrypted == FALSE)) {
    /**
     * all lengths should be equal
     */
    if ((PartitionHeader->UnEncryptedDataWordLength !=
         PartitionHeader->EncryptedDataWordLength) ||
        (PartitionHeader->EncryptedDataWordLength !=
         PartitionHeader->TotalDataWordLength)) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PARTITION_LENGTH\n\r");
      return XFSBL_ERROR_PARTITION_LENGTH;
    }
  } else if ((IsAuthenticated == TRUE) && (IsEncrypted == FALSE)) {
    if ((PartitionHeader->UnEncryptedDataWordLength !=
         PartitionHeader->EncryptedDataWordLength) ||
        (PartitionHeader->EncryptedDataWordLength >=
         PartitionHeader->TotalDataWordLength)) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PARTITION_LENGTH\n\r");
      return XFSBL_ERROR_PARTITION_LENGTH;
    }
  } else if ((IsAuthenticated == FALSE) && (IsEncrypted == TRUE)) {
    if ((PartitionHeader->UnEncryptedDataWordLength >=
         PartitionHeader->EncryptedDataWordLength) ||
        (PartitionHeader->EncryptedDataWordLength !=
         PartitionHeader->TotalDataWordLength)) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PARTITION_LENGTH\n\r");
      return XFSBL_ERROR_PARTITION_LENGTH;
    }
  } else /* Authenticated and Encrypted */
  {
    if ((PartitionHeader->UnEncryptedDataWordLength >=
         PartitionHeader->EncryptedDataWordLength) ||
        (PartitionHeader->EncryptedDataWordLength >=
         PartitionHeader->TotalDataWordLength)) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PARTITION_LENGTH\n\r");
      return XFSBL_ERROR_PARTITION_LENGTH;
    }
  }

  return XFSBL_SUCCESS;
}

inline static void print_partitionHeaderDetails(
    const XFsblPs_PartitionHeader* const PartitionHeader) {
  XFsbl_Printf(DEBUG_INFO, "UnEncrypted data Length: 0x%0lx \n\r",
               PartitionHeader->UnEncryptedDataWordLength);
  XFsbl_Printf(DEBUG_INFO, "Data word offset: 0x%0lx \n\r",
               PartitionHeader->EncryptedDataWordLength);
  XFsbl_Printf(DEBUG_INFO, "Total Data word length: 0x%0lx \n\r",
               PartitionHeader->TotalDataWordLength);
  XFsbl_Printf(DEBUG_INFO, "Destination Load Address: 0x%0lx \n\r",
               (PTRSIZE)PartitionHeader->DestinationLoadAddress);
  XFsbl_Printf(DEBUG_INFO, "Execution Address: 0x%0lx \n\r",
               (PTRSIZE)PartitionHeader->DestinationExecutionAddress);
  XFsbl_Printf(DEBUG_INFO, "Data word offset: 0x%0lx \n\r",
               PartitionHeader->DataWordOffset);
  XFsbl_Printf(DEBUG_INFO, "Partition Attributes: 0x%0lx \n\r",
               PartitionHeader->PartitionAttributes);
}

inline static u32 rCore_boot_check_dest_cpu(
    const XFsblPs_PartitionHeader* const PartitionHeader, u32 RunningCpu,
    u32 DestinationCpu) {
  /**
   * R5 can't access the DDR 0 address as TCM is present there
   */
  if (((RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
       (RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_L)) &&
      ((DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_0) ||
       (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_1) ||
       (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_2) ||
       (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_3))) {
    /**
     * DDR address for A53-x should be greater than TCM
     */
    if (PartitionHeader->DestinationLoadAddress < 0x40000U) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_ADDRESS\n\r");
      return XFSBL_ERROR_ADDRESS;
    }
  }
  /**
   * check if
   *  1. if FSBL on R5-0 and Destination CPU is R5-L
   *  2. if FSBL on R5-L and Destination CPU is R5-0/R5-1
   */
  if ((((DestinationCpu == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
        (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_R5_1)) &&
       (RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_L)) ||

      ((DestinationCpu == XIH_PH_ATTRB_DEST_CPU_R5_L) &&
       (RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_0))) {
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_LS_CPU_TYPE\n\r");
    return XFSBL_ERROR_LS_CPU_TYPE;
  }

  return XFSBL_SUCCESS;
}

/****************************************************************************/
/**
 * This function validates the partition header.
 *
 * @param PartitionHeader is pointer to the XFsblPs_PartitionHeader structure
 *
 * @return
 * 	- XFSBL_SUCCESS on successful partition header validation
 * 	- errors as mentioned in xfsbl_error.h
 *
 * @note
 *
 *****************************************************************************/
u32 XFsbl_ValidatePartitionHeader(XFsblPs_PartitionHeader* PartitionHeader,
                                  u32 RunningCpu, u32 ResetType) {
  u32 DestinationCpu = XFsbl_GetDestinationCpu(PartitionHeader);
  if (XFSBL_MASTER_ONLY_RESET == ResetType) {
    if (((XIH_PH_ATTRB_DEST_CPU_A53_0 == RunningCpu) &&
         (DestinationCpu <= XIH_PH_ATTRB_DEST_CPU_A53_3) &&
         (DestinationCpu >= XIH_PH_ATTRB_DEST_CPU_A53_0)) ||
        ((XIH_PH_ATTRB_DEST_CPU_R5_0 == RunningCpu) &&
         (XIH_PH_ATTRB_DEST_CPU_R5_0 == DestinationCpu)) ||
        ((XIH_PH_ATTRB_DEST_CPU_R5_L == RunningCpu) &&
         (XIH_PH_ATTRB_DEST_CPU_R5_L == DestinationCpu))) {
      /* Do Nothing*/
    } else {
      return XFSBL_SUCCESS_NOT_PARTITION_OWNER;
    }
  } else {
    /*Do Nothing*/
  }

  /* if destination cpu is not present, it means it is for same cpu */
  if (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_NONE) {
    DestinationCpu = RunningCpu;
  }

  u32 Status = verify_encryption_values(PartitionHeader);
  if (Status != XFSBL_SUCCESS) {
    return Status;
  }

  u32 DestinationDevice = XFsbl_GetDestinationDevice(PartitionHeader);
  Status =
      XFsbl_CheckValidMemoryAddress(PartitionHeader->DestinationLoadAddress,
                                    DestinationCpu, DestinationDevice);
  if (Status != XFSBL_SUCCESS) {
    return Status;
  }

  Status =
      rCore_boot_check_dest_cpu(PartitionHeader, RunningCpu, DestinationCpu);
  if (Status != XFSBL_SUCCESS) {
    return Status;
  }

  /**
   * Checks for invalid checksum type
   */
  if ((XFsbl_GetChecksumType(PartitionHeader) != XIH_PH_ATTRB_NOCHECKSUM) &&
      (XFsbl_GetChecksumType(PartitionHeader) != XIH_PH_ATTRB_HASH_SHA3)) {
    Status = XFSBL_ERROR_INVALID_CHECKSUM_TYPE;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_INVALID_CHECKSUM_TYPE\n\r");
    return Status;
  }

  /**
   * check for invalid cpu
   */
  if (DestinationCpu > XIH_PH_ATTRB_DEST_CPU_PMU) {
    Status = XFSBL_ERROR_INVALID_CPU_TYPE;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_INVALID_CPU_TYPE\n\r");
    return Status;
  }

  /**
   * check for invalid destination device
   */
  if (XFsbl_GetDestinationDevice(PartitionHeader) >
      XIH_PH_ATTRB_DEST_DEVICE_PMU) {
    Status = XFSBL_ERROR_INVALID_DEST_DEVICE;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_INVALID_DEST_DEVICE\n\r");
    return Status;
  }

  /**
   * Print Partition Header Details
   */
  print_partitionHeaderDetails(PartitionHeader);
  return XFSBL_SUCCESS;
}

/****************************************************************************/
/**
 * This function sets the handoff parameters to the ARM Trusted Firmware (ATF)
 * Some of the inputs for this are taken from FSBL partition header
 * A pointer to the structure containing these parameters is stored in the
 * PMU_GLOBAL.GLOBAL_GEN_STORAGE6 register, which ATF reads.
 *
 * @param PartitionHeader is pointer to the XFsblPs_PartitionHeader structure
 *
 * @return None
 *
 * @note
 *
 *****************************************************************************/
static void XFsbl_SetATFHandoffParameters(
    const XFsblPs_PartitionHeader* PartitionHeader, u32 EntryCount) {
  u32 PartitionAttributes;
  u32 PartitionFlags;

  PartitionAttributes = PartitionHeader->PartitionAttributes;

  PartitionFlags = (((PartitionAttributes & XIH_PH_ATTRB_A53_EXEC_ST_MASK) >>
                     XIH_ATTRB_A53_EXEC_ST_SHIFT_DIFF) |
                    ((PartitionAttributes & XIH_PH_ATTRB_ENDIAN_MASK) >>
                     XIH_ATTRB_ENDIAN_SHIFT_DIFF) |
                    ((PartitionAttributes & XIH_PH_ATTRB_TR_SECURE_MASK)
                     << XIH_ATTRB_TR_SECURE_SHIFT_DIFF) |
                    ((PartitionAttributes & XIH_PH_ATTRB_TARGET_EL_MASK)
                     << XIH_ATTRB_TARGET_EL_SHIFT_DIFF));

  /* Update CPU number based on destination CPU */
  if ((PartitionAttributes & XIH_PH_ATTRB_DEST_CPU_MASK) ==
      XIH_PH_ATTRB_DEST_CPU_A53_0) {
    PartitionFlags |= XIH_PART_FLAGS_DEST_CPU_A53_0;
  } else if ((PartitionAttributes & XIH_PH_ATTRB_DEST_CPU_MASK) ==
             XIH_PH_ATTRB_DEST_CPU_A53_1) {
    PartitionFlags |= XIH_PART_FLAGS_DEST_CPU_A53_1;
  } else if ((PartitionAttributes & XIH_PH_ATTRB_DEST_CPU_MASK) ==
             XIH_PH_ATTRB_DEST_CPU_A53_2) {
    PartitionFlags |= XIH_PART_FLAGS_DEST_CPU_A53_2;
  } else {
    PartitionFlags |= XIH_PART_FLAGS_DEST_CPU_A53_3;
  }

  /* Insert magic string */
  if (EntryCount == 0U) {
    ATFHandoffParams.MagicValue[0] = 'X';
    ATFHandoffParams.MagicValue[1] = 'L';
    ATFHandoffParams.MagicValue[2] = 'N';
    ATFHandoffParams.MagicValue[3] = 'X';
  }

  ATFHandoffParams.NumEntries = EntryCount + 1U;

  ATFHandoffParams.Entry[EntryCount].EntryPoint =
      PartitionHeader->DestinationExecutionAddress;
  ATFHandoffParams.Entry[EntryCount].PartitionFlags = PartitionFlags;
}
