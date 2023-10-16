/******************************************************************************
 * Copyright (c) 2015 - 2020 Xilinx, Inc.  All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file xfsbl_handoff.c
 *
 * This is the main file which contains handoff code for the FSBL.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date        Changes
 * ----- ---- -------- -------------------------------------------------------
 * 1.00  kc   10/21/13 Initial release
 * 2.0   bv   12/05/16 Made compliance to MISRAC 2012 guidelines
 *       vns           Added support for HIVEC.
 *       bo   01/25/17 During handoff again R5 is restored to LOVEC.
 *       sc   02/04/17 Lock XMPU/XPPU for further access but by default
 *                     it is by passed.
 *       bv   03/17/17 Modified such that XFsbl_PmInit is done only duing
 *                     system reset
 * 3.0   ma   09/09/19 Update FSBL proc info reporting to PMU
 * 4.0   bsv  03/05/19 Restore value of SD_CDN_CTRL register before
 *                     handoff in FSBL
 *
 * </pre>
 *
 * @note
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "psu_init.h"
#include "xfsbl_hw.h"
#include "xfsbl_image_header.h"
#include "xfsbl_main.h"
#include "xil_cache.h"

/************************** Constant Definitions *****************************/
#define XFSBL_CPU_POWER_UP (0x1U)
#define XFSBL_CPU_SWRST (0x2U)

/**
 * Aarch32 or Aarch64 CPU definitions
 */
#define APU_CONFIG_0_AA64N32_MASK_CPU0 (0x1U)
#define APU_CONFIG_0_AA64N32_MASK_CPU1 (0x2U)
#define APU_CONFIG_0_AA64N32_MASK_CPU2 (0x4U)
#define APU_CONFIG_0_AA64N32_MASK_CPU3 (0x8U)

#define APU_CONFIG_0_VINITHI_MASK_CPU0 (u32)(0x100U)
#define APU_CONFIG_0_VINITHI_MASK_CPU1 (u32)(0x200U)
#define APU_CONFIG_0_VINITHI_MASK_CPU2 (u32)(0x400U)
#define APU_CONFIG_0_VINITHI_MASK_CPU3 (u32)(0x800U)

#define APU_CONFIG_0_VINITHI_SHIFT_CPU0 (8U)
#define APU_CONFIG_0_VINITHI_SHIFT_CPU1 (9U)
#define APU_CONFIG_0_VINITHI_SHIFT_CPU2 (10U)
#define APU_CONFIG_0_VINITHI_SHIFT_CPU3 (11U)

#define OTHER_CPU_HANDOFF (0x0U)
#define A53_0_64_HANDOFF_TO_A53_0_32 (0x1U)
#define A53_0_32_HANDOFF_TO_A53_0_64 (0x2U)

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

static u32 XFsbl_SetCpuPwrSettings(u32 CpuSettings, u32 Flags);
static void XFsbl_UpdateResetVector(u64 HandOffAddress, u32 CpuSettings,
                                    u32 HandoffType, u32 Vector);
static u32 XFsbl_Is32BitCpu(u32 CpuSettings);
static u32 XFsbl_ProtectionConfig(void);

/**
 * Functions defined in xfsbl_handoff.S
 */
extern void XFsbl_Exit(PTRSIZE HandoffAddress, u32 Flags);

/************************** Variable Definitions *****************************/

extern u32 SdCdnRegVal;

