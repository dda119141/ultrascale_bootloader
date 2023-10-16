
/*****************************************************************************/
/**
 *
 * @file xfsbl_main.c
 *
 * This is the main file which contains code for the FSBL.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date        Changes
 * ----- ---- -------- -------------------------------------------------------
 * 1.00  kc   10/21/13 Initial release
 * 1.00  ba   02/22/16 Added performance measurement feature.
 * 2.0   bv   12/02/16 Made compliance to MISRAC 2012 guidelines
 *                     Added warm restart support
 * 3.0   bv   03/03/21 Print multiboot offset in FSBL banner
 *       bsv  04/28/21 Added support to ensure authenticated images boot as
 *                     non-secure when RSA_EN is not programmed
 *
 * </pre>
 *
 * @note
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "xfsbl_main.h"

#include "bspconfig.h"
#include "psu_init.h"
#include "xfsbl_hw.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/
static void XFsbl_UpdateMultiBoot(u32 MultiBootValue);
static void XFsbl_FallBack(void);

/************************** Variable Definitions *****************************/
XFsblPs FsblInstance = {0x3U, XFSBL_SUCCESS, 0U, 0U, 0U, 0U};

FsblStagesVal_t FsblStagesVal = {SYSTEM_INIT, XFSBL_SUCCESS, FALSE, 0U};

static void load_artifacts(XFsblPs* const FsblInstance,
                           FsblStagesVal_t* const stage) {
  /**
   * Load the partitions
   *  image header
   *  partition header
   *  partition parameters
   */
  stage->FsblStageStatus =
      XFsbl_PartitionLoad(FsblInstance, stage->PartitionNum);

  if (XFSBL_SUCCESS != stage->FsblStageStatus) {
    stage->FsblStageStatus += XFSBL_ERROR_STAGE_3_PARTITION_LOAD_FAILED;
    stage->FsblStage = XFSBL_STAGE_ERR;
  } else {
    if (stage->PartitionNum <
        (FsblInstance->ImageHeader.ImageHeaderTable.NoOfPartitions - 1U)) {
      stage->PartitionNum++;
    } else {
      XFsbl_Printf(DEBUG_GENERAL,
                   "All Partitions "
                   "Loaded \n\r");

      stage->FsblStage = XFSBL_HANDOFF;
      stage->EarlyHandoff = stage->FsblStageStatus;
    }
  }
}

static void initialize_primary_bootdevice(XFsblPs* const FsblInstance,
                                          FsblStagesVal_t* const stage) {
  /**
   * 	Primary Device, Secondary boot device
   *  DeviceOps, image header,  partition header
   */
  stage->FsblStageStatus = XFsbl_BootDeviceInit(FsblInstance);

  switch (stage->FsblStageStatus) {
  case XFSBL_STATUS_JTAG: {
    /*
     * Mark RPU cores as usable in JTAG boot
     * mode.
     */
    Xil_Out32(XFSBL_R5_USAGE_STATUS_REG,
              (Xil_In32(XFSBL_R5_USAGE_STATUS_REG) |
               (XFSBL_R5_0_STATUS_MASK | XFSBL_R5_1_STATUS_MASK)));

    stage->FsblStage = XFSBL_HANDOFF;
  } break;
  case XFSBL_SUCCESS: {
    XFsbl_Printf(DEBUG_GENERAL, "Boot Device Init Success \n\r");

    /**
     * Start the partition loading from 1
     * 0th partition will be FSBL
     */
    stage->PartitionNum = 0x1U;

    stage->FsblStage = XFSBL_PARTITION_LOAD;

  } break;
  default: {
    stage->FsblStageStatus += XFSBL_ERROR_STAGE_2_BOOTDEVICE_INIT_FAILED;
    stage->FsblStage = XFSBL_STAGE_ERR;
  } break;
  }
}

static void perform_handoff(const XFsblPs* const FsblInstance,
                            FsblStagesVal_t* const stage) {
  stage->FsblStageStatus =
      XFsbl_Handoff(FsblInstance, stage->PartitionNum, stage->EarlyHandoff);

  if (STATUS_PARTITION_LOAD_IN_PROGRESS == stage->FsblStageStatus) {
    XFsbl_Printf(DEBUG_INFO,
                 "Early handoff to a application "
                 "complete \n\r");
    XFsbl_Printf(DEBUG_INFO,
                 "Continuing to load remaining "
                 "partitions \n\r");

    stage->PartitionNum++;
    stage->FsblStage = XFSBL_PARTITION_LOAD;
  } else if (XFSBL_STATUS_CONTINUE_OTHER_HANDOFF == stage->FsblStageStatus) {
    XFsbl_Printf(DEBUG_INFO,
                 "Early handoff to a application "
                 "complete \n\r");
    XFsbl_Printf(DEBUG_INFO,
                 "Continuing handoff to other "
                 "applications, if present \n\r");
    stage->EarlyHandoff = FALSE;
  } else if (XFSBL_SUCCESS != stage->FsblStageStatus) {
    stage->FsblStageStatus += XFSBL_ERROR_HANDOFF_FAILED;
    stage->FsblStage = XFSBL_STAGE_ERR;
  } else {
    stage->FsblStage = XFSBL_STAGE_POST_HANDOFF;
  }
}

