#include <assert.h>
#include <pthread_time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void printBinary(uint64_t num) {
    printf("\n");
    for (int i = 0; i < 64; i++) {
        if (i%8 == 0) printf(" ");
        printf("%llu", num >> (63-i) & 1);
    }
    printf("\n");
}

#define STAT_AOK 0x00 // ok
#define STAT_HLT 0x01 // halt
#define STAT_ADR 0x02 // invalid address
#define STAT_INS 0x03 // invalid instruction


#define HALT   0x0
#define NOP    0x1

#define RRMOVQ 0x2
#define CMOVLE 0x1
#define CMOVL  0x2
#define CMOVE  0x3
#define CMOVNE 0x4
#define CMOVGE 0x5
#define CMOVG  0x6

#define IRMOVQ 0x3
#define RMMOVQ 0x4
#define MRMOVQ 0x5

#define OPQ    0x6
#define ADDQ   0x0
#define SUBQ   0x1
#define ANDQ   0x2
#define XORQ   0x3

#define JXX    0x7
#define JMP    0x0
#define JLE    0x1
#define JL     0x2
#define JE     0x3
#define JNE    0x4
#define JGE    0x5
#define JG     0x6

#define LTEQ 0x1
#define LT   0x2
#define EQ   0x3
#define NEQ  0x4
#define GTEQ 0x5
#define GT   0x6

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
    int64_t registers[15];
    int64_t PC; // value is stored in 4-bit increments
    bool ZF : 1;
    bool OF : 1;
    bool SF : 1;
    unsigned int stat : 2;
} CPU;

typedef struct {
    int64_t A;
    int64_t B;
    int64_t C;
    int64_t E;
    int64_t M;
    int64_t P;
    int64_t D;
} val;

typedef struct {
    int64_t A;
    int64_t B;
} reg;

void printCPUState(CPU* cpu) {
    char* registers[] = {
        "RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
        "R8",  "R9",  "R10", "R11", "R12", "R13", "R14"
    };

    printf("REGISTERS:\n");
    for (int i = 0; i < 15; i++) {
        printf("%s: %llX\n", registers[i], cpu->registers[i]);
    }
    printf("PC: 0x%llX\n", cpu->PC);
    printf("\nFLAGS:\nZF: %u\nOF: %u\nSF: %u\n", cpu->ZF, cpu->OF, cpu->SF);
    printf("\nSTATUS: 0x%X", cpu->stat+1);
}

/*
void updatePC(CPU* cpu, FILE* src, unsigned int addr) {
    cpu->PC = addr;
    fseek(src, cpu->PC, SEEK_SET);
}
*/


// can do spaces and newlines between read bytes, but NOT in between !!
uint64_t read(FILE* src, int bytesToRead, int from) {
    uint64_t result;

    char format[8];
    sprintf(format, "%%%dllX", bytesToRead * 2);
    fseek(src, from * 2, SEEK_SET);
    fscanf(src, format, &result);

    return result;
}

void fetchRegisters(CPU* cpu, FILE* src, reg* reg) {
    int registers = read(src, 1, cpu->PC + 1);
    reg->A = registers >> 4;
    reg->B = registers & 0x0F;
}


int fetch(CPU* cpu, FILE* src, val* val, reg* reg) {

    const int instruction = read(src, 1, cpu->PC);
    const int ifun  = instruction >> 4;

    switch(ifun) {

        case HALT:
        case NOP: {
            val->P = cpu->PC + 1;
        } break;

        case RRMOVQ: {
            fetchRegisters(cpu, src, reg);
            val->P = cpu->PC + 2;
        } break;

        case IRMOVQ:
        case RMMOVQ:
        case MRMOVQ: {
            fetchRegisters(cpu, src, reg);
            val->C = read(src, 8, cpu->PC + 2);
            val->P = cpu->PC + 10;
        } break;

        case OPQ: {
            fetchRegisters(cpu, src, reg);
            val->P = cpu->PC + 2;
        } break;

        case JXX:
        case CALL: {
            val->C = read(src, 8, cpu->PC + 1);
            val->P = cpu->PC + 9;
        } break;

        case RET: {
            val->P = cpu->PC + 1;
        } break;

        case PUSHQ:
        case POPQ: {
            fetchRegisters(cpu, src, reg);
            val->P = cpu->PC + 2;
        } break;

        default: cpu->stat = STAT_INS;
    }

    return instruction;
}

void decode(CPU* cpu, val* val, reg* reg, int icode) {

    switch (icode) {
        case RRMOVQ: {
            val->A = cpu->registers[reg->A];
        } break;

        case RMMOVQ: {
            val->A = cpu->registers[reg->A];
            val->B = cpu->registers[reg->B];
        } break;

        case MRMOVQ: {
            val->B = cpu->registers[reg->B];
        } break;

        case OPQ: {
            val->A = cpu->registers[reg->A];
            val->B = cpu->registers[reg->B];
        } break;

        case CALL: {
            val->B = cpu->registers[RSP];
        } break;

        case RET: {
            val->A = cpu->registers[RSP];
            val->B = cpu->registers[RSP];
        } break;

        case PUSHQ: {
            val->A = cpu->registers[reg->A];
            val->B = cpu->registers[RSP];
        } break;

        case POPQ: {
            val->A = cpu->registers[RSP];
            val->B = cpu->registers[RSP];
        } break;

        default: break;
    }
}

bool evalCond(CPU* cpu, int icode) {
    switch (icode) {
        case LTEQ: return (cpu->SF ^ cpu->OF) | cpu->ZF;
        case LT:   return cpu->SF ^ cpu->OF;
        case EQ:   return cpu->ZF;
        case NEQ:  return ~cpu->ZF;
        case GTEQ: return ~(cpu->SF ^ cpu->OF);
        case GT:   return ~(cpu->SF ^ cpu->OF) & ~cpu->ZF;

        default: cpu->stat = STAT_INS; break;
    }
}

int execute(CPU* cpu, val* val, int instruction, bool* cond) { // condition

    const int icode = instruction >> 4;
    const int ifun = instruction & 0x0F;
    switch (icode) {

        case HALT: {
            cpu->stat = STAT_HLT;
        } break;

        case RRMOVQ: {
            val->E = val->A;
            if (ifun != 0x0)
                *cond = evalCond(cpu, ifun);
        } break;




    }
}

int main(int argc, char** argv) {

    /*
    if(argc != 2) {
        printf("Usage: ./CPU_Emulator <source program>\n");
        exit(EXIT_FAILURE);
    }
    */

    FILE* src = fopen("../example_program.txt", "r");
    CPU cpu;
    reg reg;
    val val;
    bool cond;

    memset(&cpu, 0, sizeof(CPU));

    int instruction = fetch(&cpu, src, &val, &reg);
    decode(&cpu, &val, &reg, instruction >> 4);
    execute(&cpu, &val, instruction, &cond);

    uint8_t memory[65536];

    return 0;
}