#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "emitters_x86_64.h"

/* All emitters return size in bytes of created instruction */
/* They will write instruction to file only if ctx->emitting is true */

/* Useful defines */
#define asm_emit(...) if (ctx->emitting) fprintf(ctx->asmFile, __VA_ARGS__)
#define bin_emit() if (ctx->emitting) fwrite(opcode, 1, size, ctx->binFile)
#define TRUNC(reg) ((reg >= R_R8) ? reg-8 : reg)
/* ----------------------------------------------------------- */
const uint8_t MOD_RM_REG = 0b11;

static uint8_t modRM(uint8_t mod, uint8_t reg, uint8_t rm) {
    assert(mod < 0b100);
    assert(reg < 0b1000);
    assert(rm < 0b1000);

    return (mod << 6) | (reg << 3) | (rm);
}

/* ------------------------- Emitters ------------------------ */

int32_t emitPushReg64(emitCtx_t *ctx, REG_t reg) {
    assert(ctx);
    assert(reg <= R_R15);

    asm_emit("\tpush %s\n", REG_STRINGS[reg].str);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    if (reg >= R_R8)
        opcode[size++] = REX_B;

    opcode[size++] = 0x50 + TRUNC(reg);

    bin_emit();
    return size;
}

int32_t emitPushMemBaseDisp32(emitCtx_t *ctx, REG_t base, int32_t disp) {
    assert(ctx);
    assert(base <= R_R15);

    asm_emit("\tpush [%s + (%d)]\n", REG_STRINGS[base].str, disp);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    if (base >= R_R8)
        opcode[size++] = REX_B;

    opcode[size++] = 0xFF; //  push opcode
    opcode[size++] = modRM(0b10, 6, TRUNC(base)); // reg = /6
    *(uint32_t*)(opcode+size) = (uint32_t) disp; // immediate
    size += 4;

    bin_emit();
    return size;
}


int32_t emitPopReg64(emitCtx_t *ctx, REG_t reg) {
     assert(ctx);
    assert(reg <= R_R15);

    asm_emit("\tpop %s\n", REG_STRINGS[reg].str);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    if (reg >= R_R8)
        opcode[size++] = REX_B;

    opcode[size++] = 0x58 + TRUNC(reg);

    bin_emit();
    return size;
}

int32_t emitPopMemBaseDisp32(emitCtx_t *ctx, REG_t base, int32_t disp) {
    assert(ctx);
    assert(base <= R_R15);

    asm_emit("\tpop [%s + (%d)]\n", REG_STRINGS[base].str, disp);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    if (base >= R_R8)
        opcode[size++] = REX_B;

    opcode[size++] = 0x8F; //  pop opcode
    opcode[size++] = modRM(0b10, 0, TRUNC(base)); // reg = /0
    *(uint32_t*)(opcode+size) = (uint32_t) disp; // immediate
    size += 4;

    bin_emit();
    return size;
}

/* ======================================================================== */

int32_t emitMovRegReg64(emitCtx_t *ctx, REG_t dest, REG_t src) {
    assert(ctx);
    assert(dest <= R_R15); assert(src <= R_R15);

    asm_emit("\tmov  %s, %s\n", REG_STRINGS[dest].str, REG_STRINGS[src].str);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    uint8_t rex = REX_W | (REX_R * (src >= R_R8)) | (REX_B * (dest >= R_R8));
    opcode[size++] = rex;
    opcode[size++] = 0x89; // mov r/m64, r64 opcode
    opcode[size++] = modRM(MOD_RM_REG, TRUNC(src), TRUNC(dest));

    bin_emit();
    return size;
}


int32_t emitMovRegImm64(emitCtx_t *ctx, REG_t dest, uint64_t imm) {
    assert(ctx); assert(dest <= R_R15);

    asm_emit("\tmov  %s, 0x%lX\n", REG_STRINGS[dest].str, imm);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    uint8_t rex = REX_W | (REX_B * (dest >= R_R8));
    opcode[size++] = rex;
    opcode[size++] = 0xB8 + TRUNC(dest); // mov r64, imm64 opcode
    *(uint64_t*)(opcode+size) = imm; // immediate
    size += 8;

    bin_emit();
    return size;
}


