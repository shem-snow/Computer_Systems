/*
 * Author: Daniel Kopta
 * Updated by: Erin Parker
 * CS 4400, University of Utah
 *
 * Simulator handout
 * A simple x86-like processor simulator.
 * Read in a binary file that encodes instructions to execute.
 * Simulate a processor by executing instructions one at a time and appropriately
 * updating register and memory contents.
 *
 * Some code and pseudo code has been provided as a starting point.
 *
 * Completed by: Shem Snow
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "instruction.h"

// Forward declarations for helper functions
unsigned int get_file_size(int file_descriptor);
unsigned int *load_file(int file_descriptor, unsigned int size);
instruction_t *decode_instructions(unsigned int *bytes, unsigned int num_instructions);
unsigned int execute_instruction(unsigned int program_counter, instruction_t *instructions,
                                 int *registers, unsigned char *memory);
void print_instructions(instruction_t *instructions, unsigned int num_instructions);
void error_exit(const char *message);

// Additional helper methods that I added
instruction_t rawToT(unsigned int src);
void SetFlags(int *flagRegister, int augend, int addend);
void SetFlag(int *flagRegister, unsigned int flag, unsigned int bitPosition);
unsigned int CF(int *registers);
unsigned int ZF(int *registers);
unsigned int SF(int *registers);
unsigned int OF(int *registers);
unsigned int max(unsigned int first, unsigned int second);

// 17 registers
#define NUM_REGS 17
// 1024-byte stack
#define STACK_SIZE 1024

int main(int argc, char **argv)
{
  // Make sure we have enough arguments
  if (argc < 2)
    error_exit("must provide an argument specifying a binary file to execute");

  // Open the binary file
  int file_descriptor = open(argv[1], O_RDONLY);
  if (file_descriptor == -1)
    error_exit("unable to open input file");

  // Get the size of the file
  unsigned int file_size = get_file_size(file_descriptor);
  // Make sure the file size is a multiple of 4 bytes
  // since machine code instructions are 4 bytes each
  if (file_size % 4 != 0)
    error_exit("invalid input file");

  // Load the file into memory
  // We use an unsigned int array to represent the raw bytes
  // We could use any 4-byte integer type
  unsigned int *instruction_bytes = load_file(file_descriptor, file_size);
  close(file_descriptor);

  unsigned int num_instructions = file_size / 4;

  /****************************************/
  /**** Begin code to modify/implement ****/
  /****************************************/

  // Allocate and decode instructions (left for you to fill in)
  instruction_t *instructions = decode_instructions(instruction_bytes, num_instructions);

  // Optionally print the decoded instructions for debugging
  // Do not call this function in your submitted final version
  // print_instructions(instructions, num_instructions);

  // Allocate and initialize registers
  int *registers = (int *)malloc(sizeof(int) * NUM_REGS);
  // Initialize the stack pointer (register 6) to 1024 and all other registers to zero.
  for (int i = 0; i < NUM_REGS; i++)
  {
    registers[i] = (i == 6) ? 1024 : 0;
  }

  // Allocate the stack memory (it is byte-addressed, so it must be a 1-byte type).
  unsigned char *memory = (unsigned char *)malloc(STACK_SIZE);
  // Initiallize the stack to all zeros
  for (int i = 0; i < STACK_SIZE; i++)
  {
    memory[i] = 0;
  }

  // Run the simulation. The program_counter must start at zero.
  unsigned int program_counter = 0;

  // program_counter is a byte address, so we must multiply num_instructions by 4
  // to get the address past the last instruction
  while (program_counter != num_instructions * 4)
  {
    program_counter = execute_instruction(program_counter, instructions, registers, memory);
  }

  return 0;
}

/*
 * Decodes the array of raw instruction bytes into an array of instruction_t then returns a pointer to it.
 *
 * @param bytes - A pointer to the base of the raw instructions array
 * @param num_instructions - The number of instructions in the array (Each raw instruction is encoded as a 4-byte unsigned int)
 * @returns retval  - A pointer to the base of generated intruction_t[]
 */
instruction_t *decode_instructions(unsigned int *bytes, unsigned int num_instructions)
{
  // Allocate space for an array big enough to hold the decoded instructions
  instruction_t *retval = (instruction_t *)malloc(num_instructions * sizeof(instruction_t));

  // Use a loop to decode and store every one of the raw instructions from "bytes" into the array of instruction_t.
  for (int i = 0; i < num_instructions; i++)
    retval[i] = rawToT(bytes[i]);

  return retval;
}

/**
 * "Raw-to-T-Type-Instruction"
 * This helper method (for decode_instructions) separates the concern of converting a 32-bit raw instruction into
 * the 40-bit instruction for our processor.
 *
 * My strategy was to create a 'mask' variable that would undergo bit shifts and size adjustments so that it could move
 * alongside the raw instruction to extract and transfer each field into the instruction_t encoding.
 *
 * Readability note: When setting dest.field = (mask & src), only the right-most bits are considered.
 * That's why I have the right-shifts on each assignment except the first to the immediate value.
 *
 * @param src: A pointer to the start of a 32-bit raw instruction.
 * @return dest: The corresponding 40-bit instruction for our processor.
 */
