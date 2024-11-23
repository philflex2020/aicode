/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Decode.c - Decode stage of instruction processing pipeline.
 **************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "instr.h"
#include "instr_pipeline.h"
#include "forward.h"
#include "machine.h"
#include "hw_elts.h"

#define SP_NUM 31
#define XZR_NUM 32

extern machine_t guest;
extern mem_status_t dmem_status;

extern int64_t W_wval;

/*
 * Control signals for D, X, M, and W stages.
 * Generated by D stage logic.
 * D control signals are consumed locally.
 * Others must be buffered in pipeline registers.
 * STUDENT TO-DO:
 * Generate the correct control signals for this instruction's
 * future stages and write them to the corresponding struct.
 */

static comb_logic_t generate_DXMW_control(opcode_t op,
                                          d_ctl_sigs_t *D_sigs,
                                          x_ctl_sigs_t *X_sigs,
                                          m_ctl_sigs_t *M_sigs,
                                          w_ctl_sigs_t *W_sigs) {
    assert(X_sigs != NULL && M_sigs != NULL && W_sigs != NULL && D_sigs != NULL);

    // Initialize all control signals to false
    D_sigs->src2_sel = false;
    X_sigs->valb_sel = false;
    X_sigs->set_flags = false;
    M_sigs->dmem_read = false;
    M_sigs->dmem_write = false;
    W_sigs->w_enable = false;
    W_sigs->dst_sel = false;
    W_sigs->wval_sel = false;

    // Set signals using boolean expressions
    D_sigs->src2_sel = (op == OP_STUR);

    X_sigs->valb_sel = (op == OP_ADDS_RR || op == OP_CMN_RR || op == OP_SUBS_RR || 
                        op == OP_CMP_RR || op == OP_ORR_RR || op == OP_EOR_RR || 
                        op == OP_ANDS_RR || op == OP_TST_RR || op == OP_MVN);

    X_sigs->set_flags = (op == OP_ADDS_RR || op == OP_SUBS_RR || op == OP_ANDS_RR || 
                         op == OP_TST_RR || op == OP_CMN_RR || op == OP_CMP_RR);

    M_sigs->dmem_read = (op == OP_LDUR);
    M_sigs->dmem_write = (op == OP_STUR);

    W_sigs->dst_sel = (op == OP_BL);
    W_sigs->wval_sel = (op == OP_LDUR);
    W_sigs->w_enable = !(op == OP_NOP || op == OP_RET || op == OP_HLT || op == OP_B || op == OP_B_COND || op == OP_CMN_RR || op == OP_CMP_RR
                            || op == OP_TST_RR || op == OP_STUR || op == OP_ERROR);

    return;
}
/*
 * Logic for extracting the immediate value for M-, I-, and RI-format instructions.
 * STUDENT TO-DO:
 * Extract the immediate value and write it to *imm.
 */

static void extract_immval(uint32_t insnbits, opcode_t op, int64_t *imm) {
    assert(imm != NULL);

    switch (op) {

        case OP_LDUR:
        case OP_STUR:
            *imm = bitfield_s64(insnbits, 12, 9);
            break;

        case OP_MOVK:
        case OP_MOVZ:
            *imm = bitfield_u32(insnbits, 5, 16);
            break;

        case OP_ADRP:
            *imm = (bitfield_s64(insnbits, 5, 19) << 2 | bitfield_u32(insnbits, 29, 2)) << 12; // check manual
            break;

        case OP_ADD_RI:
        case OP_SUB_RI:
        case OP_UBFM:
            *imm = bitfield_u32(insnbits, 10, 12);
            break;

        case OP_LSR:
        case OP_ASR:
            *imm = bitfield_u32(insnbits, 16, 6);
            break;
        
        case OP_LSL:
            *imm = 63 - bitfield_u32(insnbits, 10, 6);
            break;

        case OP_B:
        case OP_BL:
            *imm = bitfield_s64(insnbits, 0, 26);
            break;

        case OP_B_COND:
            *imm = bitfield_s64(insnbits, 5, 19);
            break;

        default:
            *imm = 0;
            break;
    }

}

/*
 * Logic for determining the ALU operation needed for this opcode.
 * STUDENT TO-DO:
 * Determine the ALU operation based on the given opcode
 * and write it to *ALU_op.
 */
