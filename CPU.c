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


void fetchRegisters(CPU* cpu, byte* RAM, reg* reg) {
    int registers = readFromMemory(RAM, cpu->PC, 1);
    reg->A = registers >> 4;
    reg->B = registers & 0x0F;
}


void raiseException(CPU* cpu, int code) {
    cpu->stat = code;
    printCPUState(cpu);
    exit(EXIT_SUCCESS);
}


int fetch(CPU* cpu, uint8_t* RAM, val* val, reg* reg) { // needs some sort of assert for no-register (F)

    if(cpu->PC < CODE_SEG || cpu->PC > DATA_SEG)
        raiseException(cpu, STAT_ADR);

    const int instruction = readFromMemory(RAM, cpu->PC, 1);
    const int icode = instruction >> 4;
    const int ifun = instruction & 0x0F;

    if(icode == HALT && ifun == 0x0) {}
    else if(icode == NOP  && ifun == 0x0) {
        val->P = cpu->PC + 1;
    } else if(icode == RRMOVXX && (ifun == RRMOVQ || ifun == CMOVLE || ifun == CMOVL || ifun == CMOVE || ifun == CMOVNE || ifun == CMOVGE || ifun == CMOVG)) {
        fetchRegisters(cpu, RAM, reg);
        val->P = cpu->PC + 2;
    } else if((icode == IRMOVQ || icode == RMMOVQ || icode == MRMOVQ) && ifun == 0x0) {
        fetchRegisters(cpu, RAM, reg);
        val->C = readFromMemory(RAM, cpu->PC + 2, 8);
        val->P = cpu->PC + 10;
    } else if(icode == OPQ && (ifun == ADDQ || ifun == SUBQ || ifun == ANDQ || ifun == XORQ)) {
        fetchRegisters(cpu, RAM, reg);
        val->P = cpu->PC + 2;
    } else if(icode == JXX && (ifun == JMP || ifun == JLE || ifun == JL || ifun == JE || ifun == JNE || ifun == JGE || ifun == JG)) {
        val->C = readFromMemory(RAM, cpu->PC + 1, 8);
        val->P = cpu->PC + 9;
    } else if(icode == CALL && ifun == 0x0) {
        val->C = readFromMemory(RAM, cpu->PC + 1, 8);
        val->P = cpu->PC + 9;
    } else if(icode == RET && ifun == 0x0) {
        val->P = cpu->PC + 1;
    } else if((icode == PUSHQ || icode == POPQ) && ifun == 0x0) {
        fetchRegisters(cpu, RAM, reg);
        val->P = cpu->PC + 2;
    }
    else raiseException(cpu, STAT_INS);

    return instruction;
}

