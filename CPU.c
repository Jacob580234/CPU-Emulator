#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CPU.h"

void printBinary(uint64_t num) {
    printf("\n");
    for (int i = 0; i < 64; i++) {
        if (i%8 == 0) printf(" ");
        printf("%lld", num >> (63-i) & 1);
    }
    printf("\n");
}


void printCPUState(CPU* cpu) {
    char* registers[] = {
        "RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
        "R8 ",  "R9 ",  "R10", "R11", "R12", "R13", "R14"
    };

    printf("REGISTERS:\n");
    for (int i = 0; i < 15; i++) {
        printf("%s: 0x%llX\n", registers[i], cpu->registers[i]);
    }
    printf("PC : 0x%llX\n", cpu->PC);
    printf("\nFLAGS:\nZF: %u\nOF: %u\nSF: %u\n", cpu->ZF, cpu->OF, cpu->SF);
    printf("\nSTATUS: 0x%X", cpu->stat+1);
}


uint64_t read(FILE* src, uint64_t bytesToRead, uint64_t from) {
    uint64_t result;

    char format[8];
    sprintf(format, "%%%llullX", bytesToRead * 2);
    fseek(src, from * 2, SEEK_SET);
    fscanf(src, format, &result);

    return result;
}


void fetchRegisters(CPU* cpu, FILE* src, reg* reg) {
    int registers = read(src, 1, cpu->PC + 1);
    reg->A = registers >> 4;
    reg->B = registers & 0x0F;
}


void raiseException(CPU* cpu, int code) {
    cpu->stat = code;
    printCPUState(cpu);
    printf("\n\n=====Exception raised=====\n");
    exit(EXIT_FAILURE);
}


int fetch(CPU* cpu, FILE* src, val* val, reg* reg) {

    const int instruction = read(src, 1, cpu->PC);

    switch(instruction) { // change to ifun + icode, then just default for raiseException

        case HALT:
        case NOP: {
            val->P = cpu->PC + 1;
        } break;

        case RRMOVQ:
        case CMOVLE:
        case CMOVL:
        case CMOVE:
        case CMOVNE:
        case CMOVGE:
        case CMOVG: {
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

        case ADDQ:
        case SUBQ:
        case ANDQ:
        case XORQ: {
            fetchRegisters(cpu, src, reg);
            val->P = cpu->PC + 2;
        } break;

        case JMP:
        case JLE:
        case JL:
        case JE:
        case JNE:
        case JGE:
        case JG:
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

        default: raiseException(cpu, STAT_INS);
    }

    return instruction;
}

void decode(CPU* cpu, val* val, reg* reg, int icode) {

    switch (icode) {
        case RRMOVXX: {
            val->A = cpu->registers[reg->A];
        } break;

        case (RMMOVQ >> 4): {
            val->A = cpu->registers[reg->A];
            val->B = cpu->registers[reg->B];
        } break;

        case (MRMOVQ >> 4): {
            val->B = cpu->registers[reg->B];
        } break;

        case OPQ: {
            val->A = cpu->registers[reg->A];
            val->B = cpu->registers[reg->B];
        } break;

        case (CALL >> 4): {
            val->B = cpu->registers[RSP];
        } break;

        case (RET >> 4): {
            val->A = cpu->registers[RSP];
            val->B = cpu->registers[RSP];
        } break;

        case (PUSHQ >> 4): {
            val->A = cpu->registers[reg->A];
            val->B = cpu->registers[RSP];
        } break;

        case (POPQ >> 4): {
            val->A = cpu->registers[RSP];
            val->B = cpu->registers[RSP];
        } break;

        default: break;
    }
}


bool evalCond(CPU* cpu, int icode) {
    switch (icode) {
        case LTEQ: return ((cpu->SF ^ cpu->OF) | cpu->ZF)       & 1;
        case LT:   return (cpu->SF ^ cpu->OF)                   & 1;
        case EQ:   return (cpu->ZF)                             & 1;
        case NEQ:  return (~cpu->ZF)                            & 1;
        case GTEQ: return (~(cpu->SF ^ cpu->OF))                & 1;
        case GT:   return (~(cpu->SF ^ cpu->OF) & ~cpu->ZF)     & 1;
        default: __builtin_unreachable();
    }
}


uint64_t addq(uint64_t b, uint64_t a, CPU* cpu) { // jank
    uint64_t result = 0;
    bool carry = false;
    for(int i = 0; i < 64; i++) {
        bool aSet = a >> i & 1;
        bool bSet = b >> i & 1;

        if(aSet && bSet) {
            if (carry == true) {
                result |= 1ULL << i;
            }
            carry = true;
        }
        else if(aSet != bSet) {
            if (carry == false)
                result |= 1ULL << i;
        }
        else {
            result |= (uint64_t)carry << i;
            carry = false;
        }
    }

    cpu->OF = SBIT(a) == SBIT(b) && SBIT(a) != SBIT(result);
    return result;
}


uint64_t subq(uint64_t b, uint64_t a, CPU* cpu) {
    uint64_t result = addq(~b + 1, a, cpu);
    cpu->OF = SBIT(a) != SBIT(b) && SBIT(a) != SBIT(result); // not same as a or b?
    return result;
}


uint64_t andq(uint64_t b, uint64_t a, CPU* cpu) {
    uint64_t result = 0;
    for (int i = 0; i < 64; i++) {
        bool aSet = a >> i & 1;
        bool bSet = b >> i & 1;
        if(aSet && bSet) {
            result |= 1ULL << i;
        }
    }
    cpu->OF = 0;
    return result;
}


uint64_t xorq(uint64_t b, uint64_t a, CPU* cpu) {
    uint64_t result = 0;
    for (int i = 0; i < 64; i++) {
        bool aSet = a >> i & 1;
        bool bSet = b >> i & 1;
        if(aSet != bSet) {
            result |= 1ULL << i;
        }
    }
    cpu->OF = 0;
    return result;
}


void execute(CPU* cpu, val* val, int instruction, bool* cond) { // condition

    const int icode = instruction >> 4;
    const int ifun = instruction & 0x0F;

    switch (icode) {
        case HALT: {
            raiseException(cpu, STAT_HLT);
        }

        case RRMOVXX: {
            val->E = val->A;
            if (instruction != RRMOVQ)
                *cond = evalCond(cpu, ifun);
        } break;

        case (IRMOVQ >> 4): {
            val->E = val->C;
        } break;

        case (RMMOVQ >> 4):
        case (MRMOVQ >> 4): {
            val->E = val->B + val->C;
        } break;

        case OPQ: {
            uint64_t (*opq)(uint64_t, uint64_t, CPU*);
            switch (ifun) {
                case ADDQ & 0x0F: opq = &addq; break;
                case SUBQ & 0x0F: opq = &subq; break;
                case ANDQ & 0x0F: opq = &andq; break;
                case XORQ & 0x0F: opq = &xorq; break;
                default: __builtin_unreachable();
            }
            val->E = opq(val->B, val->A, cpu);
            cpu->ZF = val->E == 0ULL;
            cpu->SF = SBIT(val->E);
        } break;

        case JXX: {
            *cond = evalCond(cpu, icode);
        } break;

        case CALL:
        case PUSHQ: {
            val->E = val->B - 8;
        } break;

        case RET:
        case POPQ: {
            val->E = val->B + 8;
        } break;

        default: break;
    }
}


uint64_t readFromMemory(uint8_t* RAM, uint64_t from) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= (uint64_t)RAM[from + i] << i*8; // little-endian
    }
    return result;
}

