/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Execute.c - Execute stage of instruction processing pipeline.
 **************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"
#include "hw_elts.h"

extern machine_t guest;
extern mem_status_t dmem_status;

bool X_condval = true;

extern comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *, m_ctl_sigs_t *);
extern comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *, w_ctl_sigs_t *);

/*
 * Execute stage logic.
 * STUDENT TO-DO:
 * Implement the execute stage.
 *
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 *
 * You will need the following helper functions:
 * copy_m_ctl_signals, copy_w_ctl_signals, and alu.
 */

comb_logic_t execute_instr(x_instr_impl_t *in, m_instr_impl_t *out) {
    assert(in != NULL);
    assert(out != NULL);

    uint64_t alu_val_ex = 0;
    bool alu_cond_val = false;
    uint8_t alu_nzcv = 0;

    uint64_t operand_b = in->X_sigs.valb_sel ? in->val_b : in->val_imm;

    if(in->op == OP_MOVZ) {
        in->val_a = 0;
    }

    // Perform ALU operation
    //X_sigs.valb_sel if 1 val->imm, if 0 in->valb for operand b
    alu(
        in->val_a,
        operand_b,
        in->val_hw, // Ensure val_hw is passed here
        in->ALU_op,
        in->X_sigs.set_flags,
        in->cond,
        &alu_val_ex,
        &alu_cond_val,
        &guest.proc->NZCV
    );
        
    X_condval = alu_cond_val;
    out->cond_holds = alu_cond_val;


    // Set output values
    out->val_ex = alu_val_ex;
    out->val_b = in->val_b;
    out->seq_succ_PC = in->seq_succ_PC;
    out->op = in->op;
    out->print_op = in->print_op;
    out->dst = in->dst;
    out->status = in->status;

    if (in->op == OP_BL) {
        out->val_ex = out->seq_succ_PC;
    }

    // Copy control signals to the next stage
    copy_m_ctl_sigs(&(out->M_sigs), &(in->M_sigs));
    copy_w_ctl_sigs(&(out->W_sigs), &(in->W_sigs));


    return;
}