void decode(CPU* cpu, val* val, reg* reg, int icode) {

    switch (icode) {
        case RRMOVXX: {
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


bool evalCond(CPU* cpu, int ifun) {
    switch (ifun) {
        case LTEQ: return ((cpu->SF ^ cpu->OF) | cpu->ZF)       & 1;
        case LT:   return (cpu->SF ^ cpu->OF)                   & 1;
        case EQ:   return (cpu->ZF)                             & 1;
        case NEQ:  return (~cpu->ZF)                            & 1;
        case GTEQ: return (~(cpu->SF ^ cpu->OF))                & 1;
        case GT:   return (~(cpu->SF ^ cpu->OF) & ~cpu->ZF)     & 1;
        default: __builtin_unreachable();
    }
}


uint64_t addq(uint64_t b, uint64_t a, CPU* cpu) { // TODO: clean up
    uint64_t result = 0;
    bool carry = false;
    for(int i = 0; i < 64; i++) {
        bool aSet = a >> i & 1;
        bool bSet = b >> i & 1;

        if(aSet && bSet) {
            if (carry == true) result |= 1ULL << i;
            carry = true;
        }
        else if(aSet != bSet) {
            if (carry == false) result |= 1ULL << i;
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


void execute(CPU* cpu, val* val, int icode, int ifun, bool* cond) {

    switch (icode) {
        case HALT: raiseException(cpu, STAT_HLT);
        case RRMOVXX: {
            val->E = val->A;
            if(ifun != RRMOVQ) *cond = evalCond(cpu, ifun);
        } break;
        case IRMOVQ: val->E = val->C; break;
        case RMMOVQ: val->E = val->B + val->C; break;
        case MRMOVQ: val->E = val->B + val->C; break;
        case OPQ: {
            uint64_t (*opq)(uint64_t, uint64_t, CPU*);
            switch (ifun) {
                case ADDQ: opq = &addq; break;
                case SUBQ: opq = &subq; break;
                case ANDQ: opq = &andq; break;
                case XORQ: opq = &xorq; break;
                default: __builtin_unreachable();
            }
            val->E = opq(val->B, val->A, cpu);
            cpu->ZF = val->E == 0ULL;
            cpu->SF = SBIT(val->E);
        } break;
        case JXX:   *cond = evalCond(cpu, ifun); break;
        case CALL:  val->E = val->B - 8; break;
        case RET:   val->E = val->B + 8; break;
        case PUSHQ: val->E = val->B - 8; break;
        case POPQ:  val->E = val->B + 8; break;

        default: break;
    }

}


uint64_t readFromMemory(uint8_t* RAM, uint64_t from, int bytesToRead) {
    uint64_t result = 0;
    for (int i = 0; i < bytesToRead; i++) {
        result |= (uint64_t)RAM[from + i] << i*8; // little-endian
    }
    return result;
}

void writeToMemory(uint8_t* RAM, uint64_t value, uint64_t from) {
    for (int i = 0; i < 8; i++) {
        RAM[from + (7-i)] = (value >> i*8) & 0xFF; // writes value to memory as-is
    }
}


void memory(uint8_t* RAM, val* val, int icode) { // missing exception for STAT_ADR

    switch(icode) {
        case RMMOVQ: writeToMemory(RAM, val->A, val->E);      break;
        case MRMOVQ: val->M = readFromMemory(RAM, val->E, 8); break;
        case CALL:   writeToMemory(RAM, __builtin_bswap64(val->P), val->E); break; // PC value needs to be little-endian'd before being written to memory
        case RET:    val->M = readFromMemory(RAM, val->A, 8); break;
        case PUSHQ:  writeToMemory(RAM, val->A, val->E);      break;
        case POPQ:   val->M = readFromMemory(RAM, val->A, 8); break;
        default: break;
    }

}


void writeback(CPU* cpu, val* val, reg* reg, bool cond, int icode) {

    switch (icode) {
        case RRMOVXX: if(cond) cpu->registers[reg->B] = val->E; break;
        case IRMOVQ:  cpu->registers[reg->B] = val->E; break;
        case MRMOVQ:  cpu->registers[reg->A] = val->M; break;
        case OPQ:     cpu->registers[reg->B] = val->E; break;
        case CALL:    cpu->registers[RSP] = val->E; break;
        case RET:     cpu->registers[RSP] = val->E; break;
        case PUSHQ:   cpu->registers[RSP] = val->E; break;
        case POPQ: {
            cpu->registers[RSP] = val->E;
            cpu->registers[reg->A] = val->M;
        } break;

        default: break;
    }
}


void PC(CPU* cpu, val* val, bool cond, int icode) {

    switch (icode) { // HALT absent given it never reaches this point courtesy of it exiting the program
        case NOP:     cpu->PC = val->P; break;
        case RRMOVXX: cpu->PC = val->P; break;
        case IRMOVQ:  cpu->PC = val->P; break;
        case RMMOVQ:  cpu->PC = val->P; break;
        case MRMOVQ:  cpu->PC = val->P; break;
        case OPQ:     cpu->PC = val->P; break;
        case JXX:     cpu->PC = cond ? val->C : val->P; break;
        case CALL:    cpu->PC = val->C; break;
        case RET:     cpu->PC = val->M; break;
        case PUSHQ:   cpu->PC = val->P; break;
        case POPQ:    cpu->PC = val->P; break;

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
    cpu.stat = STAT_AOK;
    cpu.PC = CODE_SEG;
    cpu.registers[RSP] = STACK_SEG;

    uint32_t pos = CODE_SEG;
    while(!feof(src)) {
        fscanf(src, "%2X", &RAM[pos++]);
    }

    while (cpu.PC < pos) {
        const int instruction = fetch(&cpu, RAM, &val, &reg);
        const int icode = instruction >> 4;
        const int ifun = instruction & 0x0F;
        decode(&cpu, &val, &reg, icode);
        execute(&cpu, &val, icode, ifun, &cond);
        memory(RAM, &val, icode); // still missing exception for STAT_ADR
        writeback(&cpu, &val, &reg, cond, icode);
        PC(&cpu, &val, cond, icode);
        printf("instruction: %X PC: %llX\n", instruction, cpu.PC);
    }

    printCPUState(&cpu);

    free(RAM);
    fclose(src);

    return 0;
}