static void print_stage_status(const FsblStagesVal_t* const stage) {
  switch (stage->FsblStage) {
  case SYSTEM_INIT:
    XFsbl_Printf(DEBUG_GENERAL, "====enter system init \n ");
    break;
  case SYSTEM_PRIMARY_BOOT_DEVICE_INIT:
    XFsbl_Printf(DEBUG_GENERAL,
                 "====enter Primary boot device init "
                 "=== \n\r");
    break;
  case XFSBL_PARTITION_LOAD:
    XFsbl_Printf(DEBUG_GENERAL,
                 "======= In Stage 3, Partition Load "
                 "No:%d ======= \n\r",
                 stage->PartitionNum);
    break;
  case XFSBL_HANDOFF:
    XFsbl_Printf(DEBUG_GENERAL, "==== HandOFF=== \n\r");
    break;
  case XFSBL_STAGE_ERR:
    XFsbl_Printf(DEBUG_GENERAL,
                 "================= In Stage Err "
                 "============ \n\r");
    break;
  default:
    XFsbl_Printf(DEBUG_GENERAL, "==== Unsupported stage === \n\r");
    break;
  }
}

/*****************************************************************************/
/** This is the FSBL main function and is implemented stage wise.
 *
 * @param	None
 *
 * @return	None
 *
 *****************************************************************************/
int main(void) {
#if defined(EL3) && (EL3 != 1)
#  error "FSBL should be generated using only EL3 BSP"
#endif

  while (FsblStagesVal.FsblStage <= XFSBL_STAGE_POST_HANDOFF) {
    switch (FsblStagesVal.FsblStage) {
    case SYSTEM_INIT: {
      print_stage_status(&FsblStagesVal);
      FsblStagesVal.FsblStageStatus = XFsbl_Initialize(&FsblInstance);

      if (XFSBL_SUCCESS != FsblStagesVal.FsblStageStatus) {
        FsblStagesVal.FsblStageStatus += XFSBL_ERROR_STAGE_1_INIT_FAILED;
        FsblStagesVal.FsblStage = XFSBL_STAGE_ERR;

      } else {
        FsblStagesVal.FsblStage = SYSTEM_PRIMARY_BOOT_DEVICE_INIT;
      }
    } break;

    case SYSTEM_PRIMARY_BOOT_DEVICE_INIT:
      print_stage_status(&FsblStagesVal);
      initialize_primary_bootdevice(&FsblInstance, &FsblStagesVal);
      break;
    case XFSBL_PARTITION_LOAD:
      print_stage_status(&FsblStagesVal);
      load_artifacts(&FsblInstance, &FsblStagesVal);
      break;
    case XFSBL_HANDOFF: {
      print_stage_status(&FsblStagesVal);
      perform_handoff(&FsblInstance, &FsblStagesVal);
    } break;

    case XFSBL_STAGE_ERR: {
      XFsbl_ErrorLockDown(FsblStagesVal.FsblStageStatus);
    } break;

    case XFSBL_STAGE_POST_HANDOFF:
    default: {
      XFsbl_Printf(DEBUG_GENERAL,
                   "In post handoff stage: "
                   "handoffs completed \n\r");

      /**
       * Exit FSBL
       */
      XFsbl_HandoffExit(0U, XFSBL_NO_HANDOFFEXIT);

    } break;

    } /* End of switch(FsblStage) */

    if (FsblStagesVal.FsblStage == XFSBL_STAGE_POST_HANDOFF) {
      break;
    }
  } /* End of while(1)  */

  XFsbl_Printf(DEBUG_GENERAL,
               "Handoff probably failed: "
               "Exiting fsbl \n\r");

  XFsbl_HandoffExit(0U, XFSBL_NO_HANDOFFEXIT);

  return 0;
}