static u32 XFsbl_Is32BitCpu(u32 CpuSettings) {
  u32 Status;
  u32 CpuId;
  u32 ExecState;

  CpuId = CpuSettings & XIH_PH_ATTRB_DEST_CPU_MASK;
  ExecState = CpuSettings & XIH_PH_ATTRB_A53_EXEC_ST_MASK;

  if ((CpuId == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
      (CpuId == XIH_PH_ATTRB_DEST_CPU_R5_1) ||
      (CpuId == XIH_PH_ATTRB_DEST_CPU_R5_L) ||
      (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32)) {
    Status = TRUE;
  } else {
    Status = FALSE;
  }

  return Status;
}

/****************************************************************************/
/**
 * This function will set up the settings for the CPU's
 * This can power up the CPU or do a soft reset to the CPU's
 *
 * @param CpuId specifies for which CPU settings should be done
 *
 * @param Flags is used to specify the settings for the CPU
 * 			XFSBL_CPU_POWER_UP - This is used to power up the CPU
 * 			XFSBL_CPU_SWRST - This is used to trigger the reset to
 *CPU
 *
 * @return
 * 		- XFSBL_SUCCESS on successful settings
 * 		- XFSBL_FAILURE
 *
 * @note
 *
 *****************************************************************************/

static u32 XFsbl_SetCpuPwrSettings(u32 CpuSettings, u32 Flags) {
  u32 RegValue;
  u32 Status;
  u32 CpuId;
  u32 ExecState;
  u32 PwrStateMask;

  /**
   * Reset the CPU
   */
  if ((Flags & XFSBL_CPU_SWRST) != 0U) {
    CpuId = CpuSettings & XIH_PH_ATTRB_DEST_CPU_MASK;
    ExecState = CpuSettings & XIH_PH_ATTRB_A53_EXEC_ST_MASK;
    switch (CpuId) {
    case XIH_PH_ATTRB_DEST_CPU_A53_0:

      PwrStateMask = PMU_GLOBAL_PWR_STATE_ACPU0_MASK |
                     PMU_GLOBAL_PWR_STATE_FP_MASK |
                     PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK;

      Status = XFsbl_PowerUpIsland(PwrStateMask);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_A53_0_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_A53_0_POWER_UP\r\n");
        goto END;
      }

      /**
       * Set to Aarch32 if enabled
       */
      if (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32) {
        RegValue = XFsbl_In32(APU_CONFIG_0);
        RegValue &= ~(APU_CONFIG_0_AA64N32_MASK_CPU0);
        XFsbl_Out32(APU_CONFIG_0, RegValue);
      }

      /**
       *  Enable the clock
       */
      RegValue = XFsbl_In32(CRF_APB_ACPU_CTRL);
      RegValue |= (CRF_APB_ACPU_CTRL_CLKACT_FULL_MASK |
                   CRF_APB_ACPU_CTRL_CLKACT_HALF_MASK);
      XFsbl_Out32(CRF_APB_ACPU_CTRL, RegValue);

      /**
       * Release reset
       */
      RegValue = XFsbl_In32(CRF_APB_RST_FPD_APU);
      RegValue &= ~(CRF_APB_RST_FPD_APU_ACPU0_RESET_MASK |
                    CRF_APB_RST_FPD_APU_APU_L2_RESET_MASK |
                    CRF_APB_RST_FPD_APU_ACPU0_PWRON_RESET_MASK);
      XFsbl_Out32(CRF_APB_RST_FPD_APU, RegValue);

      break;

    case XIH_PH_ATTRB_DEST_CPU_A53_1:

      PwrStateMask = PMU_GLOBAL_PWR_STATE_ACPU1_MASK |
                     PMU_GLOBAL_PWR_STATE_FP_MASK |
                     PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK;

      Status = XFsbl_PowerUpIsland(PwrStateMask);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_A53_1_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_A53_1_POWER_UP\r\n");
        goto END;
      }

      /**
       * Set to Aarch32 if enabled
       */
      if (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32) {
        RegValue = XFsbl_In32(APU_CONFIG_0);
        RegValue &= ~(APU_CONFIG_0_AA64N32_MASK_CPU1);
        XFsbl_Out32(APU_CONFIG_0, RegValue);
      }

      /**
       *  Enable the clock
       */
      RegValue = XFsbl_In32(CRF_APB_ACPU_CTRL);
      RegValue |= (CRF_APB_ACPU_CTRL_CLKACT_FULL_MASK |
                   CRF_APB_ACPU_CTRL_CLKACT_HALF_MASK);
      XFsbl_Out32(CRF_APB_ACPU_CTRL, RegValue);

      /**
       * Release reset
       */
      RegValue = XFsbl_In32(CRF_APB_RST_FPD_APU);
      RegValue &= ~(CRF_APB_RST_FPD_APU_ACPU1_RESET_MASK |
                    CRF_APB_RST_FPD_APU_APU_L2_RESET_MASK |
                    CRF_APB_RST_FPD_APU_ACPU1_PWRON_RESET_MASK);
      XFsbl_Out32(CRF_APB_RST_FPD_APU, RegValue);

      break;

    case XIH_PH_ATTRB_DEST_CPU_A53_2:

      PwrStateMask = PMU_GLOBAL_PWR_STATE_ACPU2_MASK |
                     PMU_GLOBAL_PWR_STATE_FP_MASK |
                     PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK;

      Status = XFsbl_PowerUpIsland(PwrStateMask);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_A53_2_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_A53_2_POWER_UP\r\n");
        goto END;
      }

      /**
       * Set to Aarch32 if enabled
       */
      if (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32) {
        RegValue = XFsbl_In32(APU_CONFIG_0);
        RegValue &= ~(APU_CONFIG_0_AA64N32_MASK_CPU2);
        XFsbl_Out32(APU_CONFIG_0, RegValue);
      }

      /**
       *  Enable the clock
       */
      RegValue = XFsbl_In32(CRF_APB_ACPU_CTRL);
      RegValue |= (CRF_APB_ACPU_CTRL_CLKACT_FULL_MASK |
                   CRF_APB_ACPU_CTRL_CLKACT_HALF_MASK);
      XFsbl_Out32(CRF_APB_ACPU_CTRL, RegValue);

      /**
       * Release reset
       */
      RegValue = XFsbl_In32(CRF_APB_RST_FPD_APU);
      RegValue &= ~(CRF_APB_RST_FPD_APU_ACPU2_RESET_MASK |
                    CRF_APB_RST_FPD_APU_APU_L2_RESET_MASK |
                    CRF_APB_RST_FPD_APU_ACPU2_PWRON_RESET_MASK);

      XFsbl_Out32(CRF_APB_RST_FPD_APU, RegValue);

      break;

    case XIH_PH_ATTRB_DEST_CPU_A53_3:

      PwrStateMask = PMU_GLOBAL_PWR_STATE_ACPU3_MASK |
                     PMU_GLOBAL_PWR_STATE_FP_MASK |
                     PMU_GLOBAL_PWR_STATE_L2_BANK0_MASK;

      Status = XFsbl_PowerUpIsland(PwrStateMask);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_A53_3_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_A53_3_POWER_UP\r\n");
        goto END;
      }

      /**
       * Set to Aarch32 if enabled
       */
      if (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32) {
        RegValue = XFsbl_In32(APU_CONFIG_0);
        RegValue &= ~(APU_CONFIG_0_AA64N32_MASK_CPU3);
        XFsbl_Out32(APU_CONFIG_0, RegValue);
      }

      /**
       *  Enable the clock
       */
      RegValue = XFsbl_In32(CRF_APB_ACPU_CTRL);
      RegValue |= (CRF_APB_ACPU_CTRL_CLKACT_FULL_MASK |
                   CRF_APB_ACPU_CTRL_CLKACT_HALF_MASK);
      XFsbl_Out32(CRF_APB_ACPU_CTRL, RegValue);

      /**
       * Release reset
       */
      RegValue = XFsbl_In32(CRF_APB_RST_FPD_APU);
      RegValue &= ~(CRF_APB_RST_FPD_APU_ACPU3_RESET_MASK |
                    CRF_APB_RST_FPD_APU_APU_L2_RESET_MASK |
                    CRF_APB_RST_FPD_APU_ACPU3_PWRON_RESET_MASK);

      XFsbl_Out32(CRF_APB_RST_FPD_APU, RegValue);

      break;

    case XIH_PH_ATTRB_DEST_CPU_R5_0:

      Status = XFsbl_PowerUpIsland(PMU_GLOBAL_PWR_STATE_R5_0_MASK);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_R5_0_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_R5_0_POWER_UP\r\n");
        goto END;
      }

      /**
       * Place R5, TCM's in split mode
       */
      RegValue = XFsbl_In32(RPU_RPU_GLBL_CNTL);
      RegValue |= (RPU_RPU_GLBL_CNTL_SLSPLIT_MASK);
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
      RegValue |= (CRL_APB_CPU_R5_CTRL_CLKACT_MASK);
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

      /**
       * Take R5-0 out of HALT state
       */
      RegValue = XFsbl_In32(RPU_RPU_0_CFG);
      RegValue |= RPU_RPU_0_CFG_NCPUHALT_MASK;
      XFsbl_Out32(RPU_RPU_0_CFG, RegValue);
      break;

    case XIH_PH_ATTRB_DEST_CPU_R5_1:

      Status = XFsbl_PowerUpIsland(PMU_GLOBAL_PWR_STATE_R5_1_MASK);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_R5_1_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_R5_1_POWER_UP\r\n");
        goto END;
      }

      /**
       * Place R5, TCM's in split mode
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

      /**
       * Take R5-1 out of HALT state
       */
      RegValue = XFsbl_In32(RPU_RPU_1_CFG);
      RegValue |= RPU_RPU_1_CFG_NCPUHALT_MASK;
      XFsbl_Out32(RPU_RPU_1_CFG, RegValue);
      break;
    case XIH_PH_ATTRB_DEST_CPU_R5_L:

      Status = XFsbl_PowerUpIsland(PMU_GLOBAL_PWR_STATE_R5_0_MASK);
      if (Status != XFSBL_SUCCESS) {
        Status = XFSBL_ERROR_R5_L_POWER_UP;
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_R5_L_POWER_UP\r\n");
        goto END;
      }

      /**
       * Place R5, TCM's in safe mode
       */
      RegValue = XFsbl_In32(RPU_RPU_GLBL_CNTL);
      RegValue &= ~(RPU_RPU_GLBL_CNTL_SLSPLIT_MASK);
      RegValue |= RPU_RPU_GLBL_CNTL_TCM_COMB_MASK;
      RegValue |= RPU_RPU_GLBL_CNTL_SLCLAMP_MASK;
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
       * Release reset to R5-0, R5-1
       */
      RegValue = XFsbl_In32(CRL_APB_RST_LPD_TOP);
      RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_R50_RESET_MASK);
      RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_R51_RESET_MASK);
      RegValue &= ~(CRL_APB_RST_LPD_TOP_RPU_AMBA_RESET_MASK);
      XFsbl_Out32(CRL_APB_RST_LPD_TOP, RegValue);

      /**
       * Take R5-0 out of HALT state
       */
      RegValue = XFsbl_In32(RPU_RPU_0_CFG);
      RegValue |= RPU_RPU_0_CFG_NCPUHALT_MASK;
      XFsbl_Out32(RPU_RPU_0_CFG, RegValue);

      /**
       * Take R5-1 out of HALT state
       */
      RegValue = XFsbl_In32(RPU_RPU_1_CFG);
      RegValue |= RPU_RPU_1_CFG_NCPUHALT_MASK;
      XFsbl_Out32(RPU_RPU_1_CFG, RegValue);
      break;

    default:
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_HANDOFF_FAILED_CPUID\n\r");
      Status = XFSBL_ERROR_HANDOFF_FAILED_CPUID;
      break;
    }

  } else {
    Status = XFSBL_SUCCESS;
  }
