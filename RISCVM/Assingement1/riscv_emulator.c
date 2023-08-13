#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define VIRT_ROUT_SIZE 256
#define MEMORY_BANK_SIZE 64
#define MEMORY_BANK_COUNT 128
#define HEAP_BASE_ADDRESS 0xb700
#define INSTR_MEM_SIZE 1024
#define DATA_MEM_SIZE 1024
#define BUFFER_SIZE 4

#define CONSOLE_WRITE_CHAR 0x0800
#define CONSOLE_WRITE_SIGNED_INT 0x0804
#define CONSOLE_WRITE_UNSIGNED_INT 0x0808
#define HALT 0x080C
#define CONSOLE_READ_CHAR 0x0812
#define CONSOLE_READ_SIGNED_INT 0x0816

typedef struct {
    uint8_t data[MEMORY_BANK_SIZE];
    bool is_allocated;
} MemoryBank;

typedef struct {
    MemoryBank banks[MEMORY_BANK_COUNT];
} HeapManager;

typedef enum {
    ADD = 51,
    ADDI = 19,
    STORE = 35,
    LOAD = 3,
    BRANCH = 99,
    JALR = 103,
    JAL = 111,
    LUI = 55,
    HLT,
    
} Opcode;

typedef struct {
    Opcode opcode;
    uint32_t rd;
    uint32_t operand1;
    uint32_t operand2;
    int32_t imm;
    uint32_t func3;
    uint32_t func7;
} Instruction;

typedef struct {
    uint32_t registers[32];
    uint32_t pc; // program counter
    Instruction current_instr; 
    uint8_t data_mem[2048]; // 2048-byte memory
    HeapManager heap_manager;
} VM;
int32_t sign_extend_12(uint32_t imm_12);
bool load_binary_data(const char* filename, VM* vm);
uint32_t fetch(VM* vm);
int32_t sign_extend_12(uint32_t imm_12);
void decode(VM* vm, uint32_t raw_instr, Instruction* decoded_instr);
void basic_operations(VM* vm, Instruction* instr);
void store(VM* vm, Instruction* instr);
void load(VM* vm, Instruction* instr);
void basic_imm_operations(VM* vm, Instruction* instr);
void jalr(VM* vm, Instruction* instr);
void jal(VM* vm, Instruction* instr);
void lui(VM* vm, Instruction* instr);
void branch(VM* vm, Instruction* instr);
void execute(VM* vm, Instruction* instr);
void allocate_memory(VM* vm, uint32_t size);
uint32_t heap_manager_allocate(HeapManager* manager, uint32_t size);
void heap_manager_init(HeapManager* manager);
void free_memory(VM* vm, uint32_t address);
void dump_pc(VM* vm);
void vm_init(VM* vm) {
    // Initialize registers to 0
    memset(vm->registers, 0, sizeof(vm->registers));

    // Set the program counter to 0
    vm->pc = 0;

    // Clear data memory
    memset(vm->data_mem, 0, sizeof(vm->data_mem));
}
int main(int argc, char** argv) {
   

    VM vm = { 0 };
    vm_init(&vm);

    if (!load_binary_data("examples/hello_world/hello_world.mi", &vm)) {
        return 1;
    }

    while (true) {
        vm.registers[0] = 0; // x0 is always 0
        uint32_t raw_instr = fetch(&vm);
        Instruction decoded_instr;
        decode(&vm, raw_instr, &decoded_instr);
        execute(&vm, &decoded_instr);
        vm.pc += 4;
        
    }

    return 0;
}

bool load_binary_data(const char* filename, VM* vm) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", filename);
        return false;
    }

    size_t read_bytes = fread(vm->data_mem, 1, sizeof(vm->data_mem), file);
    fclose(file);

    if (read_bytes != sizeof(vm->data_mem)) {
        printf("Error: File size does not match memory size\n");
        return false;
    }

    return true;
}

uint32_t fetch(VM* vm) {
    if (vm->pc >= sizeof(vm->data_mem)) {
        printf("Error: PC out of bounds\n");
        exit(1);
    }

    uint32_t instruction = 0;
    for (int i = 0; i < 4; i++) {
        instruction |= (vm->data_mem[vm->pc + i] << (8 * i));
    }

    
    return instruction;
}

