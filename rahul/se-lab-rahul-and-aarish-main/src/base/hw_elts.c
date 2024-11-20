/**************************************************************************
 * C S 429 system emulator
 *
 * hw_elts.c - Module for emulating hardware elements.
 *
 * Copyright (c) 2022, 2023, 2024.
 * Authors: S. Chatterjee, Z. Leeper., P. Jamadagni
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/

#include <assert.h>
#include "hw_elts.h"
#include "mem.h"
#include "machine.h"
#include "err_handler.h"

extern machine_t guest;

comb_logic_t
imem(uint64_t imem_addr,
     uint32_t *imem_rval, bool *imem_err)
{
    // imem_addr must be in "instruction memory" and a multiple of 4
    // Students: DO NOT MODIFY
    *imem_err = (!addr_in_imem(imem_addr) || (imem_addr & 0x3U));
    *imem_rval = (uint32_t)mem_read_I(imem_addr);
}
#define NUM_GPRS 32
#define XZR_NUM 31

comb_logic_t
regfile(uint8_t src1, uint8_t src2, uint8_t dst, uint64_t val_w,
        bool w_enable,
        uint64_t *val_a, uint64_t *val_b)
{
    src1 &= 0x1F;
    src2 &= 0x1F;
    dst &= 0x1F;

    // Write operation
    if (w_enable)
    {
        if (dst < NUM_GPRS && dst != XZR_NUM)
        { // Prevent writing to XZR
            guest.proc->GPR[dst] = val_w;
        }
        // Optionally, handle writes to XZR (no effect) or invalid registers
        else if (dst >= NUM_GPRS)
        {
            fprintf(stderr, "Error: Invalid destination register X%d in regfile.\n", dst);
            // Handle error as per your emulator's design
        }
    }

    // Read operations
    // Read src1
    if (src1 >= NUM_GPRS)
    {
        *val_a = 0;
        fprintf(stderr, "Warning: Invalid source register X%d. Treated as XZR.\n", src1);
    }
    else if (src1 == XZR_NUM)
    {
        *val_a = 0; // XZR always reads as zero
    }
    else
    {
        *val_a = guest.proc->GPR[src1];
    }

    // Read src2
    if (src2 >= NUM_GPRS)
    {
        *val_b = 0;
        fprintf(stderr, "Warning: Invalid source register X%d. Treated as XZR.\n", src2);
    }
    else if (src2 == XZR_NUM)
    {
        *val_b = 0; // XZR always reads as zero
    }
    else
    {
        *val_b = guest.proc->GPR[src2];
    }
}
static bool
cond_holds(cond_t cond, uint8_t ccval)
{
    // Student TODO: Check flags against condition value

    bool N = (ccval & 0x8) != 0;
    bool Z = (ccval & 0x4) != 0;
    bool C = (ccval & 0x2) != 0;
    bool V = (ccval & 0x1) != 0;

    switch (cond)
    {
    case C_EQ:
        return Z; // Equal
    case C_NE:
        return !Z; // Not equal
    case C_CS:
        return C; // Carry set (unsigned >=)
    case C_CC:
        return !C; // Carry clear (unsigned <)
    case C_MI:
        return N; // Negative
    case C_PL:
        return !N; // Positive or zero
    case C_VS:
        return V; // Overflow
    case C_VC:
        return !V; // No overflow
    case C_HI:
        return C && !Z; // Unsigned >
    case C_LS:
        return !C || Z; // Unsigned <=
    case C_GE:
        return N == V; // Signed >=
    case C_LT:
        return N != V; // Signed <
    case C_GT:
        return !Z && (N == V); // Signed >
    case C_LE:
        return Z || (N != V); // Signed <=
    case C_AL:
        return true; // Always
    case C_NV:
        return true; // Never
    default:
        return false; // Undefined condition
    }
    // return false;
}

comb_logic_t
alu(uint64_t alu_vala, uint64_t alu_valb, uint8_t alu_valhw, alu_op_t ALUop, bool set_flags, cond_t cond,
    uint64_t *val_e, bool *cond_val, uint8_t *nzcv)
{
    uint64_t res = 0xFEEDFACEDEADBEEF; // To make it easier to detect errors.
    // Student TODO: Handle arithmetic/logical operations here. Refer to the alu_op_t enum
    bool N = false, Z = false, C = false, V = false;

    switch (ALUop)
    {
    case PLUS_OP:
        res = alu_vala + alu_valb;
        C = (res < alu_vala); // Unsigned carry
        V = (((alu_vala ^ res) & (alu_valb ^ res) & (1ULL << 63)) != 0);
        break;

    case MINUS_OP:
        res = alu_vala - alu_valb;
        C = (alu_vala >= alu_valb); // No borrow
        bool min_neg = (alu_valb == (1ULL << 63));
        V = min_neg || (((alu_vala ^ alu_valb) & (alu_vala ^ res) & (1ULL << 63)) != 0);
        break;

    case INV_OP:
        res = alu_vala | (~alu_valb);
        break;
    case OR_OP:
        res = alu_vala | alu_valb;
        break;
    case EOR_OP:
        res = alu_vala ^ alu_valb;
        break;
    case AND_OP:
        res = alu_vala & alu_valb;
        break;
    case MOV_OP:
        res = alu_vala | (alu_valb << alu_valhw);
        break;
    case LSL_OP:
        res = alu_vala << (alu_valb & 0x3F);            // Mask to 6 bits for shift
        C = (alu_vala >> (64 - (alu_valb & 0x3F))) & 1; // Carry out from shift
        break;
    case LSR_OP:
        res = alu_vala >> (alu_valb & 0x3F);           // Mask to 6 bits for shift
        C = (alu_vala >> ((alu_valb & 0x3F) - 1)) & 1; // Carry out from shift
        break;
    case ASR_OP:
        res = (int64_t)alu_vala >> (alu_valb & 0x3F);  // Mask to 6 bits for shift
        C = (alu_vala >> ((alu_valb & 0x3F) - 1)) & 1; // Carry out from shift
        break;
    case PASS_A_OP:
        res = alu_vala;
        break;
    default:
        break; // Handle any additional ALU operations here
    }

    // Set NZCV flags if requested
    if (set_flags)
    {
        N = (res >> 63) & 1;
        Z = (res == 0);
        *nzcv = (N << 3) | (Z << 2) | (C << 1) | V;
    }

    // Store result and condition value
    *val_e = res;
    *cond_val = cond_holds(cond, *nzcv);
}

comb_logic_t
dmem(uint64_t dmem_addr, uint64_t dmem_wval, bool dmem_read, bool dmem_write,
     uint64_t *dmem_rval, bool *dmem_err)
{
    // Students: DO NOT MODIFY
    if (!dmem_read && !dmem_write)
    {
        return;
    }
    // dmem_addr must be in "data memory" and a multiple of 8
    *dmem_err = (!addr_in_dmem(dmem_addr) || (dmem_addr & 0x7U));
    if (is_special_addr(dmem_addr))
        *dmem_err = false;
    if (dmem_read)
        *dmem_rval = (uint64_t)mem_read_L(dmem_addr);
    if (dmem_write)
        mem_write_L(dmem_addr, dmem_wval);
}
