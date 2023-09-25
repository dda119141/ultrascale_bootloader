/******************************************************************************
 * Copyright (c) 2015 - 2021 Xilinx, Inc.  All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file xfsbl_initilization.c
 *
 * This is the file which contains initialization code for the FSBL.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date        Changes
 * ----- ---- -------- -------------------------------------------------------
 * 1.00  kc   10/21/13 Initial release
 * 2.00  sg   13/03/15 Added QSPI 32Bit bootmode
 * 3.0   bv   12/02/16 Made compliance to MISRAC 2012 guidelines
 *            12/08/16 Added PL clear at initialization based on user
 *                     configuration
 *            01/25/17 Updated R5 TCM with lovec value in XFsbl_ProcessorInit
 *                     and XFsbl_TcmEccInit is been updated such that R5_L and
 *                     R5_0 don't initialize initial 32 bytes of TCM as they
 *                     are holding R5 vectors
 *       bv   01/29/17 Added USB boot mode initializations
 *            02/11/17 Add APU only reset code.
 *       vns  02/17/17 Added image header authentication
 *       bv   03/17/17 Based on reset reason initializations of system, tcm etc
 *                     is done.
 *       vns  04/04/17 Corrected image header size.
 *       ma   05/10/17 Enable PROG to PL when reset reason is ps-only reset
 * 4.0   vns  03/07/18 Added boot header authentication, attributes reading
 *                     from boot header local buffer, copying IV to global
 *                     variable for using during decryption of partition.
 * 5.0   mn   07/06/18 Add DDR initialization support for new DDR DIMM part
 *       mus  02/26/19 Added support for armclang compiler
 *       vns  03/14/19 Setting AES and SHA hardware engines into reset.
 * 6.0   bsv  08/27/19 Added check to ensure padding in image header does not
 *                     exceed allotted buffer in OCM
 * 7.0   bsv  03/05/20 Restore value of SD_CDN_CTRL register before handoff
 *       ma   03/19/20 Update the status of FSBL image encryption in PMU Global
 *                     register
 * 8.0   bsv  12/16/20 Update print format in XFsbl_EccInit to correctly print
 *                     64 bit addresses and lengths
 *       bsv  04/01/21 Added TPM support
 *       bsv  04/28/21 Added support to ensure authenticated images boot as
 *                     non-secure when RSA_EN is not programmed
 *       bsv  06/10/21 Mark DDR as memory just after ECC initialization to
 *                     avoid speculative accesses
 * 9.0   bsv  10/15/21 Fixed bug to support secondary boot with non-zero
 *                     multiboot offset
 *
 * </pre>
 *
 * @note
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "xfsbl_board.h"
#include "xfsbl_ddr_init.h"
#include "xfsbl_hooks.h"
#include "xfsbl_hw.h"
#include "xfsbl_main.h"
#include "xfsbl_misc_drivers.h"
#include "xfsbl_qspi.h"
#include "xil_cache.h"
#include "xil_mmu.h"

/************************** Constant Definitions *****************************/
#define PART_NAME_LEN_MAX 20U
#define XFSBL_APU_RESET_MASK (1U << 16U)
#define XFSBL_APU_RESET_BIT 16U

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/
static u32 XFsbl_ProcessorInit(XFsblPs* FsblInstancePtr);
static u32 XFsbl_ResetValidation(void);
static u32 XFsbl_SystemInit(XFsblPs* const FsblInstancePtr);
static u32 XFsbl_PrimaryBootDeviceInit(XFsblPs* const FsblInstancePtr);
static u32 retrieveImageHeaderTable(XFsblPs* const FsblInstancePtr);
static u32 retrieveBootHeader(XFsblPs* const FsblInstancePtr);
#ifdef XFSBL_SECURE
static u32 XFsbl_ValidateHeader(XFsblPs* FsblInstancePtr);
#endif
static u32 XFsbl_DdrEccInit(void);
static void XFsbl_EnableProgToPL(void);
static void XFsbl_ClearPendingInterrupts(void);

/* Functions from xfsbl_misc.c */

/**
 * Functions from xfsbl_misc.c
 */
void XFsbl_RegisterHandlers(void);

/**
 *Functions from xfsbl_qspi.c
 */

/************************** Variable Definitions *****************************/
extern XFsblPs FsblInstance;

u8 ReadBuffer[XFSBL_SIZE_IMAGE_HDR] = {0};

#ifdef XFSBL_SECURE
u8* ImageHdr = ReadBuffer;
extern u8 AuthBuffer[XFSBL_AUTH_BUFFER_SIZE];
extern u32 Iv[XIH_BH_IV_LENGTH / 4U];
#endif
u32 SdCdnRegVal;