/*****************************************************************************/
/**
 * This function is called in FSBL error cases. Error status
 * register is updated and fallback is applied
 *
 * @param ErrorStatus is the error code which is written to the
 * 		  error status register
 *
 * @return none
 *
 * @note Fallback is applied only for fallback supported bootmodes
 *****************************************************************************/
void XFsbl_ErrorLockDown(u32 ErrorStatus) {
  /**
   * Update the error status register
   * and Fsbl instance structure
   */
  XFsbl_Out32(XFSBL_ERROR_STATUS_REGISTER_OFFSET, ErrorStatus);
  FsblInstance.ErrorCode = ErrorStatus;
  XFsbl_Printf(DEBUG_GENERAL, "Fsbl Error Status: 0x%08lx\r\n", ErrorStatus);

  /**
   * Read Boot Mode register
   */
  u32 BootMode = XFsbl_In32(CRL_APB_BOOT_MODE_USER) &
                 CRL_APB_BOOT_MODE_USER_BOOT_MODE_MASK;

  /**
   * Fallback if bootmode supports
   */
  if ((BootMode == XFSBL_QSPI24_BOOT_MODE) ||
      (BootMode == XFSBL_QSPI32_BOOT_MODE) ||
      (BootMode == XFSBL_NAND_BOOT_MODE) || (BootMode == XFSBL_SD0_BOOT_MODE) ||
      (BootMode == XFSBL_EMMC_BOOT_MODE) || (BootMode == XFSBL_SD1_BOOT_MODE) ||
      (BootMode == XFSBL_SD1_LS_BOOT_MODE)) {
    XFsbl_FallBack();
  } else {
    XFsbl_Printf(DEBUG_GENERAL, "Fallback not supported \n\r");

    /**
     * Exit FSBL
     */
    XFsbl_HandoffExit(0U, XFSBL_NO_HANDOFFEXIT);
  }

  /**
   * Should never be here
   */
  return;
}

/*****************************************************************************/
/**
 * In Fallback,  soft reset is applied to the system after incrementing
 * the multiboot register. A hook is provided to before the fallback so
 * that users can write their own code before soft reset
 *
 * @param none
 *
 * @return none
 *
 * @note We will not return from this function as it does soft reset
 *****************************************************************************/
static void XFsbl_FallBack(void) {
  u32 RegValue;

  /* Hook before FSBL Fallback */
  (void)XFsbl_HookBeforeFallback();

  /* Read the Multiboot register */
  RegValue = XFsbl_In32(CSU_CSU_MULTI_BOOT);

  XFsbl_Printf(DEBUG_GENERAL, "Performing FSBL FallBack\n\r");

  XFsbl_UpdateMultiBoot(RegValue + 1U);

  return;
}

/*****************************************************************************/
/**
 * This is the function which actually updates the multiboot register and
 * does the soft reset. This function is called in fallback case and
 * in the cases where user would like to jump to a different image,
 * corresponding to the multiboot value being passed to this function.
 * The latter case is a generic one and need arise because of error scenario.
 *
 * @param MultiBootValue is the new value for the multiboot register
 *
 * @return none
 *
 * @note We will not return from this function as it does soft reset
 *****************************************************************************/

static void XFsbl_UpdateMultiBoot(u32 MultiBootValue) {
  u32 RegValue;

  XFsbl_Out32(CSU_CSU_MULTI_BOOT, MultiBootValue);

  /**
   * Due to a bug in 1.0 Silicon, PS hangs after System Reset if RPLL is
   * used. Hence, just for 1.0 Silicon, bypass the RPLL clock before
   * giving System Reset.
   */
  if (XGetPSVersion_Info() == (u32)XPS_VERSION_1) {
    RegValue = XFsbl_In32(CRL_APB_RPLL_CTRL) | CRL_APB_RPLL_CTRL_BYPASS_MASK;
    XFsbl_Out32(CRL_APB_RPLL_CTRL, RegValue);
  }

  /* make sure every thing completes */
  dsb();
  isb();

  if (XFSBL_MASTER_ONLY_RESET != FsblInstance.ResetReason) {
    /* Soft reset the system */
    XFsbl_Printf(DEBUG_GENERAL, "Performing System Soft Reset\n\r");
    RegValue = XFsbl_In32(CRL_APB_RESET_CTRL);
    XFsbl_Out32(CRL_APB_RESET_CTRL,
                RegValue | CRL_APB_RESET_CTRL_SOFT_RESET_MASK);

    /* wait here until reset happens */
    while (1) {
      ;
    }
  } else {
    for (;;) {
      /*We should not be here*/
    }
  }

  return;
}
