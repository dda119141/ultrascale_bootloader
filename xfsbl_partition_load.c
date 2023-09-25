/******************************************************************************
 * Copyright (c) 2015 - 2021 Xilinx, Inc.  All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file xfsbl_partition_load.c
 *
 * This is the file which contains partition load code for the FSBL.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date        Changes
 * ----- ---- -------- -------------------------------------------------------
 * 1.00  kc   10/21/13 Initial release
 * 2.0   bv   12/02/16 Made compliance to MISRAC 2012 guidelines
 *       bo   01/25/17 Fixed Vector regions overwritten in R5 FSBL
 *       vns  03/01/17 Enhanced security of bitstream authentication
 *                     Modified endianness of IV as APIs are modified in
 *Xilsecure While loading bitstream clearing of PL is skipped when PL is already
 *cleared at initialize. Updated destination cpu for PMUFW. bv   03/20/17
 *Removed isolation in PS - PL AXI bus thus allowing access to BRAM in PS only
 *reset vns  04/04/17 Corrected IV location w.r.t Image offset. 3.0   vns
 *01/03/18 Modified XFsbl_PartitionValidation() API, for each partition by
 *adding IV of LSB 8 bits with 8 bits of IV from XFsblPs_PartitionHeader
 *structure. vns  03/07/18 Iv copying is limited to only once from boot header,
 *                     and is used for every partition, In authentication case
 *                     we are using IV from authenticated header(copied to
 *                     internal memory), using same way for non authenticated
 *                     case as well.
 *       mus  02/26/19 Added support for armclang compiler.
 *       skd  02/02/20 Added register writes to PMU GLOBAL to indicate PL
 *configuration har  09/22/20 Removed checks for IsCheckSumEnabled with
 *authentication and encryption bsv  28/01/21 Fix build issues in case SECURE
 *and BITSTREAM code are excluded bsv  04/01/21 Added TPM support bsv  04/28/21
 *Added support to ensure authenticated images boot as non-secure when RSA_EN is
 *not programmed and boot header is not authenticated bsv  05/03/21 Add
 *provision to load bitstream from OCM with DDR present in design bsv  05/15/21
 *Support to ensure authenticated images boot as non-secure when RSA_EN is not
 *programmed and boot header is not authenticated is disabled by default
 *
 * </pre>
 *
 * @note
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "psu_init.h"
#include "xfsbl_hooks.h"
#include "xfsbl_hw.h"
#include "xfsbl_image_header.h"
#include "xfsbl_main.h"
#include "xil_cache.h"

#ifdef MAX_TODO
#  include "xfsbl_tpm.h"
#endif

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
#define XFSBL_IVT_LENGTH (u32)(0x20U)
#define XFSBL_R5_HIVEC (u32)(0xffff0000U)
#define XFSBL_R5_LOVEC (u32)(0x0U)
#define XFSBL_SET_R5_SCTLR_VECTOR_BIT (u32)(1 << 13)
#define XFSBL_PARTITION_IV_MASK (0xFFU)
#ifdef XFSBL_BS
#  define XFSBL_STATE_MASK 0x00FF0000U
#  define XFSBL_STATE_SHIFT 16U
#  define XFSBL_FIRMWARE_STATE_UNKNOWN 0U
#  define XFSBL_FIRMWARE_STATE_SECURE 1U
#  define XFSBL_FIRMWARE_STATE_NONSECURE 2U
#endif
#ifdef XFSBL_TPM
#  define XFSBL_EL2_VAL (4U)
#  define XFSBL_EL3_VAL (6U)
#endif

/************************** Function Prototypes ******************************/
static u32 XFsbl_PartitionHeaderValidation(XFsblPs* const FsblInstancePtr,
                                           u32 PartitionNum);
static u32 XFsbl_PartitionValidation(XFsblPs* const FsblInstancePtr,
                                     u32 PartitionNum);
static void XFsbl_CheckPmuFw(const XFsblPs* const FsblInstancePtr,
                             u32 PartitionNum);
