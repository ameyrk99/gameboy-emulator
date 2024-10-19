#include <stdio.h>
#include <fstream>
#include <iostream>

#include "Z80.h"

using namespace std;

// From Step 1; No longer used
// int rom[] = {0x06, 0x06, 0x3e, 0x00, 0x80, 0x05, 0xc2, 0x04, 0x00, 0x76};

Z80* z80;
char* rom;
int romSize;

unsigned char memoryRead(int address) {
  return rom[address];
}

void memoryWrite(int address, unsigned char value) {}

int main(int argc, char* argv[]) {
  ifstream romFile("testrom.gb", ios::in | ios::binary | ios::ate);
  streampos size = romFile.tellg();

  rom = new char[size];
  romSize = size;
  romFile.seekg(0, ios::beg);
  romFile.read(rom, size);
  romFile.close();

  z80 = new Z80(memoryRead, memoryWrite);
  z80->reset();

  // Step 1: No longer used
  //   z80->PC = 0;

  while (!z80->halted) {
    z80->doInstruction();
    printf("PC=%x\tA=%d\tB=%d\n", z80->PC, z80->A, z80->B);
  }

  return 0;
}
