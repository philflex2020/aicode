/**************************************************************************
 * C S 429 architecture emulator
 * 
 * instr_Writeback.c - Writeback stage of instruction processing pipeline.
 **************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err_handler.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "hw_elts.h"

extern machine_t guest;
extern mem_status_t dmem_status;

extern int64_t W_wval;

/*
 * Write-back stage logic.
 * STUDENT TO-DO:
 * Implement the writeback stage.
 * 
 * Use in as the input pipeline register.
 * 
 * You will need the global variable W_wval.
 */

#define XZR_NUM 32

comb_logic_t wback_instr(w_instr_impl_t *in) {
    assert(in != NULL);

    // Select the value to write back
    W_wval = in->W_sigs.wval_sel ? in->val_mem : in->val_ex;

    // Write back to register file if enabled and not writing to XZR
    // if (in->W_sigs.w_enable && in->dst != XZR_NUM) {
        // guest.proc->GPR[in->dst] = W_wval;
    // }

    return;
}