static void XFsbl_PrintFsblBanner(void) {
  s32 PlatInfo;
  /**
   * Print the FSBL Banner
   */
#if !defined(XFSBL_PERF) || defined(FSBL_DEBUG) || defined(FSBL_DEBUG_INFO) || \
    defined(FSBL_DEBUG_DETAILED)
  XFsbl_Printf(DEBUG_PRINT_ALWAYS,
               "Xilinx Zynq MP First Stage Boot Loader \n\r");

  /*
  XFsbl_Printf(DEBUG_GENERAL, "MultiBootOffset: 0x%0x\r\n",
               XFsbl_In32(CSU_CSU_MULTI_BOOT));
*/

  if (FsblInstance.ResetReason == XFSBL_PS_ONLY_RESET) {
    XFsbl_Printf(DEBUG_GENERAL, "Reset Mode	:	PS Only Reset\r\n");
  } else if (XFSBL_MASTER_ONLY_RESET == FsblInstance.ResetReason) {
    XFsbl_Printf(DEBUG_GENERAL,
                 "Reset Mode	:	Master Subsystem Only Reset\r\n");
  } else if (FsblInstance.ResetReason == XFSBL_SYSTEM_RESET) {
    XFsbl_Printf(DEBUG_GENERAL, "Reset Mode	:	System Reset\r\n");
  } else {
    /*MISRAC compliance*/
  }
#endif

  /**
   * Print the platform
   */

  PlatInfo = (s32)XGet_Zynq_UltraMp_Platform_info();
  if (PlatInfo == XPLAT_ZYNQ_ULTRA_MPQEMU) {
    XFsbl_Printf(DEBUG_GENERAL, "Platform: QEMU, ");
  } else if (PlatInfo == XPLAT_ZYNQ_ULTRA_MP) {
    XFsbl_Printf(DEBUG_GENERAL, "Platform: REMUS, ");
  } else if (PlatInfo == XPLAT_ZYNQ_ULTRA_MP_SILICON) {
    XFsbl_Printf(DEBUG_GENERAL, "Platform: Silicon (%d.0), ",
                 XGetPSVersion_Info() + 1U);
  } else {
    XFsbl_Printf(DEBUG_GENERAL, "Platform Not identified \r\n");
  }

  return;
}

/****************************************************************************/
/**
 * This function is used to get the Reset Reason
 *
 * @param  None
 *
 * @return Reset Reason
 *
 * @note
 *
 *****************************************************************************/
static u32 XFsbl_GetResetReason(void) {
  u32 Val;
  u32 Ret;

  Val = XFsbl_In32(CRL_APB_RESET_REASON);

  if ((Val & CRL_APB_RESET_REASON_PSONLY_RESET_REQ_MASK) != 0U) {
    /* Clear the PS Only reset bit as it is sticky */
    Val = CRL_APB_RESET_REASON_PSONLY_RESET_REQ_MASK;
    XFsbl_Out32(CRL_APB_RESET_REASON, Val);
    Ret = XFSBL_PS_ONLY_RESET;
    //		XFsbl_SaveData();
  } else {
    Ret = (XFsbl_In32(PMU_GLOBAL_GLOB_GEN_STORAGE4) & XFSBL_APU_RESET_MASK) >>
          (XFSBL_APU_RESET_BIT);
  }

  return Ret;
}

/*****************************************************************************/
/**
 * This function is initializes the processor and system.
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return
 *          - returns the error codes described in xfsbl_error.h on any error
 * 			- returns XFSBL_SUCCESS on success
 *
 *****************************************************************************/
u32 XFsbl_Initialize(XFsblPs* const FsblInstancePtr) {
  u32 Status;

  /**
   * Place AES and SHA engines in reset
   */
  XFsbl_Out32(CSU_AES_RESET, CSU_AES_RESET_RESET_MASK);
  XFsbl_Out32(CSU_SHA_RESET, CSU_SHA_RESET_RESET_MASK);

  FsblInstancePtr->ResetReason = XFsbl_GetResetReason();

  /*
   * Enables the propagation of the PROG signal to PL
   */
  if (FsblInstancePtr->ResetReason == XFSBL_PS_ONLY_RESET) {
    XFsbl_EnableProgToPL();
  }

  /**
   * Configure the system as in PSU
   */
  if (XFSBL_MASTER_ONLY_RESET != FsblInstancePtr->ResetReason) {
    Status = XFsbl_SystemInit(FsblInstancePtr);
    if (XFSBL_SUCCESS != Status) {
      return Status;
    }
  }

  /**
   * Place AES and SHA engines in reset
   */
  XFsbl_Out32(CSU_AES_RESET, CSU_AES_RESET_RESET_MASK);
  XFsbl_Out32(CSU_SHA_RESET, CSU_SHA_RESET_RESET_MASK);

  /**
   * Print the FSBL banner
   */
  XFsbl_PrintFsblBanner();

  /* Initialize the processor */
  Status = XFsbl_ProcessorInit(FsblInstancePtr);
  if (XFSBL_SUCCESS != Status) {
    return Status;
  }

  if (XFSBL_MASTER_ONLY_RESET == FsblInstancePtr->ResetReason) {
    if (FsblInstancePtr->ProcessorID == XIH_PH_ATTRB_DEST_CPU_A53_0) {
      /* APU only restart with pending interrupts can cause
       * the linux to hang when it starts the second time. So
       * FSBL clears all pending interrupts in case of APU
       * only restart.
       */
      XFsbl_ClearPendingInterrupts();
    }
  }

  if (XFSBL_MASTER_ONLY_RESET != FsblInstancePtr->ResetReason) {
    /* Do ECC Initialization of DDR if required */
    Status = XFsbl_DdrEccInit();
    if (XFSBL_SUCCESS != Status) {
      return Status;
    }
    XFsbl_MarkDdrAsReserved(FALSE);

    /* Do board specific initialization if any */
    Status = XFsbl_BoardInit();
    if (XFSBL_SUCCESS != Status) {
      return Status;
    }

    /**
     * Validate the reset reason
     */
    Status = XFsbl_ResetValidation();
    if (XFSBL_SUCCESS != Status) {
      return Status;
    }
  }

  XFsbl_Printf(DEBUG_INFO, "Processor Initialization Done \n\r");

  return XFSBL_SUCCESS;
}