#ifdef USE_CRYPTO_LIB
static u32 XFsbl_ValidateCheckSum(const XFsblPs* const FsblInstancePtr,
                                  PTRSIZE LoadAddress, u32 PartitionNum,
                                  u8* PartitionHash);
#endif
#ifdef XFSBL_BS
static void XFsbl_SetBSSecureState(u32 State);
#endif

#ifdef XFSBL_ENABLE_DDR_SR
static void XFsbl_PollForDDRReady(void);
#endif
#ifdef XFSBL_TPM
static u8 XFsbl_GetPcrIndex(const XFsblPs* FsblInstancePtr, u32 PartitionNum);
#endif

/************************** Variable Definitions *****************************/
#ifdef XFSBL_SECURE
u32 Iv[XIH_BH_IV_LENGTH / 4U] = {0};
u8 AuthBuffer[XFSBL_AUTH_BUFFER_SIZE] __attribute__((aligned(4))) = {0};
#  ifdef XFSBL_BS
#    ifdef __clang__
u8 HashsOfChunks[HASH_BUFFER_SIZE]
    __attribute__((section(".bss.bitstream_buffer")));
#    else
u8 HashsOfChunks[HASH_BUFFER_SIZE]
    __attribute__((section(".bitstream_buffer")));
#    endif
#  endif
#endif

/* buffer for storing chunks for bitstream */
#if defined(XFSBL_BS)
extern u8 ReadBuffer[READ_BUFFER_SIZE];
#endif
/*****************************************************************************/
/**
 * This function loads the partition
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @param	PartitionNum is the partition number in the image to be loaded
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 *****************************************************************************/
u32 XFsbl_PartitionLoad(XFsblPs* const FsblInstancePtr, u32 PartitionNum) {
  u32 Status;

#ifdef XFSBL_WDT_PRESENT
  if (XFSBL_MASTER_ONLY_RESET != FsblInstancePtr->ResetReason) {
    /* Restart WDT as partition copy can take more time */
    XFsbl_RestartWdt();
  }
#endif

#ifdef XFSBL_ENABLE_DDR_SR
  XFsbl_PollForDDRReady();
#endif

  Status = XFsbl_PartitionHeaderValidation(FsblInstancePtr, PartitionNum);

  /**
   * FSBL is not partition owner and skip this partition
   */
  if (Status == XFSBL_SUCCESS_NOT_PARTITION_OWNER) {
    return XFSBL_SUCCESS;
  } else if (XFSBL_SUCCESS != Status) {
    return Status;
  } else {
  }

  Status = XFsbl_PartitionCopy(FsblInstancePtr, PartitionNum);
  if (XFSBL_SUCCESS != Status) {
    return Status;
  }

  Status = XFsbl_PartitionValidation(FsblInstancePtr, PartitionNum);
  if (XFSBL_SUCCESS != Status) {
    return Status;
  }

  /* Check if PMU FW load is done and handoff it to Microblaze */
  XFsbl_CheckPmuFw(FsblInstancePtr, PartitionNum);

  XFsbl_Printf(DEBUG_GENERAL, "Partition load - No pmu firmware present\n\r");

  return XFSBL_SUCCESS;
}

/*****************************************************************************/
/**
 * This function validates the partition header
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @param	PartitionNum is the partition number in the image to be loaded
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *****************************************************************************/
static u32 XFsbl_PartitionHeaderValidation(XFsblPs* FsblInstancePtr,
                                           u32 PartitionNum) {
  u32 Status;

  XFsblPs_PartitionHeader* PartitionHeader =
      &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum];

  Status = XFsbl_ValidateChecksum((u32*)PartitionHeader, XIH_PH_LEN / 4U);
  if (XFSBL_SUCCESS != Status) {
    Status = XFSBL_ERROR_PH_CHECKSUM_FAILED;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PH_CHECKSUM_FAILED\n\r");
    return Status;
  }

  Status = XFsbl_GetPartitionOwner(PartitionHeader);
  if (Status != XIH_PH_ATTRB_PART_OWNER_FSBL) {
    /**
     * If the partition doesn't belong to FSBL, skip the partition
     */
    XFsbl_Printf(DEBUG_GENERAL, "Skipping the Partition 0x%0lx\n",
                 PartitionNum);
    Status = XFSBL_SUCCESS_NOT_PARTITION_OWNER;
    return Status;
  }

  return XFsbl_ValidatePartitionHeader(PartitionHeader,
                                       FsblInstancePtr->ProcessorID,
                                       FsblInstancePtr->ResetReason);
}

