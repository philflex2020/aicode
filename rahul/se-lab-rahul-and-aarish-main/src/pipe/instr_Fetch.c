/**************************************************************************
 * C S 429 system emulator
 *
 * instr_Fetch.c - Fetch stage of instruction processing pipeline.
 **************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "hw_elts.h"
#include "instr.h"
#include "instr_pipeline.h"
#include "machine.h"

extern machine_t guest;
extern mem_status_t dmem_status;

extern uint64_t F_PC;

/*
 * Select PC logic.
 * STUDENT TO-DO:
 * Write the next PC to *current_PC.
 */

static comb_logic_t
select_PC(uint64_t pred_PC, // The predicted PC
          opcode_t D_opcode, uint64_t val_a,
          uint64_t D_seq_succ, // Possible correction from RET
          opcode_t M_opcode, bool M_cond_val,
          uint64_t seq_succ, // Possible correction from B.cond
          uint64_t *current_PC) {
  /*
   * Students: Please leave this code
   * at the top of this function.
   * You may modify below it.
   */
  if (D_opcode == OP_RET && val_a == RET_FROM_MAIN_ADDR) {
    *current_PC = 0; // PC can't be 0 normally.
    return;
  }
  // Modify starting here.

  // Mispredicted branch
  if (M_opcode == OP_B_COND && !M_cond_val) {
    *current_PC = seq_succ;
    return;
  }

  // RET instruction
  if (D_opcode == OP_RET) {
    *current_PC = val_a;
    return;
  }

  // Base case
  *current_PC = pred_PC;

  return;
}

/*
 * Predict PC logic. Conditional branches are predicted taken.
 * STUDENT TO-DO:
 * Write the predicted next PC to *predicted_PC
 * and the next sequential pc to *seq_succ.
 */

static comb_logic_t predict_PC(uint64_t current_PC, uint32_t insnbits,
                               opcode_t op, uint64_t *predicted_PC,
                               uint64_t *seq_succ) {
  /*
   * Students: Please leave this code
   * at the top of this function.
   * You may modify below it.
   */
  if (!current_PC) {
    return; // We use this to generate a halt instruction.
  }
  // Modify starting here.

  *seq_succ = current_PC + 4; // move to next PC
  // Unconditional branch
  if (op == OP_B) {
    int64_t offset = bitfield_s64(insnbits, 0, 26) << 2; // decode branch offset
    *predicted_PC = current_PC + offset;
    return;
  }

  // Conditional branch
  if (op == OP_B_COND) {
    int64_t offset = bitfield_s64(insnbits, 5, 19) << 2;
    *predicted_PC = current_PC + offset;
    return;
  }

  if (op == OP_BL) {
    int64_t offset = bitfield_s64(insnbits, 0, 26) << 2; // decode branch offset
    *predicted_PC = current_PC + offset;
    return;
  }

  // Base case
  *predicted_PC = *seq_succ; // move to next PC
  
  return;
}

/*
 * Helper function to recognize the aliased instructions:
 * LSL, LSR, CMP, CMN, and TST. We do this only to simplify the
 * implementations of the shift operations (rather than having
 * to implement UBFM in full).
 * STUDENT TO-DO
 */

static void fix_instr_aliases(uint32_t insnbits, opcode_t *op) {
  if (*op == OP_UBFM) {
    int32_t immr = bitfield_u32(insnbits, 16, 6);
    int32_t imms = bitfield_u32(insnbits, 10, 6);
    if (imms != 63) {//&& immr == 0) {
      *op = OP_LSL; // Alias for LSL
    } else if (imms == 63) { //&& immr != 0) {
      *op = OP_LSR; // Alias for LSR
    } else {
      assert(0); // Invalid UBFM usage
    }
    return;
  }
  // adds -> cmn
  if (*op == OP_ADDS_RR) {
        uint32_t Rn = bitfield_u32(insnbits, 0, 5); // bits 5-9
        if (Rn == 31) { // If Rn is XZR
            *op = OP_CMN_RR; // Alias for CMN
        }
        return;
    }

   if (*op == OP_SUBS_RR) {
        uint32_t Rn = bitfield_u32(insnbits, 0, 5); // bits 5-9
        if (Rn == 31) { // If Rn is XZR
            *op = OP_CMP_RR; // Alias for CMP
        }
        return;
    }

    if (*op == OP_ANDS_RR) {
        uint32_t Rn = bitfield_u32(insnbits, 0, 5);
        if (Rn == 31) { // If Rn is XZR
            *op = OP_TST_RR; // Alias for TST
        }
        return;
    }
}
    
/*
 * Fetch stage logic.
 * STUDENT TO-DO:
 * Implement the fetch stage.
 *
 * Use in as the input pipeline register,
 * and update the out pipeline register as output.
 * Additionally, update F_PC for the next
 * cycle's predicted PC.
 *
 * You will also need the following helper functions:
 * select_pc, predict_pc, and imem.
 */

comb_logic_t fetch_instr(f_instr_impl_t *in, d_instr_impl_t *out) {
  bool imem_err = 0;
  uint64_t current_PC;
  //select_PC();  Students: Modify this line to call select_PC with the proper params
    select_PC(in->pred_PC, X_out->op, X_out->val_a, X_out->seq_succ_PC, M_out->op, M_out->cond_holds, M_out->seq_succ_PC, &current_PC);
  /*
   * Students: This case is for generating HLT instructions
   * to stop the pipeline. Only write your code in the **else** case.
   */
  if (!current_PC || F_in->status == STAT_HLT) {
    out->insnbits = 0xD4400000U;
    out->op = OP_HLT;
    out->print_op = OP_HLT;
    imem_err = false;
  } else {
    //Student TODO: Add code here
    imem(current_PC, &out->insnbits, &imem_err);
    out->op = itable[bitfield_u32(out->insnbits, 21, 11)];
    fix_instr_aliases(out->insnbits, &out->op);
    out->print_op = out->op;
    predict_PC(current_PC, out->insnbits, out->op, &F_PC, &out->multipurpose_val.seq_succ_PC);
    if (out->op == OP_ADRP) {
      out->multipurpose_val.adrp_val = current_PC & (~((uint64_t)(0xFFF)));
    }
  }

 if (imem_err || out->op == OP_ERROR) {
        in->status = STAT_INS;
        F_in->status = in->status;
    } else if (out->op == OP_HLT) {
        in->status = STAT_HLT;
        F_in->status = in->status;
    } else {
        in->status = STAT_AOK;
    }
    out->status = in->status;

  return;
}