/*****************************************************************************/
/**
 * This function initializes the primary and secondary boot devices
 * and validates the image header
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 ******************************************************************************/
u32 XFsbl_BootDeviceInit(XFsblPs* const FsblInstancePtr) {
  u32 Status;

  /**
   * Configure the primary boot device
   */
  Status = XFsbl_PrimaryBootDeviceInit(FsblInstancePtr);
  XFsbl_Printf(DEBUG_INFO, "Primary device status 0x%0lx\n\r", Status);
  if (XFSBL_SUCCESS != Status) {
    return Status;
  }

  /**
   * Retrieve Boot header
   */
  Status = retrieveBootHeader(FsblInstancePtr);
  XFsbl_Printf(DEBUG_INFO, "retrieve header status 0x%0lx\n\r", Status);
  if (XFSBL_SUCCESS != Status) {
    return Status;
  }

  /**
   * Retrieve Image header table
   */
  Status = retrieveImageHeaderTable(FsblInstancePtr);
  XFsbl_Printf(DEBUG_INFO, "Image header table status 0x%0lx\n\r", Status);
  if (XFSBL_SUCCESS != Status) {
    return Status;
  }

  return XFSBL_SUCCESS;
}

/*****************************************************************************/
/**
 * This function enables the propagation of the PROG signal to PL after
 * PS-only reset
 *
 * @param	None
 *
 * @return	None
 *
 ******************************************************************************/
void XFsbl_EnableProgToPL(void) {
  u32 RegVal = 0x0U;

  /*
   * PROG control to PL.
   */
  Xil_Out32(CSU_PCAP_PROG, CSU_PCAP_PROG_PCFG_PROG_B_MASK);

  /*
   * Enable the propagation of the PROG signal to the PL after PS-only
   * reset
   * */
  RegVal = XFsbl_In32(PMU_GLOBAL_PS_CNTRL);

  RegVal &= ~(PMU_GLOBAL_PS_CNTRL_PROG_GATE_MASK);
  RegVal |= (PMU_GLOBAL_PS_CNTRL_PROG_ENABLE_MASK);

  Xil_Out32(PMU_GLOBAL_PS_CNTRL, RegVal);
}

/*****************************************************************************/
/**
 * This function initializes the processor and updates the cluster id
 * which indicates CPU on which fsbl is running
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_ProcessorInit(XFsblPs* FsblInstancePtr) {
  u32 Status;
  PTRSIZE ClusterId;
  u32 FsblProcType = 0;
  char DevName[PART_NAME_LEN_MAX];

  /**
   * Read the cluster ID and Update the Processor ID
   * Initialize the processor settings that are not done in
   * BSP startup code
   */
#ifdef ARMA53_64
  ClusterId = mfcp(MPIDR_EL1);
#else
  ClusterId = mfcp(XREG_CP15_MULTI_PROC_AFFINITY);