/****************************************************************************/
/**
 * This function is used to check whether cpu has handoff address stored
 * in the handoff structure
 *
 * @param FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @param DestinationCpu is the cpu which needs to be checked
 *
 * @return
 * 		- XFSBL_SUCCESS if cpu handoff address is not present
 * 		- XFSBL_FAILURE if cpu handoff address is present
 *
 * @note
 *
 *****************************************************************************/
u32 XFsbl_CheckHandoffCpu(const XFsblPs* const FsblInstancePtr,
                          u32 DestinationCpu) {
  u32 ValidHandoffCpuNo;
  u32 Index;
  u32 CpuId;

  ValidHandoffCpuNo = FsblInstancePtr->HandoffCpuNo;

  for (Index = 0U; Index < ValidHandoffCpuNo; Index++) {
    CpuId = FsblInstancePtr->HandoffValues[Index].CpuSettings &
            XIH_PH_ATTRB_DEST_CPU_MASK;
    if (CpuId == DestinationCpu) {
      return XFSBL_FAILURE;
    }
  }

  return XFSBL_SUCCESS;
}

/*****************************************************************************/
/**
 * This function checks the power state and reset for the memory type
 * and release the reset if required
 *
 * @param	MemoryType is the memory to be checked
 * 			- XFSBL_R5_0_TCM
 * 			- XFSBL_R5_1_TCM
 *				(to be added)
 *			- XFSBL_R5_0_TCMA
 *			- XFSBL_R5_0_TCMB
 *			- XFSBL_PS_DDR
 *			- XFSBL_PL_DDR
 *
 * @return	none
 *****************************************************************************/