instruction_t rawToT(unsigned int src)
{

  instruction_t dest;

  // Extract the immediate value (16 right-most bits)
  unsigned int mask = 65535; // 0....0 1111 1111 1111 1111
  dest.immediate = src & mask;

  // Change the size of the mask to fit into 'raw' register and opcode fields then transfer each one.
  mask = (mask >> 11) << 17;
  // reg2
  dest.second_register = (mask & src) >> 17;
  mask = mask << 5;
  // reg1
  dest.first_register = (mask & src) >> 22;
  mask = mask << 5;
  // opcode
  dest.opcode = (mask & src) >> 27;

  return dest;
}

/*
 * Executes a single instruction and returns the next program counter
 */
unsigned int execute_instruction(unsigned int program_counter, instruction_t *instructions, int *registers, unsigned char *memory)
{
  // program_counter is a byte address, but instructions are 4 bytes each
  // divide by 4 to get the index into the instructions array
  instruction_t instr = instructions[program_counter / 4];

  switch (instr.opcode)
  {
  case subl: // 0
    registers[instr.first_register] = registers[instr.first_register] - instr.immediate;
    break;
  case addl_reg_reg: // 1
    registers[instr.second_register] = registers[instr.first_register] + registers[instr.second_register];
    break;
  case addl_imm_reg: // 2
    registers[instr.first_register] += instr.immediate;
    break;
  case imull: // 3
    registers[instr.second_register] *= registers[instr.first_register];
    break;
  case shrl: // 4
    registers[instr.first_register] = ((unsigned int)registers[instr.first_register]) >> 1;
    break;
  case movl_reg_reg: // 5
    // reg2 = reg1
    registers[instr.second_register] = registers[instr.first_register];
    break;
  case movl_deref_reg: // 6
    // %reg2 = imm(%reg1)
    registers[instr.second_register] = *((int *)(&memory[(registers[instr.first_register] + instr.immediate)]));
    break;
  case movl_reg_deref: // 7
    // imm(%reg2) = reg1
    *((int *)&memory[(registers[instr.second_register] + instr.immediate)]) = *(int*)&registers[instr.first_register];
    break;
  case movl_imm_reg: // 8
    // reg1 = (MSB == 1)? (reg1 | 11..11 0000 0000 0000 0000 -> -65536) : (reg1 & 00..00 1111 1111 1111 1111 -> 65535); // This is sign-extension
    registers[instr.first_register] = instr.immediate;
    break;
  case cmpl: // 9
    SetFlags(&registers[16], -1 * registers[instr.first_register], registers[instr.second_register]);
    break;
  case je: // 10
    if (ZF(registers))
      program_counter += instr.immediate;
    break;
  case jl: // 11
    if (SF(registers) ^ OF(registers))
      program_counter += instr.immediate;
    break;
  case jle: // 12
    if ((SF(registers) ^ OF(registers)) | ZF(registers))
      program_counter += instr.immediate;
    break;
  case jge: // 13
    if (!(SF(registers) ^ OF(registers)))
      program_counter += instr.immediate;
    break;
  case jbe: // 14
    if (CF(registers) | ZF(registers))
      program_counter += instr.immediate;
    break;
  case jmp: // 15
    program_counter += instr.immediate;
    break;
  case call: // 16
    // Push the address of the next instruction onto the stack.
    registers[6] -= 4;
    *(int*)&memory[registers[6]] = program_counter + 4;

    // Then change the program_counter to hold the called procedure.
    program_counter += instr.immediate;
    break;
  case ret: // 17
    // Terminate the program if the stack is empty.
    if (registers[6] == 1024)
      exit(0);
    else
    {
      // Pop the address of the next instruction from the stack into the program_counter.
      program_counter = *((int *)&memory[registers[6]]);
      // program_counter = memory[registers[6]] - 4; // -4 because every instruction automatically adds 4.
      registers[6] += 4;
      return program_counter;
    }
    break;
  case pushl: // 18
    // Push the current instruction's first_register to the stack
    registers[6] -= 4;
    *(int *)&memory[registers[6]] = *(int *)&registers[instr.first_register];
    break;
  case popl: // 19
    // Pop the top item of the stack into the current instruction's first_register.
    registers[instr.first_register] = *(int*)&memory[registers[6]];
    registers[6] += 4;
    break;
  case printr: // 20
    printf("%d (0x%x)\n", registers[instr.first_register], registers[instr.first_register]);
    break;
  case readr:
    scanf("%d", &(registers[instr.first_register]));
    break;
  // If an unrecognized instruction gets this far without a crash just ignore it and go to the next one.
  default:
    break;
  }

  // program_counter + 4 represents the subsequent instruction
  return program_counter + 4;
}