#endif

  XFsbl_Printf(DEBUG_INFO, "Cluster ID 0x%0lx\n\r", ClusterId);

  if (XGet_Zynq_UltraMp_Platform_info() == (u32)XPLAT_ZYNQ_ULTRA_MPQEMU) {
    /**
     * Remmaping for R5 in QEMU
     */
    if (ClusterId == 0x80000004U) {
      ClusterId = 0xC0000100U;
    } else if (ClusterId == 0x80000005U) {
      /* this corresponds to R5-1 */
      Status = XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID;
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID\n\r");
      goto END;
    } else {
      /* For MISRA C compliance */
    }
  }

  /* store the processor ID based on the cluster ID */
  if ((ClusterId & XFSBL_CLUSTER_ID_MASK) == XFSBL_A53_PROCESSOR) {
    XFsbl_Printf(DEBUG_GENERAL, "Running on A53-0 ");
    FsblInstancePtr->ProcessorID = XIH_PH_ATTRB_DEST_CPU_A53_0;
    FsblProcType = XFSBL_RUNNING_ON_A53 << XFSBL_STATE_PROC_SHIFT;

    /* Running on A53 64-bit */
    XFsbl_Printf(DEBUG_GENERAL, "(64-bit) Processor");
    FsblInstancePtr->A53ExecState = XIH_PH_ATTRB_A53_EXEC_ST_AA64;
  } else {
    Status = XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID\n\r");
    goto END;
  }

  /*
   * Update FSBL processor information to PMU Global Reg5
   * as PMU require this during boot for warm-restart feature.
   */
  FsblProcType |= (XFsbl_In32(PMU_GLOBAL_GLOB_GEN_STORAGE5) &
                   ~(XFSBL_STATE_PROC_INFO_MASK));

  XFsbl_Out32(PMU_GLOBAL_GLOB_GEN_STORAGE5, FsblProcType);

  /* Build Device name and print it */
  (void)XFsbl_Strcpy(DevName, "XCZU");
  (void)XFsbl_Strcat(DevName, XFsbl_GetSiliconIdName());
  (void)XFsbl_Strcat(DevName, XFsbl_GetProcEng());
  XFsbl_Printf(DEBUG_GENERAL, ", Device Name: %s\n\r", DevName);

  /**
   * Register the exception handlers
   */
  XFsbl_RegisterHandlers();

  Status = XFSBL_SUCCESS;
END:
  return Status;
}

/*****************************************************************************/
/**
 * This function validates the reset reason
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/

static u32 XFsbl_ResetValidation(void) {
  u32 Status;
  u32 FsblErrorStatus;
  /**
   *  Read the Error Status register
   *  If WDT reset, do fallback
   */
  FsblErrorStatus = XFsbl_In32(XFSBL_ERROR_STATUS_REGISTER_OFFSET);

  /**
   * Mark FSBL running in error status register to
   * detect the WDT reset while FSBL execution
   */
  if (FsblErrorStatus != XFSBL_RUNNING) {
    XFsbl_Out32(XFSBL_ERROR_STATUS_REGISTER_OFFSET, XFSBL_RUNNING);
  }

  /**
   *  Read system error status register
   * 	provide FsblHook function for any action
   */

  Status = XFSBL_SUCCESS;

  return Status;
}

/*****************************************************************************/
/**
 * This function initializes the system using the psu_init()
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_SystemInit(XFsblPs* const FsblInstancePtr) {
  u32 Status;

  if (FsblInstancePtr->ResetReason != XFSBL_PS_ONLY_RESET) {
    /**
     * MIO33 can be used to control power to PL through PMU.
     * For 1.0 and 2.0 Silicon, a workaround is needed to Powerup PL
     * before MIO33 is configured. Hence, before MIO configuration,
     * Powerup PL (but restore isolation).
     */
    if (XGetPSVersion_Info() <= (u32)XPS_VERSION_2) {
      Status = XFsbl_PowerUpIsland(PMU_GLOBAL_PWR_STATE_PL_MASK);

      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_PL_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PL_POWER_UP\r\n");
        goto END;
      }

      /* For PS only reset, make sure FSBL exits with
       * isolation removed */

      Status =
          XFsbl_IsolationRestore(PMU_GLOBAL_REQ_ISO_INT_EN_PL_NONPCAP_MASK);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_PMU_GLOBAL_REQ_ISO;
        XFsbl_Printf(DEBUG_GENERAL,
                     "XFSBL_ERROR_PMU_GLOBAL_REQ_ISO_"
                     "INT_EN_PL\r\n");
        goto END;
      }
    }
  } else if (XFSBL_MASTER_ONLY_RESET == FsblInstancePtr->ResetReason) {
    /*Do nothing*/
  } else {
    /**
     * PMU-fw applied AIB between ps and pl only while ps only
     * reset. Remove the isolation so as to access pl again
     */
    u32 reg = XFsbl_In32(PMU_GLOBAL_AIB_STATUS);
    while (reg) {
      /* Unblock the FPD and LPD AIB for PS only reset*/
      XFsbl_Out32(PMU_GLOBAL_AIB_CNTRL, 0U);
      reg = XFsbl_In32(PMU_GLOBAL_AIB_STATUS);
    }
  }

  /**
   * psu initialization
   */
  Status = XFsbl_HookPsuInit();
  if (XFSBL_SUCCESS != Status) {
    goto END;
  }