END:
  return Status;
}

/****************************************************************************/
/**
 * FSBL exit function before the assembly code
 *
 * @param HandoffAddress is handoff address for the FSBL running cpu
 *
 * @param Flags is to determine whether to handoff to application or
 * 			to be in wfe state
 *
 * @return None
 *
 *
 *****************************************************************************/
void XFsbl_HandoffExit(u64 HandoffAddress, u32 Flags) {
  u32 RegVal;

  /*
   * Write 1U to PMU GLOBAL general storage register 5 to indicate
   * PMU Firmware that FSBL completed execution
   */
  RegVal = XFsbl_In32(PMU_GLOBAL_GLOB_GEN_STORAGE5);
  RegVal &= ~(XFSBL_EXEC_COMPLETED);
  RegVal |= XFSBL_EXEC_COMPLETED;
  XFsbl_Out32(PMU_GLOBAL_GLOB_GEN_STORAGE5, RegVal);

  XFsbl_Printf(DEBUG_GENERAL, "Exit from FSBL \n\r");

  /**
   * Exit to handoff address
   * PTRSIZE is used since handoff is in same running cpu
   * and address is of PTRSIZE
   */
  XFsbl_Exit((PTRSIZE)HandoffAddress, Flags);

  /**
   * should not reach here
   */
  return;
}