static void decide_alu_op(opcode_t op, alu_op_t *ALU_op) {
    assert(ALU_op != NULL);

    switch (op) {
        case OP_NOP:
            *ALU_op = PASS_A_OP;
            break;
        case OP_ADD_RI: 
        case OP_ADDS_RR:
        case OP_LDUR:
        case OP_STUR:
        case OP_CMN_RR:
        case OP_ADRP:
        // ldur stur adrp cmn
            *ALU_op = PLUS_OP; 
            break;
        case OP_SUB_RI: 
        case OP_SUBS_RR: 
        case OP_CMP_RR: 
            *ALU_op = MINUS_OP; 
            break;
        case OP_ANDS_RR: 
        case OP_TST_RR:
            *ALU_op = AND_OP; 
            break;
        case OP_ORR_RR: 
            *ALU_op = OR_OP; 
            break;
        case OP_EOR_RR: 
            *ALU_op = EOR_OP; 
            break;
        case OP_LSL:
            *ALU_op = LSL_OP; 
            break;
        case OP_LSR: 
            *ALU_op = LSR_OP; 
            break;
        case OP_ASR: 
            *ALU_op = ASR_OP; 
            break;
        case OP_MOVK: 
        case OP_MOVZ: 
            *ALU_op = MOV_OP; 
            break;
        case OP_MVN:
            *ALU_op = INV_OP;
            break;
        default: 
            *ALU_op = PASS_A_OP; 
            break;
    }

	return;
}
/*
 * Utility functions for copying over control signals across a stage.
 * STUDENT TO-DO:
 * Copy the input signals from the input side of the pipeline
 * register to the output side of the register.
 */

comb_logic_t copy_m_ctl_sigs(m_ctl_sigs_t *dest, m_ctl_sigs_t *src) {
	assert(dest != NULL && src != NULL); 
    *dest = *src;
	return;
}

comb_logic_t copy_w_ctl_sigs(w_ctl_sigs_t *dest, w_ctl_sigs_t *src) {
	assert(dest != NULL && src != NULL);
    *dest = *src;
	return;
}
comb_logic_t extract_regs(uint32_t insnbits, opcode_t op, uint8_t *src1,
						  uint8_t *src2, uint8_t *dst) {
	assert(src1 != NULL && src2 != NULL && dst != NULL);

    if (op == OP_NOP) {
        *src1 = XZR_NUM;
        *src2 = XZR_NUM;
    }

    // Extract src1
    if (op == OP_LDUR || op == OP_STUR || op == OP_RET || op == OP_ADD_RI || 
        op == OP_ADDS_RR || op == OP_CMN_RR || op == OP_SUB_RI || 
        op == OP_SUBS_RR || op == OP_CMP_RR || op == OP_ORR_RR || 
        op == OP_EOR_RR || op == OP_ANDS_RR || op == OP_TST_RR || op == OP_LSL || op == OP_LSR) {
        *src1 = bitfield_u32(insnbits, 5, 5);
    } else {
        *src1 = XZR_NUM;
        // printf("src1 = %d", *src1);
    }


    // Extract src2
    if (op == OP_ADDS_RR || op == OP_CMN_RR || op == OP_SUBS_RR || 
        op == OP_CMP_RR || op == OP_ORR_RR || op == OP_EOR_RR || 
        op == OP_ANDS_RR || op == OP_TST_RR || op == OP_MVN) {
        *src2 = bitfield_u32(insnbits, 16, 5);
    } else {
        *src2 = XZR_NUM;
    }

    // Extract dst
    if (op == OP_LDUR || op == OP_STUR || op == OP_ADD_RI || 
        op == OP_ADDS_RR || op == OP_SUB_RI || op == OP_SUBS_RR || 
        op == OP_CMP_RR || op == OP_ORR_RR || op == OP_EOR_RR || 
        op == OP_ANDS_RR || op == OP_ADRP || op == OP_MOVZ ||
        op == OP_MOVK || op == OP_LSL || op == OP_LSR || 
        op == OP_ASR || op == OP_MVN || op == OP_UBFM || OP_MVN) {
        *dst = bitfield_u32(insnbits, 0, 5);
   } else {
        *dst = XZR_NUM;
   }

   if (op == OP_STUR) {
    *src2 = bitfield_u32(insnbits, 0, 5);
   }

   if (*src1 == SP_NUM && !(op == OP_LDUR || op == OP_STUR || op == OP_ADD_RI || op == OP_SUB_RI || op == OP_MVN || op == OP_RET ||
        op == OP_LSL || op == OP_LSR)) {
    *src1 = XZR_NUM;
   }

   if (*src2 == SP_NUM) {
    *src2 = XZR_NUM;
   }

   if (*dst == SP_NUM && !(op == OP_ADD_RI || op == OP_SUB_RI)) {
    *dst = XZR_NUM;
   }
   if (op == OP_BL) {
    *dst = 30;
   }

   if (op == OP_MOVK) {
        *src1 = *dst;
    }

    return;
}
/*
 * Decode stage logic.
 * STUDENT TO-DO:
 * Implement the decode stage.
 *
 * Use `in` as the input pipeline register,
 * and update the `out` pipeline register as output.
 * Additionally, make sure the register file is updated
 * with W_out's output when you call it in this stage.
 *
 * You will also need the following helper functions:
 * generate_DXMW_control, regfile, extract_immval,
 * and decide_alu_op.
 */