#if 0
	/*
	 * This function is used for all the ZynqMP boards.
	 * This function initialize the DDR by fetching the SPD data from
	 * EEPROM. This function will determine the type of the DDR and decode
	 * the SPD structure accordingly. The SPD data is used to calculate the
	 * register values of DDR controller and DDR PHY.
	 */
	Status = XFsbl_DdrInit();
	if (XFSBL_SUCCESS != Status) {
		XFsbl_Printf(DEBUG_GENERAL, "XFSBL_DDR_INIT_FAILED\n\r");
		goto END;
	}
#endif
  /**
   * Forcing the SD card detection signal to bypass the debouncing logic.
   * This will ensure that SD controller doesn't end up waiting for long,
   * fixed durations for card to be stable.
   */
  SdCdnRegVal = XFsbl_In32(IOU_SLCR_SD_CDN_CTRL);
  XFsbl_Out32(IOU_SLCR_SD_CDN_CTRL, (IOU_SLCR_SD_CDN_CTRL_SD1_CDN_CTRL_MASK |
                                     IOU_SLCR_SD_CDN_CTRL_SD0_CDN_CTRL_MASK));

  /**
   * Poweroff the unused blocks as per PSU
   */

END:
  return Status;
}

/*****************************************************************************/
/**
 * This function initializes the primary boot device
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_PrimaryBootDeviceInit(XFsblPs* const FsblInstancePtr) {
  u32 Status;
  u32 BootMode;

  /**
   * Read Boot Mode register and update the value
   */
  BootMode = XFsbl_In32(CRL_APB_BOOT_MODE_USER) &
             CRL_APB_BOOT_MODE_USER_BOOT_MODE_MASK;

  FsblInstancePtr->PrimaryBootDevice = BootMode;

  switch (BootMode) {
  /**
   * For JTAG boot mode, it will be in while loop
   */
  case XFSBL_JTAG_BOOT_MODE: {
    XFsbl_Printf(DEBUG_GENERAL, "In JTAG Boot Mode \n\r");
    Status = XFSBL_STATUS_JTAG;
  } break;

  case XFSBL_QSPI24_BOOT_MODE: {
    XFsbl_Printf(DEBUG_GENERAL, "QSPI 24bit Boot Mode \n\r");

    FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_Qspi24Init;
    FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_Qspi24Copy;
    FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_Qspi24Release;
    Status = XFSBL_SUCCESS;
  } break;

  case XFSBL_QSPI32_BOOT_MODE: {
    XFsbl_Printf(DEBUG_GENERAL, "QSPI 32 bit Boot Mode \n\r");

    FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_Qspi32Init;
    FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_Qspi32Copy;
    FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_Qspi32Release;
    Status = XFSBL_SUCCESS;
  } break;

  case XFSBL_SD0_BOOT_MODE:
  case XFSBL_EMMC_BOOT_MODE: {
    if (BootMode == XFSBL_SD0_BOOT_MODE) {
      XFsbl_Printf(DEBUG_GENERAL, "SD0 Boot Mode \n\r");
    } else {
      XFsbl_Printf(DEBUG_GENERAL, "eMMC Boot Mode \n\r");
    }
#ifdef XFSBL_SD_0
    /**
     * Update the deviceops structure with necessary values
     */
    FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_SdInit;
    FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_SdCopy;
    FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_SdRelease;
    Status = XFSBL_SUCCESS;
#else
    /**
     * This bootmode is not supported in this release
     */
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
    Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
#endif
  } break;

  case XFSBL_SD1_BOOT_MODE:
  case XFSBL_SD1_LS_BOOT_MODE: {
    if (BootMode == XFSBL_SD1_BOOT_MODE) {
      XFsbl_Printf(DEBUG_GENERAL, "SD1 Boot Mode \n\r");
    } else {
      XFsbl_Printf(DEBUG_GENERAL, "SD1 with level shifter Boot Mode \n\r");
    }
#ifdef XFSBL_SD_1
    /**
     * Update the deviceops structure with necessary values
     */
    FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_SdInit;
    FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_SdCopy;
    FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_SdRelease;
    Status = XFSBL_SUCCESS;
#else
    /**
     * This bootmode is not supported in this release
     */
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
    Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
#endif
  } break;

  default: {
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
    Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
  } break;
  }

  /**
   * In case of error or Jtag boot, goto end
   */
  if (XFSBL_SUCCESS != Status) {
    goto END;
  }

  /**
   * Initialize the Device Driver
   */
  Status = FsblInstancePtr->DeviceOps.DeviceInit(BootMode);
  if (XFSBL_SUCCESS != Status) {
    goto END;
  }

END:
  return Status;
}