/**
 * Helper method for setting condition codes. There are exactly four flags and they are all set based on the result of
 * result = a + b
 *
 * Calling agreement: It is up to the caller to decide if a and/or b are negative so that they get the correct
 * signed and unsigned flag results.
 *
 * Strategy is to calculate a boolean for each flag then OR the flag register with that boolean bit-shifted
 * by the "bitPosition" (constant value) of the specific flag.
 *
 * @param flagRegister: A pointer to the 32-bit flag register to set.
 * @param augend: Proper math language for "first thing being added".
 * @param addend: Proper math language for "second thing being added".
 */
void SetFlags(int *flagRegister, int augend, int addend)
{

  // Do the calculation then set each flag
  int result = augend + addend;

  // CF is true if the unsigned value of addition is less than either of the operators.
  unsigned int CF = (unsigned int)result > max((unsigned int)augend, (unsigned int)addend); // TODO: still doesn't make sense
  SetFlag(flagRegister, CF, 0);

  // ZF is true when result = zero.
  unsigned int ZF = (result == 0);
  SetFlag(flagRegister, ZF, 6);

  // SF is the same as the sign bit of the result.
  unsigned int SF = ((unsigned int)result >> 31);
  SetFlag(flagRegister, SF, 7);

  // OF is true when operands have the same sign that is opposite to the result.
  unsigned int OF = ((augend < 0) == (addend < 0)) && ((result < 0) != (augend < 0));
  SetFlag(flagRegister, OF, 11);
}
/**
 * Helper method for "SetFlags" that returns the "flagRegister" as it was except with "flag"'s
 * value at the "bitPosition".
 *
 * User notice: The function "SetFlags" sets all the flags but the function "SetFlag" only sets one.
 * They are different.
 *
 * @param flag - a Boolean 1 or 0 to represent the flag
 * @param bitPosition - the index of the flag register for the flag.
 */
void SetFlag(int *flagRegister, unsigned int flag, unsigned int bitPosition)
{
  *flagRegister = (*flagRegister & ~(1 << bitPosition)) | (flag << bitPosition);
}
/**
 * The following four helper methods receive the array of instructions and fetch the flags register (register 16).
 * They use bit-shifts to find and return the flags at the block associated to each method name.
 *
 * User notice: These are getters not setters.
 *
 * CF: 0
 * ZF: 6
 * SF: 7
 * OF: 11
 */
unsigned int CF(int *registers)
{
  return (registers[16] << 31) >> 31;
}
unsigned int ZF(int *registers)
{
  return (registers[16] << 25) >> 31;
}
unsigned int SF(int *registers)
{
  return (registers[16] << 24) >> 31;
}
unsigned int OF(int *registers)
{
  return (registers[16] << 20) >> 31;
}

/**
 * A helper to determine the max of new numbers
 */
unsigned int max(unsigned int first, unsigned int second)
{
  return (first > second) ? first : second;
}

/*********************************************/
/****  DO NOT MODIFY THE FUNCTIONS BELOW  ****/
/*********************************************/

/*
 * Returns the file size in bytes of the file referred to by the given descriptor
 */
unsigned int get_file_size(int file_descriptor)
{
  struct stat file_stat;
  fstat(file_descriptor, &file_stat);
  return file_stat.st_size;
}

/*
 * Loads the raw bytes of a file into an array of 4-byte units
 */
unsigned int *load_file(int file_descriptor, unsigned int size)
{
  // Allocate space for the array
  unsigned int *raw_instruction_bytes = (unsigned int *)malloc(size);

  // Check for inability to read
  if (raw_instruction_bytes == NULL)
    error_exit("unable to allocate memory for instruction bytes (something went really wrong)");
  // read the file and store it into the "raw_instructions_bytes" array. Also record the number of bytes read.
  int num_read = read(file_descriptor, raw_instruction_bytes, size);
  // check for an incorrect number of bytes read
  if (num_read != size)
    error_exit("unable to read file (something went really wrong)");

  return raw_instruction_bytes;
}

/*
 * Prints the opcode, register IDs, and immediate of every instruction,
 * assuming they have been decoded into the instructions array
 */
void print_instructions(instruction_t *instructions, unsigned int num_instructions)
{
  printf("instructions: \n");
  unsigned int i;
  for (i = 0; i < num_instructions; i++)
  {
    printf("op: %d, reg1: %d, reg2: %d, imm: %d\n",
           instructions[i].opcode,
           instructions[i].first_register,
           instructions[i].second_register,
           instructions[i].immediate);
  }
  printf("--------------\n");
}

/*
 * Prints an error and then exits the program with status 1
 */
void error_exit(const char *message)
{
  printf("Error: %s\n", message);
  exit(1);
}