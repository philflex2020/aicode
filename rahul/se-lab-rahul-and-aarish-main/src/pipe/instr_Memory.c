/**************************************************************************
 * C S 429 system emulator
 * 
 * instr_Memory.c - Memory stage of instruction processing pipeline.
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

extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

/*
 * Memory stage logic.
 * STUDENT TO-DO:
 * Implement the memory stage.
 * 
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 * 
 * You will need the following helper functions:
 * copy_w_ctl_signals and dmem.
 */
comb_logic_t memory_instr(m_instr_impl_t *in, w_instr_impl_t *out) {
    // Copy opcodes and status
    out->op = in->op;
    out->print_op = in->print_op;
    out->status = in->status; // Initialize with input status

    // Copy destination register
    out->dst = in->dst;

    // Copy Writeback control signals
    copy_w_ctl_sigs(&out->W_sigs, &in->W_sigs);

    // Initialize variables for memory access
    uint64_t dmem_rval = 0;
    bool dmem_err = false;

    // Perform memory operations if needed
    // if (in->M_sigs.dmem_read || in->M_sigs.dmem_write) {
        dmem(
            in->val_ex,           // Address to access
            in->val_b,            // Data to write
            in->M_sigs.dmem_read, // Read enable
            in->M_sigs.dmem_write,// Write enable
            &dmem_rval,           // Data read from memory
            &dmem_err             // Error flag
        );
    // }

    // Handle memory access errors
    if (dmem_err) {
        out->status = STAT_ADR; // Set the status to indicate an address error
    }

    // Set values for Writeback stage
    if (in->M_sigs.dmem_read) {
        // For load instructions, use the value read from memory
        out->val_mem = dmem_rval;
    } else {
        // For other instructions, pass the ALU result
        out->val_ex = in->val_ex;
    }
}