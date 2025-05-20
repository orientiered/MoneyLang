#ifndef EMITTERS_X86_64
#define EMITTERS_X86_64

#include <stdint.h>
#include <stdio.h>

// Intel manual 39, table 2-4
const uint8_t REX_CODE = 0b01000000; ///> 0x40
const uint8_t REX_W    = REX_CODE | 0b00001000; ///> 0 = Operand size determined by CS.D
                                                ///> 1 = 64 Bit Operand Size
const uint8_t REX_R    = REX_CODE | 0b00000100; ///> Extension of the ModR/M reg field
const uint8_t REX_X    = REX_CODE | 0b00000010; ///> Extension of the SIB index field
const uint8_t REX_B    = REX_CODE | 0b00000001; ///> Extension of the ModR/M r/m field, SIB base field, or Opcode reg fiel

#define _REX(W, R, X, B) (REX_CODE | REX_W*W | REX_R*R | REX_X*X | REX_B * B)

const size_t MAX_OPCODE_LEN = 16;

enum REGS {
    R_RAX = 0,
    R_RCX = 1,
    R_RDX = 2,
    R_RBX = 3,
    R_RSP = 4,
    R_RBP = 5,
    R_RSI = 6,
    R_RDI = 7,

    R_R8  = 0 + 8,
    R_R9  = 1 + 8,
    R_R10 = 2 + 8,
    R_R11 = 3 + 8,
    R_R12 = 4 + 8,
    R_R13 = 5 + 8,
    R_R14 = 6 + 8,
    R_R15 = 7 + 8
};
typedef enum REGS REG_t;

const struct {
    REG_t reg;
    const char *str;
} REG_STRINGS[] = {
    {R_RAX, "rax"},
    {R_RCX, "rcx"},
    {R_RDX, "rdx"},
    {R_RBX, "rbx"},
    {R_RSP, "rsp"},
    {R_RBP, "rbp"},
    {R_RSI, "rsi"},
    {R_RDI, "rdi"},

    {R_R8 , "r8" },
    {R_R9 , "r9" },
    {R_R10, "r10"},
    {R_R11, "r11"},
    {R_R12, "r12"},
    {R_R13, "r13"},
    {R_R14, "r14"},
    {R_R15, "r15"}
};


typedef struct {
    FILE *binFile;
    FILE *asmFile;
    bool emitting;
} emitCtx_t;

int32_t emitPushReg64(emitCtx_t *ctx, REG_t reg);
int32_t emitPushMemBaseDisp32(emitCtx_t *ctx, REG_t base, int32_t disp);

int32_t emitPopReg64(emitCtx_t *ctx, REG_t reg);
int32_t emitPopMemBaseDisp32(emitCtx_t *ctx, REG_t base, int32_t disp);


int32_t emitMovRegReg64(emitCtx_t *ctx, REG_t dest, REG_t src);
int32_t emitMovRegImm64(emitCtx_t *ctx, REG_t dest, uint64_t imm);

int32_t emitAddReg64Imm32(emitCtx_t *ctx, REG_t dest, uint32_t imm);
int32_t emitSubReg64Imm32(emitCtx_t *ctx, REG_t dest, uint32_t imm);

int32_t emitRet(emitCtx_t *ctx);
int32_t emitTest(emitCtx_t *ctx, REG_t dest, REG_t src);

int32_t emitJmp(emitCtx_t *ctx, int32_t offset);
int32_t emitJz(emitCtx_t *ctx, int32_t offset);

int32_t emitCall(emitCtx_t *ctx, int32_t offset);



#endif