void decode(VM* vm, uint32_t raw_instr, Instruction* decoded_instr) {
    decoded_instr->opcode = raw_instr & 0x7F;
    decoded_instr->rd = (raw_instr >> 7) & 0x1F;
    decoded_instr->func3 = (raw_instr >> 12) & 0x7;
    decoded_instr->operand1 = (raw_instr >> 15) & 0x1F;
    decoded_instr->operand2 = (raw_instr >> 20) & 0x1F;
    decoded_instr->func7 = (raw_instr >> 25) & 0x7F;



    switch (decoded_instr->opcode) {
    case ADDI: // I-format
    case LOAD:
    case JALR:
        decoded_instr->imm = sign_extend_12(raw_instr >> 20);
        break;
    case STORE: // S-format
    {
        int32_t imm_S = ((int32_t)(raw_instr & 0xFE000000) >> 20) | ((raw_instr >> 7) & 0x1F); // Sign-extended 12-bit immediate
        decoded_instr->imm = imm_S;
        break;
    }
    case BRANCH: // B-format
    {
        int32_t imm_SB = ((raw_instr >> 7) & 0x1E) | ((raw_instr >> 20) & 0x7E0) | ((raw_instr << 4) & 0x800) | ((raw_instr >> 19) & 0x1000);
        decoded_instr->imm = sign_extend_12(imm_SB);
        break;
    }
    
    case LUI:// U-format
    {
        int32_t imm_U = raw_instr & 0xFFFFF000;
        decoded_instr->imm = sign_extend_12(imm_U);
        break;
    }
    case JAL: // J-format
    {
        int32_t imm_J = ((raw_instr & 0x80000000) ? 0xFFF00000 : 0x0) | ((raw_instr >> 20) & 0x7FE) | ((raw_instr >> 9) & 0x800) | ((raw_instr << 3) & 0x1000);
        decoded_instr->imm = sign_extend_12(imm_J);
       
    }
    break;
    case ADD:
        break;
    default:
        printf("Error: Unsupported opcode 0x%02X\n", decoded_instr->opcode);
        exit(1);
    }
}




int32_t sign_extend_12(uint32_t imm_12) {
    // Check if the immediate value is negative
    if ((imm_12 & 0x800) == 0x800) {
        // If so, extend the sign bit to the left
        return (int32_t)(imm_12 | 0xFFFFF000);
    }
    else {
        // If not, simply return the immediate value
        return (int32_t)imm_12;
    }
}
void load(VM* vm, Instruction* instr) {
    uint32_t rs1_val = vm->registers[instr->operand1];
    uint32_t address = rs1_val + instr->imm;
    if (address >= sizeof(vm->data_mem)+VIRT_ROUT_SIZE) {
        printf("Error: Load address out of bounds\n");
        exit(1);
    }

    if (address == 0x0812) { // Console Read Character
        vm->registers[instr->rd] = (uint32_t)getchar();
        return;
    }
    else if (address == 0x0816) { // Console Read Signed Integer
        scanf("%d", (int32_t*)&vm->registers[instr->rd]);
        return;
    }

    switch (instr->func3) {
    case 0x0: { // LB
        vm->registers[instr->rd] = sign_extend_12(vm->data_mem[address]);
        break;
    }
    case 0x1: { // LH
        uint16_t halfword = vm->data_mem[address] | (vm->data_mem[address + 1] << 8);
        vm->registers[instr->rd] = sign_extend_12(halfword);
        break;
    }
    case 0x2: { // LW
        uint32_t word = vm->data_mem[address] |
            (vm->data_mem[address + 1] << 8) |
            (vm->data_mem[address + 2] << 16) |
            (vm->data_mem[address + 3] << 24);
        vm->registers[instr->rd] = word;
        break;
    }

            case 0x4: { // LBU
            		vm->registers[instr->rd] = vm->data_mem[address];
            		break;
            	}
    default: {
        printf("Error: Unsupported func3 value for load 0x%02X\n", instr->func3);
        exit(1);
    }
    }
}
void store(VM* vm, Instruction* instr) {
    uint32_t rs1_val = vm->registers[instr->operand1];
    uint32_t rs2_val = vm->registers[instr->operand2];
    uint32_t address = rs1_val + instr->imm;
    if (address >= sizeof(vm->data_mem)+ VIRT_ROUT_SIZE) {
        printf("Error: Store address out of bounds\n");
        exit(1);
    }
    if (address == 0x0800) { // Console Write Character
        putchar((char)rs2_val);
        fflush(stdout);
        return;
    }
    else if (address == 0x0804) { // Console Write Signed Integer
        printf("%d", (int32_t)rs2_val);
        fflush(stdout);
        return;
    }
    else if (address == 0x0808) { // Console Write Unsigned Integer
        printf("%x", rs2_val);
        fflush(stdout);
        return;
    }
    else if (address == 0x080C) { // Halt
        printf("CPU Halt Requested\n");
        exit(0);
        return;
    }
    else if (address == 0x0820) { // Dump PC
        dump_pc(vm);
    }
    else if (address == 0x0830) { // malloc
        allocate_memory(vm, vm->registers[instr->operand2]);
    }
    else if (address == 0x0834) { // free
        free_memory(vm, vm->registers[instr->operand2]);
    }
    switch (instr->func3) {
    case 0x0: { // SB
        vm->data_mem[address] = rs2_val & 0xFF;
        break;
    }
    case 0x1: { // SH
        vm->data_mem[address] = rs2_val & 0xFF;
        vm->data_mem[address + 1] = (rs2_val >> 8) & 0xFF;
        break;
    }
    case 0x2: { // SW
        vm->data_mem[address] = rs2_val & 0xFF;
        vm->data_mem[address + 1] = (rs2_val >> 8) & 0xFF;
        vm->data_mem[address + 2] = (rs2_val >> 16) & 0xFF;
        vm->data_mem[address + 3] = (rs2_val >> 24) & 0xFF;
        break;
    }
    default: {
        printf("Error: Unsupported func3 value for store 0x%02X\n", instr->func3);
        exit(1);
    }
    }
}