/****************************************************************************/
/**
 *
 * @param
 *
 * @return
 *
 * @note
 *
 *
 *****************************************************************************/
static void XFsbl_UpdateResetVector(u64 HandOffAddress, u32 CpuSettings,
                                    u32 HandoffType, u32 Vector) {
  u32 HandOffAddressLow;
  u32 HandOffAddressHigh;
  u32 LowAddressReg;
  u32 HighAddressReg;
  u32 CpuId;
  u32 RegVal;
  u32 ExecState;

  CpuId = CpuSettings & XIH_PH_ATTRB_DEST_CPU_MASK;
  ExecState = CpuSettings & XIH_PH_ATTRB_A53_EXEC_ST_MASK;

  /**
   * Put R5 or A53-32 in Lovec/Hivec
   */
  if ((CpuId == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
      (CpuId == XIH_PH_ATTRB_DEST_CPU_R5_L)) {
    RegVal = XFsbl_In32(RPU_RPU_0_CFG);
    RegVal &= ~RPU_RPU_0_CFG_VINITHI_MASK;
    RegVal |= (Vector << RPU_RPU_0_CFG_VINITHI_SHIFT);
    XFsbl_Out32(RPU_RPU_0_CFG, RegVal);
  }

  else if ((CpuId == XIH_PH_ATTRB_DEST_CPU_R5_1) ||
           (CpuId == XIH_PH_ATTRB_DEST_CPU_R5_L)) {
    RegVal = XFsbl_In32(RPU_RPU_1_CFG);
    RegVal &= ~RPU_RPU_1_CFG_VINITHI_MASK;
    RegVal |= (Vector << RPU_RPU_1_CFG_VINITHI_SHIFT);
    XFsbl_Out32(RPU_RPU_1_CFG, RegVal);
  }

  else if ((CpuId == XIH_PH_ATTRB_DEST_CPU_A53_0) &&
           (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32)) {
    RegVal = XFsbl_In32(APU_CONFIG_0);
    RegVal &= ~APU_CONFIG_0_VINITHI_MASK_CPU0;
    RegVal |= (Vector << APU_CONFIG_0_VINITHI_SHIFT_CPU0);
    XFsbl_Out32(APU_CONFIG_0, RegVal);
  }

  else if ((CpuId == XIH_PH_ATTRB_DEST_CPU_A53_1) &&
           (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32)) {
    RegVal = XFsbl_In32(APU_CONFIG_0);
    RegVal &= ~APU_CONFIG_0_VINITHI_MASK_CPU1;
    RegVal |= (Vector << APU_CONFIG_0_VINITHI_SHIFT_CPU1);
    XFsbl_Out32(APU_CONFIG_0, RegVal);
  }

  else if ((CpuId == XIH_PH_ATTRB_DEST_CPU_A53_2) &&
           (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32)) {
    RegVal = XFsbl_In32(APU_CONFIG_0);
    RegVal &= ~APU_CONFIG_0_VINITHI_MASK_CPU2;
    RegVal |= (Vector << APU_CONFIG_0_VINITHI_SHIFT_CPU2);
    XFsbl_Out32(APU_CONFIG_0, RegVal);
  }

  else if ((CpuId == XIH_PH_ATTRB_DEST_CPU_A53_3) &&
           (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32)) {
    RegVal = XFsbl_In32(APU_CONFIG_0);
    RegVal &= ~APU_CONFIG_0_VINITHI_MASK_CPU3;
    RegVal |= (Vector << APU_CONFIG_0_VINITHI_SHIFT_CPU3);
    XFsbl_Out32(APU_CONFIG_0, RegVal);
  } else {
    /* for MISRA C compliance */
  }

  if ((XFsbl_Is32BitCpu(CpuSettings) == FALSE) &&
      (HandoffType != A53_0_32_HANDOFF_TO_A53_0_64)) {
    /**
     * for A53 cpu, write 64bit handoff address
     * to the RVBARADDR in APU
     */

    HandOffAddressLow = (u32)(HandOffAddress & 0xFFFFFFFFU);
    HandOffAddressHigh = (u32)((HandOffAddress >> 32) & 0xFFFFFFFFU);
    switch (CpuId) {
    case XIH_PH_ATTRB_DEST_CPU_A53_0:
      LowAddressReg = APU_RVBARADDR0L;
      HighAddressReg = APU_RVBARADDR0H;
      break;
    case XIH_PH_ATTRB_DEST_CPU_A53_1:
      LowAddressReg = APU_RVBARADDR1L;
      HighAddressReg = APU_RVBARADDR1H;
      break;
    case XIH_PH_ATTRB_DEST_CPU_A53_2:
      LowAddressReg = APU_RVBARADDR2L;
      HighAddressReg = APU_RVBARADDR2H;
      break;
    case XIH_PH_ATTRB_DEST_CPU_A53_3:
      LowAddressReg = APU_RVBARADDR3L;
      HighAddressReg = APU_RVBARADDR3H;
      break;
    default:
      /**
       * error can be triggered here
       */
      LowAddressReg = 0U;
      HighAddressReg = 0U;
      break;
    }
    XFsbl_Out32(LowAddressReg, HandOffAddressLow);
    XFsbl_Out32(HighAddressReg, HandOffAddressHigh);
  }

  return;
}

u32 XFsbl_HandoffExecute(const XFsblPs* const FsblInstancePtr,
                         u32 PartitionNum) {
  u32 CpuIndex = 0U;
  u32 CpuSettings;
  u32 ExecState;
  u32 Status;
  u32 CpuId;
  u64 HandoffAddress;
  const XFsblPs_PartitionHeader* PartitionHeader;

  PartitionHeader = &FsblInstancePtr->ImageHeader.PartitionHeader[PartitionNum];

  while (CpuIndex < FsblInstancePtr->HandoffCpuNo) {
    CpuSettings = FsblInstancePtr->HandoffValues[CpuIndex].CpuSettings;

    CpuId = CpuSettings & XIH_PH_ATTRB_DEST_CPU_MASK;
    ExecState = CpuSettings & XIH_PH_ATTRB_A53_EXEC_ST_MASK;

    /**
     * Run the code in this loop in the below conditions:
     * - HandOffCPU is not the running CPU
     */
    if (CpuId != FsblInstancePtr->ProcessorID) {
      /* Check if handoff CPU is supported */
      Status = XFsbl_CheckSupportedCpu(CpuId);
      if (XFSBL_SUCCESS != Status) {
        XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_UNAVAILABLE_CPU\n\r");
        Status = XFSBL_ERROR_UNAVAILABLE_CPU;
        return Status;
      }

      /**
       * Check for power status of the cpu
       * Update the IVT
       * Take cpu out of reset
       */
      Status = XFsbl_SetCpuPwrSettings(CpuSettings, XFSBL_CPU_POWER_UP);
      if (XFSBL_SUCCESS != Status) {
        XFsbl_Printf(DEBUG_GENERAL,
                     "Power Up "
                     "Cpu 0x%0lx failed \n\r",
                     CpuId);

        Status = XFSBL_ERROR_PWR_UP_CPU;
        return Status;
      }

      HandoffAddress =
          (u64)FsblInstancePtr->HandoffValues[CpuIndex].HandoffAddress;

      /**
       * Update the handoff address at reset vector
       * address
       */
      XFsbl_UpdateResetVector(HandoffAddress, CpuSettings, OTHER_CPU_HANDOFF,
                              XFsbl_GetVectorLocation(PartitionHeader) >>
                                  XIH_ATTRB_VECTOR_LOCATION_SHIFT);

      XFsbl_Printf(
          DEBUG_INFO,
          "CPU 0x%0lx reset release, "
          "Exec State 0x%0lx, "
          "HandoffAddress: %0lx\n\r",
          CpuId, ExecState,
          (PTRSIZE)FsblInstancePtr->HandoffValues[CpuIndex].HandoffAddress);

      /**
       * Take CPU out of reset
       */
      Status = XFsbl_SetCpuPwrSettings(CpuSettings, XFSBL_CPU_SWRST);
      if (XFSBL_SUCCESS != Status) {
        return Status;
      }
    } else {
      /**
       * Update reset vector address for
       * - FSBL running on A53-0 (64bit), handoff to
       * A53-0 (32 bit)
       * - FSBL running on A53-0 (32bit), handoff to
       * A53-0 (64 bit)
       */
      if ((FsblInstancePtr->A53ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA64) &&
          (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32)) {
        Status = XFSBL_ERROR_UNSUPPORTED_HANDOFF;
        XFsbl_Printf(DEBUG_GENERAL,
                     "XFSBL_ERROR_UNSUPPORTED_HANDOFF : "
                     "A53-0 64 bit to 32 bit\n\r");
        return Status;
      } else if ((FsblInstancePtr->A53ExecState ==
                  XIH_PH_ATTRB_A53_EXEC_ST_AA32) &&
                 (ExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA64)) {
        Status = XFSBL_ERROR_UNSUPPORTED_HANDOFF;
        XFsbl_Printf(DEBUG_GENERAL,
                     "XFSBL_ERROR_UNSUPPORTED_HANDOFF : "
                     "A53-0 32 bit to 64 bit\n\r");
        return Status;
      } else {
        /* for MISRA C compliance */
      }

      completeHandoff_RunningCoreIsHandoffCore(
          FsblInstancePtr->HandoffValues[CpuIndex].HandoffAddress, ExecState);
    }
#if 0
    if ((EarlyHandoff == TRUE) && (CpuNeedsEarlyHandoff == TRUE)){

			/* Enable cache again as we will continue loading partitions */
			Xil_DCacheEnable();

			if (PartitionNum <
					(FsblInstancePtr->
							ImageHeader.ImageHeaderTable.NoOfPartitions-1U)) {
				/**
				 * If this is not the last handoff CPU, return back and continue
				 * loading remaining partitions in stage 3
				 */
				CpuIndexEarlyHandoff++;
				Status = XFSBL_STATUS_CONTINUE_PARTITION_LOAD;
			}
			else {
				/**
				 * Early handoff to all required CPUs is done, continue with
				 * regular handoff for remaining applications, as applicable
				 */
				Status = XFSBL_STATUS_CONTINUE_OTHER_HANDOFF;
			}
			goto END;
		}
#endif
    /* Go to the next cpu */
    CpuIndex++;
  }
  return Status;
}

void completeHandoff_RunningCoreIsHandoffCore(u32 CpuHandoffAddress,
                                              u32 RunningCpuExecState) {
  /**
   * call to the handoff routine
   * which will never return
   */
  XFsbl_Printf(DEBUG_GENERAL,
               "Running Cpu Handoff address: 0x%0lx, Exec State: %0lx\n\r",
               (PTRSIZE)CpuHandoffAddress, RunningCpuExecState);
  if (RunningCpuExecState == XIH_PH_ATTRB_A53_EXEC_ST_AA32) {
    XFsbl_HandoffExit(CpuHandoffAddress, XFSBL_HANDOFFEXIT_32);
  } else {
    XFsbl_HandoffExit(CpuHandoffAddress, XFSBL_HANDOFFEXIT);
  }
}

void HandoffJtagMode(const XFsblPs* const FsblInstancePtr) {
  /**
   * Mark Error status with Fsbl completed
   */
  XFsbl_Out32(XFSBL_ERROR_STATUS_REGISTER_OFFSET, XFSBL_COMPLETED);

  if (XGet_Zynq_UltraMp_Platform_info() == (u32)(0X2U)) {
    XFsbl_Printf(DEBUG_GENERAL, "Exit from FSBL. \n\r");
#ifdef ARMA53_64
    XFsbl_Out32(0xFFFC0000U, 0x14000000U);
#else
    XFsbl_Out32(0xFFFC0000U, 0xEAFFFFFEU);
#endif
    XFsbl_Exit(0xFFFC0000U, XFSBL_HANDOFFEXIT);
  } else {
    /**
     * Exit from FSBL
     */
    XFsbl_HandoffExit(0U, XFSBL_NO_HANDOFFEXIT);
  }
}

u32 XFsbl_Handoff(const XFsblPs* const FsblInstancePtr, u32 PartitionNum,
                  u32 EarlyHandoff) {
  u32 Status;

  /* Restoring the SD card detection signal */
  XFsbl_Out32(IOU_SLCR_SD_CDN_CTRL, SdCdnRegVal);

  if (FsblInstancePtr->ResetReason == XFSBL_PS_ONLY_RESET) {
    /**Remove PS-PL isolation to allow u-boot and linux to access
     * PL*/
    (void)psu_ps_pl_isolation_removal_data();
    (void)psu_ps_pl_reset_config_data();
  }

  /**
   * Flush the L1 data cache and L2 cache, Disable Data Cache
   */
  Xil_DCacheDisable();

  if (XFSBL_MASTER_ONLY_RESET != FsblInstancePtr->ResetReason) {
    Status = XFsbl_PmInit();
    if (Status != XFSBL_SUCCESS) {
      Status = XFSBL_ERROR_PM_INIT;
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PM_INIT\r\n");
      return Status;
    }

    Status = XFsbl_ProtectionConfig();
    if (Status != XFSBL_SUCCESS) {
      XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_HOOK_BEFORE_HANDOFF\r\n");
      return Status;
    }
    XFsbl_Printf(DEBUG_GENERAL, "Protection configuration applied\r\n");
  }

  if (XFsbl_HookBeforeHandoff(EarlyHandoff) != XFSBL_SUCCESS) {
    Status = XFSBL_ERROR_HOOK_BEFORE_HANDOFF;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_HOOK_BEFORE_HANDOFF\r\n");
    return Status;
  }

  /**
   * if JTAG bootmode, be in while loop as of now
   * Check if Process can be parked in HALT state
   */
  if (FsblInstancePtr->PrimaryBootDevice == XFSBL_JTAG_BOOT_MODE) {
    HandoffJtagMode(FsblInstancePtr);
  }

  /**
   * Mark Error status with Fsbl completed
   */
  XFsbl_Out32(XFSBL_ERROR_STATUS_REGISTER_OFFSET, XFSBL_COMPLETED);

  Status = XFsbl_HandoffExecute(FsblInstancePtr, PartitionNum);
  if (Status != XFSBL_SUCCESS) {
    return Status;
  }

  return Status;
}

/*****************************************************************************/
/**
 * This function determines if the given partition needs early handoff
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @param	PartitionNum is the partition number of the image
 *
 * @return	TRUE if this partitions needs early handoff, and FALSE if not
 *
 *****************************************************************************/
u32 XFsbl_CheckEarlyHandoff(XFsblPs* FsblInstancePtr, u32 PartitionNum) {
  u32 Status = FALSE;
  return Status;
}

/****************************************************************************/
/**
 *
 * @param
 *
 * @return
 *
 * @note
 *
 *
 *****************************************************************************/
static u32 XFsbl_ProtectionConfig(void) {
  u32 CfgRegVal1;
  u32 CfgRegVal3;
  u32 Status;
  /* Disable Tamper responses*/
  CfgRegVal1 = XFsbl_In32(XFSBL_PS_SYSMON_CONFIGREG1);
  CfgRegVal3 = XFsbl_In32(XFSBL_PS_SYSMON_CONFIGREG3);

  XFsbl_Out32(XFSBL_PS_SYSMON_CONFIGREG1,
              CfgRegVal1 | XFSBL_PS_SYSMON_CFGREG1_ALRM_DISBL_MASK);
  XFsbl_Out32(XFSBL_PS_SYSMON_CONFIGREG3,
              CfgRegVal3 | XFSBL_PS_SYSMON_CFGREG3_ALRM_DISBL_MASK);

  /* FSBL shall bypass XPPU and FPD XMPU configuration BY DEFAULT.
   *  This means though the Isolation configuration through hdf is used
   * throughout the software flow, for the hardware, isolation will only
   * be limited to just OCM.
   */
#ifdef XFSBL_PROT_BYPASS
  psu_apply_master_tz();
  psu_ocm_protection();
#else
  /* Apply protection configuration */
  Status = (u32)psu_protection();
  if (Status != XFSBL_SUCCESS) {
    Status = XFSBL_ERROR_PROTECTION_CFG;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PROTECTION_CFG\r\n");
    goto END;
  }

  /* Lock XMPU/XPPU for further access */
  Status = (u32)psu_protection_lock();
  if (Status != XFSBL_SUCCESS) {
    Status = XFSBL_ERROR_PROTECTION_CFG;
    XFsbl_Printf(DEBUG_GENERAL, "XFSBL_ERROR_PROTECTION_CFG\r\n");
    goto END;
  }
#endif

  /*Enable Tamper responses*/

  XFsbl_Out32(XFSBL_PS_SYSMON_CONFIGREG1, CfgRegVal1);
  XFsbl_Out32(XFSBL_PS_SYSMON_CONFIGREG3, CfgRegVal3);
  Status = XFSBL_SUCCESS;
  goto END;

END:
  return Status;
}