static u32 retrieveBootHeader(XFsblPs* const FsblInstancePtr) {
  u32 FlashImageOffsetAddress;
  u32 Status;
  u32 MultiBootOffset;

  /**
   * Read the Multiboot Register
   */
  MultiBootOffset = XFsbl_In32(CSU_CSU_MULTI_BOOT);
  /*
          XFsbl_Printf(DEBUG_INFO, "Multiboot Reg : 0x%0lx \n\r",
               MultiBootOffset);
  */

  /**
   *  Calculate the Flash Offset Address
   *  For file system based devices, Flash Offset Address should be 0 always
   */
  if ((FsblInstancePtr->SecondaryBootDevice == 0U) &&
      (!((FsblInstancePtr->PrimaryBootDevice == XFSBL_SD0_BOOT_MODE) ||
         (FsblInstancePtr->PrimaryBootDevice == XFSBL_EMMC_BOOT_MODE) ||
         (FsblInstancePtr->PrimaryBootDevice == XFSBL_SD1_BOOT_MODE) ||
         (FsblInstancePtr->PrimaryBootDevice == XFSBL_SD1_LS_BOOT_MODE) ||
         (FsblInstancePtr->PrimaryBootDevice == XFSBL_USB_BOOT_MODE)))) {
    FsblInstancePtr->ImageOffsetAddress =
        MultiBootOffset * XFSBL_IMAGE_SEARCH_OFFSET;
  } else {
    FsblInstancePtr->ImageOffsetAddress = 0U;
  }

  FsblInstancePtr->ImageOffsetAddress = 0U;
  FlashImageOffsetAddress = FsblInstancePtr->ImageOffsetAddress;

  /* Copy boot header to internal memory */
  Status = FsblInstancePtr->DeviceOps.DeviceCopy(
      FlashImageOffsetAddress, (PTRSIZE)ReadBuffer, XIH_BH_MAX_SIZE);
  if (XFSBL_SUCCESS != Status) {
    XFsbl_Printf(DEBUG_GENERAL, "Device Copy Failed \n\r");
    goto END;
  }

  xil_printf("*** Boot header copy successful *** \n\r");
  /**
   * Read Boot Image attributes
   */
  FsblInstancePtr->BootHdrAttributes =
      Xil_In32((UINTPTR)ReadBuffer + XIH_BH_IMAGE_ATTRB_OFFSET);

END:
  return Status;
}

static u32 retrieveImageHeaderTable(XFsblPs* const FsblInstancePtr) {
  u32 Status;
  u32 ImageHeaderTableAddressOffset = 0U;

  ImageHeaderTableAddressOffset =
      Xil_In32((UINTPTR)ReadBuffer + XIH_BH_IH_TABLE_OFFSET);

  XFsbl_Printf(DEBUG_INFO, "Image Header Table Offset 0x%0lx \n\r",
               ImageHeaderTableAddressOffset);

  XFsbl_Printf(DEBUG_INFO,
               "Image Header Table raw 0x%0x 0x%0x 0x%0x 0x%0x \n\r",
               ReadBuffer[XIH_BH_IH_TABLE_OFFSET],
               ReadBuffer[XIH_BH_IH_TABLE_OFFSET + 1],
               ReadBuffer[XIH_BH_IH_TABLE_OFFSET + 2],
               ReadBuffer[XIH_BH_IH_TABLE_OFFSET + 3]);

  /* Read Image Header Table */
  Status = XFsbl_ReadImageHeader(
      &FsblInstancePtr->ImageHeader, &FsblInstancePtr->DeviceOps,
      FsblInstancePtr->ImageOffsetAddress, FsblInstancePtr->ProcessorID,
      ImageHeaderTableAddressOffset);
  if (XFSBL_SUCCESS != Status) {
    goto END;
  }

END:
  return Status;
}