void execute(VM* vm, Instruction* instr) {
    switch (instr->opcode) {
    case ADDI: {
        basic_imm_operations(vm, instr);
        break;
    }
    case LOAD: {
        load(vm, instr);
        break;
    }
    case STORE: {
        store(vm, instr);
        break;
    }
    case JAL: {
        jal(vm, instr);
        break;
    }
    case JALR: {
        jalr(vm, instr);
        break;
    }
    case LUI: {
        lui(vm, instr);
        break;
    }
    case BRANCH: {
        branch(vm, instr);
        break;
    }
    case ADD: {
        basic_operations(vm, instr);
        break;
    }
            
    default: {
        printf("Error: Unsupported opcode 0x%02X\n", instr->opcode);
        exit(1);
    }
 }

}
void basic_operations(VM* vm, Instruction* instr) {
    uint32_t rs1_val = vm->registers[instr->operand1];
    uint32_t rs2_val = vm->registers[instr->operand2];

    switch (instr->func3) {
    case 0x0: {
        if (instr->func7 == 0x00) {
            vm->registers[instr->rd] = rs1_val + rs2_val;
        }
        else if (instr->func7 == 0x20) {
            vm->registers[instr->rd] = rs1_val - rs2_val;
        }
        else {
            printf("Error: Unsupported func7 value for ADD/SUB 0x%02X\n", instr->func7);
            exit(1);
        }
        break;
    }
    case 0x1: {
        vm->registers[instr->rd] = rs1_val << (rs2_val & 0x1F);
        break;
    }
    case 0x4: {
        vm->registers[instr->rd] = rs1_val ^ rs2_val;
        break;
    }
    case 0x5: {
        if (instr->func7 == 0x00) {
            vm->registers[instr->rd] = rs1_val >> (rs2_val & 0x1F);
        }
        else if (instr->func7 == 0x20) {
            vm->registers[instr->rd] = (int32_t)rs1_val >> (rs2_val & 0x1F);
        }
        else {
            printf("Error: Unsupported func7 value for SRL/SRA 0x%02X\n", instr->func7);
            exit(1);
        }
        break;
    }
    case 0x6: {
        vm->registers[instr->rd] = rs1_val | rs2_val;
        break;
    }
    case 0x7: {
        vm->registers[instr->rd] = rs1_val & rs2_val;
        break;
    }
    default: {
        printf("Error: Unsupported func3 value 0x%02X\n", instr->func3);
        exit(1);
    }
    }
}