u32 XFsbl_PowerUpMemory(u32 MemoryType) {
  u32 RegValue;
  u32 Status;
  u32 PwrStateMask;

  /**
   * Check the power status of the memory
   * Power up if required
   *
   * Release the reset of the memory if present
   */
  switch (MemoryType) {
  case XFSBL_R5_0_TCM: {
    PwrStateMask =
        (PMU_GLOBAL_PWR_STATE_R5_0_MASK | PMU_GLOBAL_PWR_STATE_TCM0A_MASK |
         PMU_GLOBAL_PWR_STATE_TCM0B_MASK);

    Status = XFsbl_PowerUpIsland(PwrStateMask);

    if (Status != XFSBL_SUCCESS) {
      Status = XFSBL_ERROR_R5_0_TCM_POWER_UP;
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_R5_0_TCM_POWER_UP\r\n");
      goto END;
    }

    /**
     * To access TCM,
     * 	Release reset to R5 and enable the clk
     * 	R5 is under halt state
     *
     * 	If R5 are out of reset and clk is enabled so
     * doing again is no issue. R5 might be under running
     * state
     */

    /**
     * Place R5, TCM in split mode
     */
    RegValue = XFsbl_In32(RPU_RPU_GLBL_CNTL);
    RegValue |= RPU_RPU_GLBL_CNTL_SLSPLIT_MASK;
    RegValue &= ~(RPU_RPU_GLBL_CNTL_TCM_COMB_MASK);
    RegValue &= ~(RPU_RPU_GLBL_CNTL_SLCLAMP_MASK);
    XFsbl_Out32(RPU_RPU_GLBL_CNTL, RegValue);

    /**
     * Place R5-0 in HALT state
     */
    RegValue = XFsbl_In32(RPU_RPU_0_CFG);
    RegValue &= ~(RPU_RPU_0_CFG_NCPUHALT_MASK);
    XFsbl_Out32(RPU_RPU_0_CFG, RegValue);

    /**
     *  Enable the clock
     */
    RegValue = XFsbl_In32(CRL_APB_CPU_R5_CTRL);
    RegValue |= CRL_APB_CPU_R5_CTRL_CLKACT_MASK;
    XFsbl_Out32(CRL_APB_CPU_R5_CTRL, RegValue);

    /**
     * Provide some delay,
     * so that clock propagates properly.
     */
    (void)usleep(0x50U);

    /**
     * Release reset to R5-0
     */
    RegValue = XFsbl_In32(CRL_APB_RST_LPD_TOP);
    RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_R50_RESET_MASK);
    RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_AMBA_RESET_MASK);
    XFsbl_Out32(CRL_APB_RST_LPD_TOP, RegValue);
  } break;

  case XFSBL_R5_1_TCM: {
    PwrStateMask =
        (PMU_GLOBAL_PWR_STATE_R5_1_MASK | PMU_GLOBAL_PWR_STATE_TCM1A_MASK |
         PMU_GLOBAL_PWR_STATE_TCM1B_MASK);

    Status = XFsbl_PowerUpIsland(PwrStateMask);

    if (Status != XFSBL_SUCCESS) {
      Status = XFSBL_ERROR_R5_1_TCM_POWER_UP;
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_R5_1_TCM_POWER_UP\r\n");
      goto END;
    }

    /**
     * Place R5 in split mode
     */
    RegValue = XFsbl_In32(RPU_RPU_GLBL_CNTL);
    RegValue |= RPU_RPU_GLBL_CNTL_SLSPLIT_MASK;
    RegValue &= ~(RPU_RPU_GLBL_CNTL_TCM_COMB_MASK);
    RegValue &= ~(RPU_RPU_GLBL_CNTL_SLCLAMP_MASK);
    XFsbl_Out32(RPU_RPU_GLBL_CNTL, RegValue);

    /**
     * Place R5-1 in HALT state
     */
    RegValue = XFsbl_In32(RPU_RPU_1_CFG);
    RegValue &= ~(RPU_RPU_1_CFG_NCPUHALT_MASK);
    XFsbl_Out32(RPU_RPU_1_CFG, RegValue);

    /**
     *  Enable the clock
     */
    RegValue = XFsbl_In32(CRL_APB_CPU_R5_CTRL);
    RegValue |= CRL_APB_CPU_R5_CTRL_CLKACT_MASK;
    XFsbl_Out32(CRL_APB_CPU_R5_CTRL, RegValue);

    /**
     * Provide some delay,
     * so that clock propagates properly.
     */
    (void)usleep(0x50U);

    /**
     * Release reset to R5-1
     */
    RegValue = XFsbl_In32(CRL_APB_RST_LPD_TOP);
    RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_R51_RESET_MASK);
    RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_AMBA_RESET_MASK);
    XFsbl_Out32(CRL_APB_RST_LPD_TOP, RegValue);
  } break;

  case XFSBL_R5_L_TCM: {
    PwrStateMask =
        (PMU_GLOBAL_PWR_STATE_R5_0_MASK | PMU_GLOBAL_PWR_STATE_TCM0A_MASK |
         PMU_GLOBAL_PWR_STATE_TCM0B_MASK | PMU_GLOBAL_PWR_STATE_TCM1A_MASK |
         PMU_GLOBAL_PWR_STATE_TCM1B_MASK);

    Status = XFsbl_PowerUpIsland(PwrStateMask);

    if (Status != XFSBL_SUCCESS) {
      Status = XFSBL_ERROR_R5_L_TCM_POWER_UP;
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_R5_L_TCM_POWER_UP\r\n");
      goto END;
    }

    /**
     * Place R5 in lock step mode
     * Combine TCM's
     */
    RegValue = XFsbl_In32(RPU_RPU_GLBL_CNTL);
    RegValue |= RPU_RPU_GLBL_CNTL_SLCLAMP_MASK;
    RegValue &= ~(RPU_RPU_GLBL_CNTL_SLSPLIT_MASK);
    RegValue |= RPU_RPU_GLBL_CNTL_TCM_COMB_MASK;
    XFsbl_Out32(RPU_RPU_GLBL_CNTL, RegValue);

    /**
     * Place R5-0 in HALT state
     */
    RegValue = XFsbl_In32(RPU_RPU_0_CFG);
    RegValue &= ~(RPU_RPU_0_CFG_NCPUHALT_MASK);
    XFsbl_Out32(RPU_RPU_0_CFG, RegValue);

    /**
     * Place R5-1 in HALT state
     */
    RegValue = XFsbl_In32(RPU_RPU_1_CFG);
    RegValue &= ~(RPU_RPU_1_CFG_NCPUHALT_MASK);
    XFsbl_Out32(RPU_RPU_1_CFG, RegValue);

    /**
     *  Enable the clock
     */
    RegValue = XFsbl_In32(CRL_APB_CPU_R5_CTRL);
    RegValue |= CRL_APB_CPU_R5_CTRL_CLKACT_MASK;
    XFsbl_Out32(CRL_APB_CPU_R5_CTRL, RegValue);

    /**
     * Provide some delay,
     * so that clock propagates properly.
     */
    (void)usleep(0x50U);

    /**
     * Release reset to R5-0,R5-1
     */
    RegValue = XFsbl_In32(CRL_APB_RST_LPD_TOP);
    RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_R50_RESET_MASK);
    RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_R51_RESET_MASK);
    RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_AMBA_RESET_MASK);
    XFsbl_Out32(CRL_APB_RST_LPD_TOP, RegValue);
  } break;

  default:
    /* nothing to do */
    Status = XFSBL_SUCCESS;
    break;
  }