#ifdef XFSBL_SECURE
/*****************************************************************************/
/**
 * This function validates the image header
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_ValidateHeader(XFsblPs* FsblInstancePtr) {
  u32 Status = 0U;
  u32 BootHdrAttrb = 0U;
  u32 EfuseCtrl;
  u32 Size;
  u32 AcOffset = 0U;

  BootHdrAttrb = Xil_In32((UINTPTR)ReadBuffer + XIH_BH_IMAGE_ATTRB_OFFSET);

  /**
   * Read Efuse bit and check Boot Header for Authentication
   */
  EfuseCtrl = XFsbl_In32(EFUSE_SEC_CTRL);

  if (((EfuseCtrl & EFUSE_SEC_CTRL_RSA_EN_MASK) != 0x00) &&
      ((BootHdrAttrb & XIH_BH_IMAGE_ATTRB_RSA_MASK) ==
       XIH_BH_IMAGE_ATTRB_RSA_MASK)) {
    Status = XFSBL_ERROR_BH_AUTH_IS_NOTALLOWED;
    XFsbl_Printf(DEBUG_GENERAL,
                 "XFSBL_ERROR_BH_AUTH_IS_NOTALLOWED"
                 " when eFSUE RSA bit is set \n\r");
    goto END;
  }

  /* If authentication is enabled */
  if (((EfuseCtrl & EFUSE_SEC_CTRL_RSA_EN_MASK) != 0U) ||
      ((BootHdrAttrb & XIH_BH_IMAGE_ATTRB_RSA_MASK) ==
       XIH_BH_IMAGE_ATTRB_RSA_MASK)) {
    FsblInstancePtr->AuthEnabled = TRUE;
    XFsbl_Printf(DEBUG_INFO, "Authentication Enabled\r\n");

    /* Read AC offset from Image header table */
    Status = FsblInstancePtr->DeviceOps.DeviceCopy(
        FlashImageOffsetAddress + ImageHeaderTableAddressOffset +
            XIH_IHT_AC_OFFSET,
        (PTRSIZE)&AcOffset, XIH_FIELD_LEN);
    if (XFSBL_SUCCESS != Status) {
      XFsbl_Printf(DEBUG_GENERAL, "Device Copy Failed \n\r");
      goto END;
    }
    if (AcOffset != 0x00U) {
      /* Authentication exists copy AC to OCM */
      Status = FsblInstancePtr->DeviceOps.DeviceCopy(
          (FsblInstancePtr->ImageOffsetAddress +
           (AcOffset * XIH_PARTITION_WORD_LENGTH)),
          (INTPTR)AuthBuffer, XFSBL_AUTH_CERT_MIN_SIZE);
      if (XFSBL_SUCCESS != Status) {
        goto END;
      }

      /* Authenticate boot header */
      /* When eFUSE RSA enable bit is blown */
      if ((EfuseCtrl & EFUSE_SEC_CTRL_RSA_EN_MASK) != 0U) {
        Status = XFsbl_BhAuthentication(FsblInstancePtr, ReadBuffer,
                                        (PTRSIZE)AuthBuffer, TRUE);
      }
      /* When eFUSE RSA bit is not blown */
      else {
        Status = XFsbl_BhAuthentication(FsblInstancePtr, ReadBuffer,
                                        (PTRSIZE)AuthBuffer, FALSE);
      }
      if (Status != XST_SUCCESS) {
        XFsbl_Printf(DEBUG_GENERAL,
                     "Failure at boot header authentication\r\n");
        goto END;
      }

      /* Authenticate Image header table */
      /*
       * Total size of Image header may vary
       * depending on padding so
       * size = AC address - Start address;
       */
      Size = (AcOffset * XIH_PARTITION_WORD_LENGTH) -
             (ImageHeaderTableAddressOffset);
      if (Size > sizeof(ReadBuffer)) {
        Status = XFSBL_ERROR_IMAGE_HEADER_SIZE;
        goto END;
      }

      /* Copy the Image header to OCM */
      Status = FsblInstancePtr->DeviceOps.DeviceCopy(
          FsblInstancePtr->ImageOffsetAddress + ImageHeaderTableAddressOffset,
          (INTPTR)ImageHdr, Size);
      if (Status != XFSBL_SUCCESS) {
        goto END;
      }

      /* Authenticate the image header */
      Status = XFsbl_Authentication(FsblInstancePtr, (PTRSIZE)ImageHdr,
                                    Size + XFSBL_AUTH_CERT_MIN_SIZE,
                                    (PTRSIZE)(AuthBuffer), 0x00U);
      if (Status != XFSBL_SUCCESS) {
        XFsbl_Printf(DEBUG_GENERAL,
                     "Failure at image header"
                     " table authentication\r\n");
        goto END;
      }
      /*
       * As authentication is success
       * verify ACoffset used for authentication
       */
      if (AcOffset != Xil_In32((UINTPTR)ImageHdr + XIH_IHT_AC_OFFSET)) {
        Status = XFSBL_ERROR_IMAGE_HEADER_ACOFFSET;
        XFsbl_Printf(DEBUG_GENERAL,
                     "Wrong Authentication "
                     "certificate offset\r\n");
        goto END;
      }
    } else {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_IMAGE_HEADER_ACOFFSET\r\n");
      Status = XFSBL_ERROR_IMAGE_HEADER_ACOFFSET;
      goto END;
    }
  } else {
  }

END:
  return Status;
}
#endif

/*****************************************************************************/
/**
 * This function does ECC Initialization of DDR memory
 *
 * @param none
 *
 * @return
 * 		- XFSBL_SUCCESS for successful ECC Initialization
 * 		-               or ECC is not enabled for DDR
 * 		- errors as mentioned in xfsbl_error.h
 *
 *****************************************************************************/