void basic_imm_operations(VM* vm, Instruction* instr) {
    uint32_t rs1_val = vm->registers[instr->operand1];
    int32_t imm = sign_extend_12(instr->imm);

    switch (instr->func3) {
    case 0x0:
        vm->registers[instr->rd] = rs1_val + imm;
        break;
    case 0x2:
        vm->registers[instr->rd] = ((int32_t)rs1_val < imm) ? 1 : 0;
        break;
    case 0x3:
        vm->registers[instr->rd] = (rs1_val < (uint32_t)imm) ? 1 : 0;
        break;
    case 0x4:
        vm->registers[instr->rd] = rs1_val ^ imm;
        break;
    case 0x6:
        vm->registers[instr->rd] = rs1_val | imm;
        break;
    case 0x7:
        vm->registers[instr->rd] = rs1_val & imm;
        break;
    }
}
void jal(VM* vm, Instruction* instr) {
  
    int32_t offset = sign_extend_12(instr->imm);
    vm->registers[instr->rd] = vm->pc;
    vm->pc += offset-4;
}
void jalr(VM* vm, Instruction* instr) {
    int32_t offset = sign_extend_12(instr->imm);
    uint32_t rs1_val = vm->registers[instr->operand1];

    vm->registers[instr->rd] = vm->pc;
    vm->pc = (rs1_val + offset) & ~1U;
}
void lui(VM* vm, Instruction* instr) {
    vm->registers[instr->rd] = instr->imm;
}
void branch(VM* vm, Instruction* instr) {
    bool should_branch = false;
    int32_t src1 = vm->registers[instr->operand1];
    int32_t src2 = vm->registers[instr->operand2];
    uint32_t usrc1 = src1;
    uint32_t usrc2 = src2;

    switch (instr->func3) {
    case 0: // BEQ
        should_branch = src1 == src2;
       
        break;
    case 1: // BNE
        should_branch = src1 != src2;
        break;
    case 4: // BLT
        should_branch = src1 < src2;
        break;
    case 6: // BLTU
        should_branch = usrc1 < usrc2;
        break;
    case 5: // BGE
        should_branch = src1 >= src2;
        break;
    case 7: // BGEU
        should_branch = usrc1 >= usrc2;
        break;
    }

    if (should_branch) {
        uint32_t dest = vm->pc + instr->imm - 4;
        vm->pc = dest;

    }

}
void allocate_memory(VM* vm, uint32_t size) {
    uint32_t allocated_memory = heap_manager_allocate(&vm->heap_manager, size);
    vm->registers[28] = allocated_memory;
}
void free_memory(VM* vm, uint32_t address) {
    if (address < HEAP_BASE_ADDRESS || address >= HEAP_BASE_ADDRESS + MEMORY_BANK_COUNT * MEMORY_BANK_SIZE) {
        printf("Invalid address for free operation.\n");
        exit(1);
    }

    int start_bank = (address - HEAP_BASE_ADDRESS) / MEMORY_BANK_SIZE;

    if (!vm->heap_manager.banks[start_bank].is_allocated) {
        printf("Illegal operation: Trying to free unallocated memory.\n");
        exit(1);
    }

    while (start_bank < MEMORY_BANK_COUNT && vm->heap_manager.banks[start_bank].is_allocated) {
        vm->heap_manager.banks[start_bank].is_allocated = false;
        start_bank++;
    }
}
void heap_manager_init(HeapManager* manager) {
    for (int i = 0; i < MEMORY_BANK_COUNT; i++) {
        manager->banks[i].is_allocated = false;
    }
}

uint32_t heap_manager_allocate(HeapManager* manager, uint32_t size) {
    int required_banks = (size + MEMORY_BANK_SIZE - 1) / MEMORY_BANK_SIZE;
    int consecutive_banks = 0;
    int start_bank = -1;

    for (int i = 0; i < MEMORY_BANK_COUNT; i++) {
        if (!manager->banks[i].is_allocated) {
            consecutive_banks++;
            if (consecutive_banks >= required_banks) {
                start_bank = i - required_banks + 1;
                break;
            }
        }
        else {
            consecutive_banks = 0;
        }
    }

    if (start_bank == -1) {
        return 0;
    }

    for (int i = start_bank; i < start_bank + required_banks; i++) {
        manager->banks[i].is_allocated = true;
    }

    return HEAP_BASE_ADDRESS + start_bank * MEMORY_BANK_SIZE;
}
void dump_pc(VM* vm) {
    printf("%x\n", vm->pc);
}