comb_logic_t decode_instr(d_instr_impl_t *in, x_instr_impl_t *out) {
    assert(in != NULL && out != NULL);

    opcode_t op = in->op;

    // Local variables for signals and values
    int64_t imm = 0;
    alu_op_t ALU_op = ERROR_OP;
    uint8_t src1 = 0, src2 = 0, dst = 0;
    bool src2_sel = false;

    // Extract immediate value
    extract_immval(in->insnbits, op, &imm);

    // Extract val_hw for instructions that need it
    uint8_t hw = 0;
    if (op == OP_MOVZ || op == OP_MOVK) {
        // For MOVZ and MOVK, hw is bits [22:21] (2 bits)
        out->val_hw = bitfield_u32(in->insnbits, 21, 2) * 16; // Extract bits 21-22
    }

    // Decide ALU operation
    decide_alu_op(op, &ALU_op);

    // Extract registers
    extract_regs(in->insnbits, op, &src1, &src2, &dst);

    // Generate control signals
        d_ctl_sigs_t d_sigs;

    generate_DXMW_control(op, &d_sigs, &out->X_sigs, &out->M_sigs, &out->W_sigs);

    // Read from register file
    uint64_t val_a = 0, val_b = 0;
    regfile(src1, src2, W_out->dst, W_wval, W_out->W_sigs.w_enable, &out->val_a, &out->val_b);
    if (dst == XZR_NUM) {
        out->W_sigs.w_enable = false;
    }
    //Comment this call to forward_reg to get correctness for week 2
    forward_reg(src1, src2, X_out->dst,
                M_out->dst, W_out->dst, M_in->val_ex,
                M_out->val_ex, W_in->val_mem,
                W_out->val_ex, W_out->val_mem, M_out->W_sigs.wval_sel,
                W_out->W_sigs.wval_sel, X_out->W_sigs.w_enable, M_out->W_sigs.w_enable, 
                W_out->W_sigs.w_enable, &out->val_a, &out->val_b);

    if (op == OP_ADRP) {
        out->val_a = in->multipurpose_val.adrp_val;
    }

    if (op == OP_MOVZ) {
        out->val_a = 0;
    }

    if (op == OP_MOVK) {
        out->val_a = out->val_a & (~(((uint64_t)0xFFFF) << out->val_hw));
    }

    if (op == OP_B_COND) {
        out->cond = bitfield_u32(in->insnbits, 0, 4);
    }

    /*call extract regs and after that call regfile using the 2 uint8_t parameters you passed in & set in the extract_regs call as the 
    first 2 parameters of regfile and &out->val_a / &out->val_b for val_a and val_b*/

    // Set output values
    out->op = in->op;
    out->print_op = in->print_op;
    out->seq_succ_PC = in->multipurpose_val.seq_succ_PC;
    out->val_imm = imm;
    out->ALU_op = ALU_op;
    out->dst = dst;
    out->status = in->status;

    return;
}