static u32 XFsbl_DdrEccInit(void) {
  u32 Status;
#if XPAR_PSU_DDRC_0_HAS_ECC
  u64 LengthBytes =
      (XFSBL_PS_DDR_END_ADDRESS - XFSBL_PS_DDR_INIT_START_ADDRESS) + 1;
  u64 DestAddr = XFSBL_PS_DDR_INIT_START_ADDRESS;

  XFsbl_Printf(DEBUG_GENERAL, "Initializing DDR ECC\n\r");

  Status = XFsbl_EccInit(DestAddr, LengthBytes);
  if (XFSBL_SUCCESS != Status) {
    Status = XFSBL_ERROR_DDR_ECC_INIT;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_DDR_ECC_INIT\n\r");
    goto END;
  }

  /* If there is upper PS DDR, initialize its ECC */
#  ifdef XFSBL_PS_HI_DDR_START_ADDRESS
  LengthBytes =
      (XFSBL_PS_HI_DDR_END_ADDRESS - XFSBL_PS_HI_DDR_START_ADDRESS) + 1;
  DestAddr = XFSBL_PS_HI_DDR_START_ADDRESS;

  Status = XFsbl_EccInit(DestAddr, LengthBytes);
  if (XFSBL_SUCCESS != Status) {
    Status = XFSBL_ERROR_DDR_ECC_INIT;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_DDR_ECC_INIT\n\r");
    goto END;
  }
#  endif
END:
#else
  Status = XFSBL_SUCCESS;
#endif
  return Status;
}

/*****************************************************************************/
/**
 * This function clears pending interrupts. This is called only during APU only
 *  reset.
 *
 * @param
 *
 * @return
 *
 *
 *****************************************************************************/
static void XFsbl_ClearPendingInterrupts(void) {
  u32 InterruptClearVal = 0xFFFFFFFFU;
  /* Clear pending peripheral interrupts */

  XFsbl_Out32(ACPU_GIC_GICD_ICENBLR0, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICPENDR0, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICACTIVER0, InterruptClearVal);

  XFsbl_Out32(ACPU_GIC_GICD_ICENBLR1, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICPENDR1, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICACTIVER1, InterruptClearVal);

  XFsbl_Out32(ACPU_GIC_GICD_ICENBLR2, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICPENDR2, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICACTIVER2, InterruptClearVal);

  XFsbl_Out32(ACPU_GIC_GICD_ICENBLR3, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICPENDR3, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICACTIVER3, InterruptClearVal);

  XFsbl_Out32(ACPU_GIC_GICD_ICENBLR4, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICPENDR4, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICACTIVER4, InterruptClearVal);

  XFsbl_Out32(ACPU_GIC_GICD_ICENBLR5, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICPENDR5, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_ICACTIVER5, InterruptClearVal);

  /* Clear active software generated interrupts, if any */
  u32 RegVal = XFsbl_In32(ACPU_GIC_GICD_INTR_ACK_REG);
  XFsbl_Out32(ACPU_GIC_GICD_END_INTR_REG, RegVal);

  /* Clear pending software generated interrupts */

  XFsbl_Out32(ACPU_GIC_GICD_CPENDSGIR0, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_CPENDSGIR1, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_CPENDSGIR2, InterruptClearVal);
  XFsbl_Out32(ACPU_GIC_GICD_CPENDSGIR3, InterruptClearVal);
}

/*****************************************************************************/
/**
 * This function marks DDR region as "Reserved" or mark as "Memory".
 *
 * @param Cond is the condition to mark DDR region as Reserved or Memory. If
 *	  this parameter is TRUE it marks DDR region as Reserved and if it is
 *	  FALSE it marks DDR as Memory.
 *
 * @return
 *
 *
 *****************************************************************************/
void XFsbl_MarkDdrAsReserved(u8 Cond) {
#if defined(XPAR_PSU_DDR_0_S_AXI_BASEADDR) && !defined(ARMR5)
  u32 Attrib = ATTRIB_MEMORY_A53_64;
  u64 BlockNum;

  if (TRUE == Cond) {
    Attrib = ATTRIB_RESERVED_A53;
  }

#  ifdef ARMA53_64
  /* For A53 64bit*/
  for (BlockNum = 0; BlockNum < NUM_BLOCKS_A53_64; BlockNum++) {
    XFsbl_SetTlbAttributes(BlockNum * BLOCK_SIZE_A53_64, Attrib);
  }
#    ifdef XFSBL_PS_HI_DDR_START_ADDRESS
  for (BlockNum = 0; BlockNum < NUM_BLOCKS_A53_64_HIGH; BlockNum++) {
    XFsbl_SetTlbAttributes(
        XFSBL_PS_HI_DDR_START_ADDRESS + BlockNum * BLOCK_SIZE_A53_64_HIGH,
        Attrib);
  }
#    endif
  Xil_DCacheFlush();
#  else
  if (FALSE == Cond) {
    Attrib = ATTRIB_MEMORY_A53_32;
  }
  /* For A53 32bit*/
  for (BlockNum = 0U; BlockNum < NUM_BLOCKS_A53_32; BlockNum++) {
    XFsbl_SetTlbAttributes(BlockNum * BLOCK_SIZE_A53_32, Attrib);
  }
  Xil_DCacheFlush();
#  endif
#endif
}
