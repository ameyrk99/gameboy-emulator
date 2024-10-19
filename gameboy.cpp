#include <stdio.h>
#include <fstream>
#include <iostream>

#include "Z80.h"

using namespace std;

const int SCREEN_W = 160, SCREEN_H = 144;
const int TILE_MAP_0_ADDRESS = 0x1800, TILE_MAP_1_ADDRESS = 0x1c00;
// A char representation of GameBoy in increasing degree of opacity
const char* PIXEL_REPRESENTATION[] = {" ", ".", "+", "*"};

// From Step 1; No longer used
// int rom[] = {0x06, 0x06, 0x3e, 0x00, 0x80, 0x05, 0xc2, 0x04, 0x00, 0x76};

Z80* z80;
char* rom;
int romSize;

unsigned char graphicsRAM[8192];
int palette[4];
int tileSet, tileMap, scrollX, scrollY;
int Screen[SCREEN_W][SCREEN_H];

unsigned char memoryRead(int address) {
  return rom[address];
}

void memoryWrite(int address, unsigned char value) {}

/*
Sets colors on 4 grade palette and reads into Screen
*/
void readScreen() {
  for (int pixelX = 0; pixelX < SCREEN_W; pixelX++) {
    for (int pixelY = 0; pixelY < SCREEN_H; pixelY++) {
      int x = pixelX, y = pixelY;

      // Apply scroll and wrap using bitmask
      x = (x + scrollX) & 255;
      y = (y + scrollY) & 255;

      int tileX = x / 8, tileY = y / 8;
      int tilePosition = tileY * 32 + tileX;
      int tileIndex = (tileMap == 0)
                          ? graphicsRAM[TILE_MAP_0_ADDRESS + tilePosition]
                          : graphicsRAM[TILE_MAP_1_ADDRESS + tilePosition];

      // If tileSet is 0, tile indices are signed number with neg below
      // 0x1000
      int tileAddress;
      if (tileSet == 1) {
        tileAddress = tileIndex * 16;
      } else {
        if (tileIndex >= 128)
          tileIndex -= 256;
        tileAddress = tileIndex * 16 + 0x1000;
      }

      // Each 8x8 tile is encoded as 8 rows each consisting of 2 bytes
      int offsetX = x % 8, offsetY = y % 8;
      char byte0 = graphicsRAM[tileAddress + offsetY * 2];
      char byte1 = graphicsRAM[tileAddress + offsetY * 2 + 1];

      // Bit shift to right and AND to zero all bits except ones we
      // want
      char capturePixel0 = byte0 >> (7 - offsetX) & 1;
      char capturePixel1 = byte1 >> (7 - offsetX) & 1;
      int pixel = capturePixel1 * 2 + capturePixel0;

      Screen[pixelX][pixelY] = palette[pixel];
    }
  }
}

void renderScreen() {
  readScreen();

  // Clear terminal
  printf("\033c");

  for (int y = 0; y < SCREEN_H; y++) {
    for (int x = 0; x < SCREEN_W; x++) {
      printf("%s", PIXEL_REPRESENTATION[Screen[x][y]]);
    }
    printf("\n");
  }
}

int main(int argc, char* argv[]) {
  // setup(argc, argv)

  // --------------------------------------------------------------------------
  // Step 1
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

  // --------------------------------------------------------------------------
  // Step 2
  ifstream vidfile("screendump.txt", ios::in);

  // Read first 8192 integers into graphics RAM
  for (int i = 0; i < 8192; i++) {
    int n;
    vidfile >> n;
    graphicsRAM[i] = (unsigned char)n;
  }

  // Read rest of the variables
  vidfile >> tileSet;
  vidfile >> tileMap;
  vidfile >> scrollX;
  vidfile >> scrollY;
  vidfile >> palette[0];
  vidfile >> palette[1];
  vidfile >> palette[2];
  vidfile >> palette[3];

  renderScreen();

  return 0;
}