int32_t emitAddReg64Imm32(emitCtx_t *ctx, REG_t dest, uint32_t imm) {
    assert(ctx); assert(dest <= R_R15);

    asm_emit("\tadd  %s, 0x%X\n", REG_STRINGS[dest].str, imm);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    bool highReg = (dest >= R_R8);
    uint8_t rex = REX_W | (REX_B * highReg);
    opcode[size++] = rex;
    opcode[size++] = 0x81; // add r64, imm32 opcode
    opcode[size++] = modRM(MOD_RM_REG, 0, TRUNC(dest)); // add requires /0 in reg
    *(uint32_t*)(opcode+size) = imm; // immediate
    size += 4;

    bin_emit();
    return size;
}


int32_t emitSubReg64Imm32(emitCtx_t *ctx, REG_t dest, uint32_t imm) {
    assert(ctx); assert(dest <= R_R15);

    asm_emit("\tsub  %s, 0x%X\n", REG_STRINGS[dest].str, imm);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    bool highReg = (dest >= R_R8);
    uint8_t rex = REX_W | (REX_B * highReg);
    opcode[size++] = rex;
    opcode[size++] = 0x81; // sub r64, imm32 opcode
    opcode[size++] = modRM(MOD_RM_REG, 5, TRUNC(dest)); // sub requires /5 in reg
    *(uint32_t*)(opcode+size) = imm; // immediate
    size += 4;

    bin_emit();
    return size;
}


int32_t emitRet(emitCtx_t *ctx) {
    assert(ctx);

    asm_emit("\tret\n");
    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;
    opcode[size++] = 0xC3;

    bin_emit();
    return 1;
}


int32_t emitTest(emitCtx_t *ctx, REG_t dest, REG_t src) {
    assert(ctx);
    assert(dest <= R_R15); assert(src <= R_R15);

    asm_emit("\ttest %s, %s\n", REG_STRINGS[dest].str, REG_STRINGS[src].str);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    uint8_t rex = REX_W | (REX_R * (src >= R_R8)) | (REX_B * (dest >= R_R8));
    opcode[size++] = rex;
    opcode[size++] = 0x85; // test r/m64, r64 opcode
    opcode[size++] = modRM(MOD_RM_REG, TRUNC(src), TRUNC(dest));

    bin_emit();
    return size;
}


/* Jmp and Jz are rel32 for simplicty*/

int32_t emitJmp(emitCtx_t *ctx, int32_t offset) {
    assert(ctx);

    // TODO: it might be incorrect
    asm_emit("\tjmp $ + 5 + %X\n", (uint32_t) offset);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    opcode[size++] = 0xE9; // jmp rel32 opcode
    *(uint32_t*)(opcode+size) = (uint32_t) offset; // immediate
    size += 4;

    bin_emit();
    return size;
}


int32_t emitJz(emitCtx_t *ctx, int32_t offset) {
    assert(ctx);

    // TODO: it might be incorrect
    asm_emit("\tjz $ + 5 + %X\n", (uint32_t) offset);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    opcode[size++] = 0x0F; //jz rel32 opcode
    opcode[size++] = 0x84;
    *(uint32_t*)(opcode+size) = (uint32_t) offset; // offset
    size += 4;

    bin_emit();
    return size;
}


int32_t emitCall(emitCtx_t *ctx, int32_t offset) {
    assert(ctx);

    // TODO: it might be incorrect
    asm_emit("\tjmp $ + 5 + %X\n", (uint32_t) offset);

    uint8_t opcode[MAX_OPCODE_LEN] = {};
    int32_t size = 0;

    opcode[size++] = 0xE8; // jmp rel32 opcode
    *(uint32_t*)(opcode+size) = (uint32_t) offset; // immediate
    size += 4;

    bin_emit();
    return size;
}