void writeToMemory(uint8_t* RAM, uint64_t value, uint64_t from) {
    for (int i = 0; i < 8; i++) {
        RAM[from + i] = (value >> i*8) & 0xFF; // little-endian
    }
}

void memory(uint8_t* RAM, val* val, int instruction) {
    switch(instruction) {
        case RMMOVQ: {
            writeToMemory(RAM, val->A, val->E);
        } break;

        case MRMOVQ: {
            val->M = readFromMemory(RAM, val->E);
        } break;

        case CALL: {
            writeToMemory(RAM, val->P, val->E);
        } break;

        case RET: {
            val->M = readFromMemory(RAM, val->A);
        } break;

        case PUSHQ: {
            writeToMemory(RAM, val->A, val->E);
        } break;

        case POPQ: {
            val->M = readFromMemory(RAM, val->A);
        } break;

        default: break;
    }
}


void writeback(CPU* cpu, val* val, reg* reg, bool cond, int instruction) {

    int ifun = instruction >> 4;

    switch (ifun) {
        case RRMOVXX: {
            if(cond) cpu->registers[reg->B] = val->E;
        } break;

        case IRMOVQ >> 4: {
            cpu->registers[reg->B] = val->E;
        } break;

        case MRMOVQ >> 4: {
            cpu->registers[reg->A] = val->M;
        } break;

        case OPQ: {
            cpu->registers[reg->B] = val->E;
        } break;

        case CALL >> 4:
        case RET >> 4:
        case PUSHQ >> 4: {
            cpu->registers[RSP] = val->E;
        } break;

        case POPQ >> 4: {
            cpu->registers[RSP] = val->E;
            cpu->registers[reg->A] = val->M;
        } break;

        default: break;
    }
}

void PC(CPU* cpu, val* val, bool cond, int instruction) {
    int icode = instruction >> 4;
    switch (icode) {
        case NOP >> 4:
        case RRMOVXX:
        case IRMOVQ >> 4:
        case RMMOVQ >> 4:
        case MRMOVQ >> 4:
        case OPQ:
        case PUSHQ >> 4:
        case POPQ >> 4: {
            cpu->PC = val->P;
        } break;

        case JXX: {
            cpu->PC = cond ? val->C : val->P;
        } break;

        case RET >> 4: {
            cpu->PC = val->M;
        } break;

        default: break;
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
    uint8_t* RAM = malloc(sizeof(uint8_t) * UINT32_MAX);

    memset(&cpu, 0, sizeof(CPU));

    do {
        int instruction = fetch(&cpu, src, &val, &reg);
        decode(&cpu, &val, &reg, instruction >> 4);
        execute(&cpu, &val, instruction, &cond);
        memory(RAM, &val, instruction); // still missing exception for STAT_ADR
        writeback(&cpu, &val, &reg, cond, instruction);
        PC(&cpu, &val, cond, instruction);
    } while (fgetc(src) != EOF);


    printCPUState(&cpu);


    free(RAM);
    fclose(src);

    return 0;
}