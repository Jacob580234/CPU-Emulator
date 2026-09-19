//
// Created by Jacob on 09-09-2026.
//

#ifndef CPU_EMULATOR_CPU_H
#define CPU_EMULATOR_CPU_H

#include <stdint.h>

#define STAT_AOK 0x00 // ok
#define STAT_HLT 0x01 // halt
#define STAT_ADR 0x02 // invalid address
#define STAT_INS 0x03 // invalid instruction


#define HALT 0x0
#define NOP  0x1

#define RRMOVXX 0x2
#define RRMOVQ  0x00
#define CMOVLE  0x01
#define CMOVL   0x02
#define CMOVE   0x03
#define CMOVNE  0x04
#define CMOVGE  0x05
#define CMOVG   0x06

#define IRMOVQ 0x3
#define RMMOVQ 0x4
#define MRMOVQ 0x5

#define OPQ    0x6
#define ADDQ   0x00
#define SUBQ   0x01
#define ANDQ   0x02
#define XORQ   0x03

#define JXX    0x7
#define JMP    0x00
#define JLE    0x01
#define JL     0x02
#define JE     0x03
#define JNE    0x04
#define JGE    0x05
#define JG     0x06

#define LTEQ   0x01
#define LT     0x02
#define EQ     0x03
#define NEQ    0x04
#define GTEQ   0x05
#define GT     0x06

#define CALL   0x8
#define RET    0x9
#define PUSHQ  0xA
#define POPQ   0xB


#define RAX         0x00
#define RCX         0x01
#define RDX         0x02
#define RBX         0x03
#define RSP         0x04
#define RBP         0x05
#define RSI         0x06
#define RDI         0x07
#define R8          0x08
#define R9          0x09
#define R10         0x0A
#define R11         0x0B
#define R12         0x0C
#define R13         0x0D
#define R14         0x0E
#define NO_REGISTER 0x0F

#define CODE_SEG  0x00100000 // lowest address: smallest (64kb-1mb)
#define DATA_SEG  0x10000000 // second lowest address: small (64kb-256kb)
#define STACK_SEG 0xFFFFFFF0 // highest address, grows down: biggest (256kb-4mb)

#define SBIT(n) ((n) >> 63 & 1)

typedef enum: unsigned int { false, true } bool;

typedef struct {
    /*
    union { int64_t rax;  int32_t eax;  int16_t ax;   int8_t al;   };
    union { int64_t rbx;  int32_t ebx;  int16_t bx;   int8_t bl;   };
    union { int64_t rcx;  int32_t ecx;  int16_t cx;   int8_t cl;   };
    union { int64_t rdx;  int32_t edx;  int16_t dx;   int8_t dl;   };
    union { int64_t rsi;  int32_t esi;  int16_t si;   int8_t sil;  };
    union { int64_t rdi;  int32_t edi;  int16_t di;   int8_t dil;  };
    union { int64_t rbp;  int32_t ebp;  int16_t bp;   int8_t bpl;  };
    union { int64_t rsp;  int32_t esp;  int16_t sp;   int8_t spl;  };
    union { int64_t r8;   int32_t r8d;  int16_t r8w;  int8_t r8b;  };
    union { int64_t r9;   int32_t r9d;  int16_t r9w;  int8_t r9b;  };
    union { int64_t r10;  int32_t r10d; int16_t r10w; int8_t r10b; };
    union { int64_t r11;  int32_t r11d; int16_t r11w; int8_t r11b; };
    union { int64_t r12;  int32_t r12d; int16_t r12w; int8_t r12b; };
    union { int64_t r13;  int32_t r13d; int16_t r13w; int8_t r13b; };
    union { int64_t r14;  int32_t r14d; int16_t r14w; int8_t r14b; };
    */
    uint64_t registers[15];
    uint64_t PC; // value is stored in 4-bit increments
    bool ZF : 1;
    bool OF : 1;
    bool SF : 1;
    unsigned int stat : 2;
} CPU;

typedef struct {
    uint64_t A;
    uint64_t B;
    uint64_t C;
    uint64_t E;
    uint64_t M;
    uint64_t P;
    uint64_t D;
} val;

typedef struct {
    uint64_t A;
    uint64_t B;
} reg;

typedef uint8_t byte;

uint64_t readFromMemory(uint8_t* RAM, uint64_t from, int bytesToRead);
void writeToMemory(uint8_t* RAM, uint64_t value, uint64_t from);

#endif //CPU_EMULATOR_CPU_H