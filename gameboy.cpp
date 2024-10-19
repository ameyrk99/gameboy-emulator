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
unsigned char workingRAM[0x2000];
unsigned char page0RAM[0x80];
int palette[4];
int tileSet, tileMap, scrollX, scrollY;
int Screen[SCREEN_W][SCREEN_H];
int H_BLANK = 0, V_BLANK = 1, SPRITE = 2, VRAM = 3;
int line = 0, cmpLine = 0, videoState = 0, keyboardColumn = 0, horizontal = 0;
int gpuMode = H_BLANK;
int romOffset = 0x4000;
long totalInstructions = 0;

/* -------------------------------------------------------------------------- */
// Function Definitions
void setControlByte(unsigned char b);
void setPalette(unsigned char b);
unsigned char getVideoState();

unsigned char memoryRead(int address);
void memoryWrite(int address, unsigned char value);

void readScreen();
void renderScreen();

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

unsigned char memoryRead(int address) {
  if (0 <= address && address <= 0x3fff)
    return rom[address];
  else if (0x4000 <= address && address <= 0x7fff)
    return rom[romOffset + address % 0x4000];
  else if (0x8000 <= address && address < 0x9fff)
    return graphicsRAM[address % 0x2000];
  else if (0xc000 <= address && address < 0xdfff)
    return workingRAM[address % 0x2000];
  else if (0xff80 <= address && address < 0xffff)
    return page0RAM[address % 0x80];
  else if (address == 0xff00)
    return 0xf;
  else if (address == 0xff41)
    return getVideoState();
  else if (address == 0xff42)
    return scrollY;
  else if (address == 0xff43)
    return scrollX;
  else if (address == 0xff44)
    return line;
  else if (address == 0xff45)
    return cmpLine;

  // For anything else, we return 0
  return 0;
}

void memoryWrite(int address, unsigned char value) {
  if (0 <= address && address <= 0x7fff)
    return;
  else if (0x8000 <= address && address < 0x9fff)
    graphicsRAM[address % 0x2000] = value;
  else if (0xc000 <= address && address < 0xdfff)
    workingRAM[address % 0x2000] = value;
  else if (0xff80 <= address && address < 0xffff)
    page0RAM[address % 0x80] = value;
  else if (address == 0xff00)
    keyboardColumn = value;
  else if (address == 0xff40)
    setControlByte(value);
  else if (address == 0xff41)
    videoState = value;
  else if (address == 0xff42)
    scrollY = value;
  else if (address == 0xff43)
    scrollX = value;
  else if (address == 0xff44)
    line = value;
  else if (address == 0xff45)
    cmpLine = value;
  else if (address == 0xff47)
    setPalette(value);
}

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

void setControlByte(unsigned char b) {
  tileMap = (b & 8) != 0 ? 1 : 0;
  tileSet = (b & 16) != 0 ? 1 : 0;
}

void setPalette(unsigned char b) {
  palette[0] = b & 3;
  palette[1] = (b >> 2) & 3;
  palette[2] = (b >> 4) & 3;
  palette[3] = (b >> 6) & 3;
}

unsigned char getVideoState() {
  int by = 0;
  if (line == cmpLine)
    by |= 4;
  if (gpuMode == V_BLANK)
    by |= 1;
  if (gpuMode == SPRITE)
    by |= 2;
  if (gpuMode == VRAM)
    by |= 3;
  return (unsigned char)((by | (videoState & 0xf8)) & 0xff);
}