END:
  return Status;
}

/*****************************************************************************/
/**
 * This function copies the partition to specified destination
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @param	PartitionNum is the partition number in the image to be loaded
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *****************************************************************************/
u32 XFsbl_PartitionCopy(XFsblPs* const FsblInstancePtr, u32 PartitionNum) {
  u32 Status;
  u32 DestinationCpu;
  u32 CpuNo;
  u32 DestinationDevice;
  u32 ExecState;
  XFsblPs_PartitionHeader* PartitionHeader;
  u32 SrcAddress;
  PTRSIZE LoadAddress;
  u32 Length;

  /**
   * Assign the partition header to local variable
   */
  PartitionHeader = &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum];

  /**
   * Check for XIP image
   * No need to copy for XIP image
   */
  DestinationCpu = XFsbl_GetDestinationCpu(PartitionHeader);

  /**
   * if destination cpu is not present, it means it is for same cpu
   */
  if (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_NONE) {
    DestinationCpu = FsblInstancePtr->ProcessorID;
  }

  if (PartitionHeader->UnEncryptedDataWordLength == 0U) {
    /**
     * Update the Handoff address only for the first application
     * of that cpu
     * This is for XIP image. For other partitions it handoff
     * address is updated after partition validation
     */
    CpuNo = FsblInstancePtr->HandoffCpuNo;
    if (XFsbl_CheckHandoffCpu(FsblInstancePtr, DestinationCpu) ==
        XFSBL_SUCCESS) {
      /* Get the execution state */
      ExecState = XFsbl_GetA53ExecState(PartitionHeader);
      FsblInstancePtr->HandoffValues[CpuNo].CpuSettings =
          DestinationCpu | ExecState;
      FsblInstancePtr->HandoffValues[CpuNo].HandoffAddress =
          PartitionHeader->DestinationExecutionAddress;
      FsblInstancePtr->HandoffCpuNo += 1U;
    } else {
      /**
       *
       * if two partitions has same destination cpu, error can
       * be triggered here
       */
    }
    return XFSBL_SUCCESS;
  }

  /**
   * Get the source(flash offset) address where it needs to copy
   */
  SrcAddress = FsblInstancePtr->ImageOffsetAddress +
               ((PartitionHeader->DataWordOffset) * XIH_PARTITION_WORD_LENGTH);

  /**
   * Length of the partition to be copied
   */
  Length = (PartitionHeader->TotalDataWordLength) * XIH_PARTITION_WORD_LENGTH;
  DestinationDevice = XFsbl_GetDestinationDevice(PartitionHeader);

  /**
   * Copy the authentication certificate to auth. buffer
   * Update Partition length to be copied.
   * For bitstream it will be taken care saperately
   */

  LoadAddress = (PTRSIZE)PartitionHeader->DestinationLoadAddress;
  /**
   * Copy the PL to temporary DDR Address
   * Copy the PS to Load Address
   * Copy the PMU firmware to PMU RAM
   */

  if (DestinationDevice == XIH_PH_ATTRB_DEST_DEVICE_PL) {
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PL_NOT_ENABLED \r\n");
    Status = XFSBL_ERROR_PL_NOT_ENABLED;
    return Status;
  }

  /**
   * Copy the partition to PS_DDR/PL_DDR/TCM
   */
  return FsblInstancePtr->DeviceOps.DeviceCopy(SrcAddress, LoadAddress, Length);
}

#ifdef USE_CRYPTO_LIB
/*****************************************************************************/
/**
 * This function calculates checksum of the partition.
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 * @param	LoadAddress Load address of partition
 * @param	PartitionNum is the partition number to calculate checksum
 *
 * @return	returns XFSBL_SUCCESS on success
 * 		returns XFSBL_ERROR_INVALID_CHECKSUM_TYPE on failure
 *
 *****************************************************************************/
static u32 XFsbl_CalculateCheckSum(const XFsblPs* const FsblInstancePtr,
                                   PTRSIZE LoadAddress, u32 PartitionNum,
                                   u8* PartitionHash) {
  const XFsblPs_PartitionHeader* const PartitionHeader =
      &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum];
  u32 ChecksumType;

  ChecksumType = XFsbl_GetChecksumType(PartitionHeader);
  if (ChecksumType != XIH_PH_ATTRB_HASH_SHA3) {
    /* Check sum type is other than SHA3 */
    return XFSBL_ERROR_INVALID_CHECKSUM_TYPE;
  }
  XFsbl_Printf(DEBUG_INFO, "CheckSum Type - SHA3\r\n");

  /* SHA calculation in DDRful systems */
  u32 Length = PartitionHeader->TotalDataWordLength * 4U;

  /* Calculate SHA hash */
  XFsbl_ShaDigest((u8*)LoadAddress, Length, PartitionHash,
                  XFSBL_HASH_TYPE_SHA3);

  return XFSBL_SUCCESS;
}
#endif

#ifdef USE_CRYPTO_LIB
static u32 VerifyChecksum(const XFsblPs* const FsblInstancePtr,
                          u32 PartitionNum) {
  const XFsblPs_PartitionHeader* PartitionHeader =
      &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum];
  PTRSIZE LoadAddress = (PTRSIZE)PartitionHeader->DestinationLoadAddress;
  u32 Status = XFSBL_SUCCESS;

  u8 PartitionHash[XFSBL_HASH_TYPE_SHA3] __attribute__((aligned(4U))) = {0U};

  /* Checksum verification */
  if (XFsbl_GetChecksumType(PartitionHeader) != XIH_PH_ATTRB_NOCHECKSUM) {
    Status = XFsbl_CalculateCheckSum(FsblInstancePtr, LoadAddress, PartitionNum,
                                     PartitionHash);
    if (Status != XFSBL_SUCCESS) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PARTITION_CHECKSUM_FAILED \r\n");
      Status = XFSBL_ERROR_PARTITION_CHECKSUM_FAILED;
      return Status;
    }
    Status = XFsbl_ValidateCheckSum(FsblInstancePtr, LoadAddress, PartitionNum,
                                    PartitionHash);
  }

  return Status;
}
#endif

/*****************************************************************************/
/**
 * This function validates the partition
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @param	PartitionNum is the partition number in the image to be loaded
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 *****************************************************************************/
static u32 XFsbl_PartitionValidation(XFsblPs* const FsblInstancePtr,
                                     u32 PartitionNum) {
  XFsblPs_PartitionHeader* PartitionHeader =
      &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum];
  u32 Status = XFSBL_SUCCESS;

#ifdef USE_CRYPTO_LIB
  Status = VerifyChecksum(FsblInstancePtr, PartitionNum);
  if (Status != XFSBL_SUCCESS) {
    return Status;
  }
#endif

  /**
   * if destination cpu is not present, it means it is for same cpu
   */
  u32 DestinationCpu = XFsbl_GetDestinationCpu(PartitionHeader);
  if (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_NONE) {
    DestinationCpu = FsblInstancePtr->ProcessorID;
  }
  u32 DestinationDevice = XFsbl_GetDestinationDevice(PartitionHeader);

  /**
   * Update the handoff details
   */
  if ((DestinationDevice != XIH_PH_ATTRB_DEST_DEVICE_PL) &&
      (DestinationCpu != XIH_PH_ATTRB_DEST_CPU_PMU)) {
    u32 CpuNo = FsblInstancePtr->HandoffCpuNo;

    if (XFsbl_CheckHandoffCpu(FsblInstancePtr, DestinationCpu) ==
        XFSBL_SUCCESS) {
      /* Get the execution state */
      u32 ExecState = XFsbl_GetA53ExecState(PartitionHeader);
      FsblInstancePtr->HandoffValues[CpuNo].CpuSettings =
          DestinationCpu | ExecState;
      FsblInstancePtr->HandoffValues[CpuNo].HandoffAddress =
          PartitionHeader->DestinationExecutionAddress;
      FsblInstancePtr->HandoffCpuNo += 1U;
    }
  }
  return Status;
}

#ifdef USE_CRYPTO_LIB
/*****************************************************************************/
/**
 * This function validates the partition.
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 * @param	LoadAddress Load address of partition
 * @param	PartitionNum is the partition number to calculate checksum
 *
 * @return	returns XFSBL_SUCCESS on success
 * 			returns XFSBL_FAILURE on failure
 *
 *****************************************************************************/
static u32 XFsbl_ValidateCheckSum(const XFsblPs* const FsblInstancePtr,
                                  PTRSIZE LoadAddress, u32 PartitionNum,
                                  u8* PartitionHash) {
  u32 Status = XFSBL_FAILURE;
  u8 Hash[XFSBL_HASH_TYPE_SHA3] __attribute__((aligned(4U))) = {0U};

  const XFsblPs_PartitionHeader* PartitionHeader =
      &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum];
  u32 HashOffset = FsblInstancePtr->ImageOffsetAddress +
                   PartitionHeader->ChecksumWordOffset * 4U;

  Status = FsblInstancePtr->DeviceOps.DeviceCopy(HashOffset, (PTRSIZE)Hash,
                                                 XFSBL_HASH_TYPE_SHA3);
  if (Status != XFSBL_SUCCESS) {
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_HASH_COPY_FAILED\r\n");
    return Status;
  }

  u8 Index;
  for (Index = 0U; Index < XFSBL_HASH_TYPE_SHA3; Index++) {
    if (PartitionHash[Index] != Hash[Index]) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_HASH_FAILED\r\n");
      Status = XFSBL_FAILURE;
      return Status;
    }
  }

  return Status;
}
#endif

/*****************************************************************************/
/**
 * This function checks if PMU FW is loaded and gives handoff to PMU
 *Microblaze
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @param	PartitionNum is the partition number of the image
 *
 * @return	None
 *
 *****************************************************************************/
static void XFsbl_CheckPmuFw(const XFsblPs* const FsblInstancePtr,
                             u32 PartitionNum) {
  u32 DestinationCpu;
  u32 DestinationCpuNxt;
  u32 PmuFwLoadDone;
  u32 RegVal;

  DestinationCpu = XFsbl_GetDestinationCpu(
      &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum]);

  if (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_PMU) {
    if ((PartitionNum + 1U) <=
        (FsblInstancePtr->ImageHeader.ImageHeaderTable.NoOfPartitions - 1U)) {
      DestinationCpuNxt = XFsbl_GetDestinationCpu(
          &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum + 1U]);
      if (DestinationCpuNxt != XIH_PH_ATTRB_DEST_CPU_PMU) {
        /* there is a partition after this but that is
         * not PMU FW */
        PmuFwLoadDone = TRUE;
      } else {
        PmuFwLoadDone = FALSE;
      }
    } else {
      /* the current partition is last PMU FW partition */
      PmuFwLoadDone = TRUE;
    }
  } else {
    PmuFwLoadDone = FALSE;
  }

  /* If all partitions of PMU FW loaded, handoff it to PMU MicroBlaze */
  if (PmuFwLoadDone == TRUE) {
    /* Wakeup the processor */
    XFsbl_Out32(PMU_GLOBAL_GLOBAL_CNTRL,
                XFsbl_In32(PMU_GLOBAL_GLOBAL_CNTRL) | 0x1);

    /* wait until done waking up */
    do {
      RegVal = XFsbl_In32(PMU_GLOBAL_GLOBAL_CNTRL);
      if ((RegVal & PMU_GLOBAL_GLOBAL_CNTRL_FW_IS_PRESENT_MASK) ==
          PMU_GLOBAL_GLOBAL_CNTRL_FW_IS_PRESENT_MASK) {
        break;
      }
    } while (1);
  }
}

#ifdef XFSBL_BS
/*****************************************************************************/
/** Sets the library firmware state
 *
 * @param	State BS firmware state
 *
 * @return	None
 *****************************************************************************/
static void XFsbl_SetBSSecureState(u32 State) {
  u32 RegVal;

  /* Set Firmware State in PMU GLOBAL GEN STORAGE Register */
  RegVal = Xil_In32(PMU_GLOBAL_GLOB_GEN_STORAGE5);
  RegVal &= ~XFSBL_STATE_MASK;
  RegVal |= State << XFSBL_STATE_SHIFT;
  Xil_Out32(PMU_GLOBAL_GLOB_GEN_STORAGE5, RegVal);
}
#endif

#ifdef XFSBL_ENABLE_DDR_SR
/*****************************************************************************/
/**
 * This function waits for DDR out of self refresh.
 *
 * @param	None
 *
 * @return	None
 *
 *****************************************************************************/
static void XFsbl_PollForDDRSrExit(void) {
  u32 RegValue;
  /* Timeout count for around 1 second */
#  ifdef ARMR5
  u32 TimeOut = XPAR_PSU_CORTEXR5_0_CPU_CLK_FREQ_HZ;
#  else
  u32 TimeOut = XPAR_PSU_CORTEXA53_0_CPU_CLK_FREQ_HZ;
#  endif

  /* Wait for DDR exit from self refresh mode within 1 second */
  while (TimeOut > 0) {
    RegValue = Xil_In32(XFSBL_DDR_STATUS_REGISTER_OFFSET);
    if (!(RegValue & DDR_STATUS_FLAG_MASK)) {
      break;
    }
    TimeOut--;
  }
}

/*****************************************************************************/
/**
 * This function removes reserved mark of DDR once it is out of self refresh.
 *
 * @param	None
 *
 * @return	None
 *
 *****************************************************************************/
static void XFsbl_PollForDDRReady(void) {
  volatile u32 RegValue;

  RegValue = XFsbl_In32(PMU_GLOBAL_GLOBAL_CNTRL);
  if ((RegValue & PMU_GLOBAL_GLOBAL_CNTRL_FW_IS_PRESENT_MASK) ==
      PMU_GLOBAL_GLOBAL_CNTRL_FW_IS_PRESENT_MASK) {
    /*
     * PMU firmware is ready. Set flag to indicate that DDR
     * controller is ready, so that the PMU may bring the DDR out
     * of self refresh if necessary.
     */
    RegValue = Xil_In32(XFSBL_DDR_STATUS_REGISTER_OFFSET);
    Xil_Out32(XFSBL_DDR_STATUS_REGISTER_OFFSET, RegValue | DDRC_INIT_FLAG_MASK);

    /*
     * Read PMU register bit value that indicates DDR is in self
     * refresh mode.
     */
    RegValue =
        Xil_In32(XFSBL_DDR_STATUS_REGISTER_OFFSET) & DDR_STATUS_FLAG_MASK;
    if (RegValue) {
      /* Wait until DDR exits from self refresh */
      XFsbl_PollForDDRSrExit();
      /*
       * Mark DDR region as "Memory" as DDR initialization is
       * done
       */
      XFsbl_MarkDdrAsReserved(FALSE);
    }
  }
}
#endif
