#include <stdio.h>
#include <unistd.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <thread>

#include <gdkmm/general.h>
#include <gtkmm/application.h>
#include <gtkmm/button.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>

#include <glibmm/main.h>
#include "Z80.h"

using namespace std;

// Used to update screen for GTK and sleep for emulator
const int SCREEN_REFRESH_IN_MS = 16, EMULATOR_UPDATE_IN_MS = 32;

const int SCREEN_W = 160, SCREEN_H = 144;
const int TILE_MAP_0_ADDRESS = 0x1800, TILE_MAP_1_ADDRESS = 0x1c00;
// A char representation of GameBoy in increasing degree of opacity
const char* PIXEL_REPRESENTATION[] = {" ", ".", "+", "*"};

Z80* z80;
char* rom;
int romSize;

unsigned char graphicsRAM[8192];
unsigned char workingRAM[0x2000];
unsigned char page0RAM[0x80];
unsigned char spriteRAM[0x100];
int palette[4], objPalette0[4], objPalette1[4];
int tileSet, tileMap, scrollX, scrollY;
int Screen[SCREEN_W][SCREEN_H] = {{0}};  // 0 to 3 pixel values
int H_BLANK = 0, V_BLANK = 1, SPRITE = 2, VRAM = 3;
int line = 0, cmpLine = 0, videoState = 0, horizontal = 0;
int gpuMode = H_BLANK;
int romOffset = 0x4000;
long totalInstructions = 0;

int romBank, cartridgeType, romSizeMask;
int romSizeMaskList[] = {0x7fff,  0xffff,   0x1ffff,  0x3ffff, 0x7ffff,
                         0xfffff, 0x1fffff, 0x3fffff, 0x7fffff};

int keys0 = 0xf, keys1 = 0xf;
int keyboardColumn = 0;

/* -------------------------------------------------------------------------- */
// Function Definitions
void setControlByte(unsigned char b);
void dma(int address);
void setPalette(int* pal, unsigned char b);
unsigned char getVideoState();
int getSpritePixel(int currentPixel, int x, int y);

void updateEmulator();
unsigned char memoryRead(int address);
void memoryWrite(int address, unsigned char value);

void readScreen(int pixelY);
void renderAsciiScreen();

class GameBoyDrawingArea : public Gtk::DrawingArea {
 public:
  // Init default screen
  int GbScreen[SCREEN_W][SCREEN_H] = {{0}};  // 0 to 3 pixel values

  /* Queue a redraw for the screen*/
  void update_screen() {
    for (int y = 0; y < SCREEN_H; ++y) {
      for (int x = 0; x < SCREEN_W; ++x) {
        GbScreen[x][y] = Screen[x][y];
      }
    }

    queue_draw();
  }

  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    // Pixel size based off the window size
    const double pixelWidth = static_cast<double>(width) / SCREEN_W;
    const double pixelHeight = static_cast<double>(height) / SCREEN_H;

    // Grayscale
    // const std::vector<std::tuple<double, double, double>> colors = {
    //     {1.0, 1.0, 1.0},        // Off
    //     {0.75, 0.75, 0.75},     // 33%
    //     {0.375, 0.375, 0.375},  // 66%
    //     {0.0, 0.0, 0.0}         // 100%
    // };
    // Green tint (mimic GameBoy look)
    const std::vector<std::tuple<double, double, double>> colors = {
        {0.88, 1.0, 0.88},   // Off
        {0.66, 0.85, 0.66},  // 33%
        {0.33, 0.55, 0.33},  // 66%
        {0.0, 0.25, 0.0}     // 100%
    };

    for (int y = 0; y < SCREEN_H; ++y) {
      for (int x = 0; x < SCREEN_W; ++x) {
        int pixelValue = GbScreen[x][y];
        pixelValue = getSpritePixel(pixelValue, x, y);

        auto [r, g, b] = colors[pixelValue];
        // Set the fill color
        cr->set_source_rgb(r, g, b);
        // Draw the rectangle for pixel
        cr->rectangle(x * pixelWidth, y * pixelHeight, pixelWidth, pixelHeight);
        cr->fill();
      }
    }

    return true;
  }
};

// Keycode defs for my two computers
#ifdef __APPLE__  // For macOS
enum KeyCodes {
  KB_UP = 0x0d,      // w
  KB_LEFT = 0x00,    // a
  KB_RIGHT = 0x02,   // d
  KB_DOWN = 0x01,    // s
  KB_SELECT = 0x04,  // h
  KB_START = 0x25,   // l
  KB_A = 0x2d,       // n
  KB_B = 0x2e        // m
};
#elif __linux__  // For Linux
enum KeyCodes {
  KB_UP = 0x19,      // w
  KB_LEFT = 0x26,    // a
  KB_RIGHT = 0x28,   // d
  KB_DOWN = 0x27,    // s
  KB_SELECT = 0x2b,  // h
  KB_START = 0x2e,   // l
  KB_A = 0x39,       // n
  KB_B = 0x3a        // m
};
#else
#error "Unsupported OS"
#endif

void handleKeyDown(int keyCode) {
  switch (keyCode) {
    case KB_RIGHT:
      keys1 &= 0xe;
      break;
    case KB_LEFT:
      keys1 &= 0xd;
      break;
    case KB_UP:
      keys1 &= 0xb;
      break;
    case KB_DOWN:
      keys1 &= 0x7;
      break;
    case KB_A:
      keys0 &= 0xe;
      break;
    case KB_B:
      keys0 &= 0xd;
      break;
    case KB_SELECT:
      keys0 &= 0xb;
      break;
    case KB_START:
      keys0 &= 0x7;
      break;
    default:
      // printf("Invalid keycode: 0x%x", keyCode);
      break;
  }
}

void handleKeyUp(int keyCode) {
  switch (keyCode) {
    case KB_RIGHT:
      keys1 |= 1;
      break;
    case KB_LEFT:
      keys1 |= 2;
      break;
    case KB_UP:
      keys1 |= 4;
      break;
    case KB_DOWN:
      keys1 |= 8;
      break;
    case KB_A:
      keys0 |= 1;
      break;
    case KB_B:
      keys0 |= 2;
      break;
    case KB_SELECT:
      keys0 |= 4;
      break;
    case KB_START:
      keys0 |= 8;
      break;
    default:
      // printf("Invalid keycode: 0x%x", keyCode);
      break;
  }
}

class GameBoyWindow : public Gtk::Window {
 public:
  GameBoyDrawingArea screen;

  GameBoyWindow() {
    set_title("GameBoy Emulator");
    add(screen);

    // Set the event mask to capture key press and release events
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);

    // Connect signal handlers
    signal_key_press_event().connect(
        sigc::mem_fun(*this, &GameBoyWindow::onKeyPress));
    signal_key_release_event().connect(
        sigc::mem_fun(*this, &GameBoyWindow::onKeyRelease));

    screen.show();
  }

 protected:
  bool onKeyPress(GdkEventKey* keyEvent) {
    // printf("[Press] Key: %s\tCode: 0x%2x\n",
    // gdk_keyval_name(keyEvent->keyval),
    //        keyEvent->hardware_keycode);

    handleKeyDown(keyEvent->hardware_keycode);
    z80->throwInterrupt(0x10);
    return false;
  }

  bool onKeyRelease(GdkEventKey* key_event) {
    // printf("[Release] Key: %s\tCode: 0x%2x\n",
    //        gdk_keyval_name(key_event->keyval), key_event->hardware_keycode);

    handleKeyUp(key_event->hardware_keycode);
    z80->throwInterrupt(0x10);
    return false;
  }
};

gboolean timeoutUpdateScreen(gpointer sc) {
  GameBoyDrawingArea* obj = (GameBoyDrawingArea*)sc;

  obj->update_screen();

  // Continue loop
  return true;
}

atomic<bool> running(true);
void emulatorThread() {
  while (running) {
    updateEmulator();
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Incorrect execution. Use ./gameboy rom.gb\n");
    exit(128);
  }

  // Need argc, argv for GTK but remove rom name
  int newArgc = argc - 1;  // One argument will be removed
  char** newArgv = new char*[newArgc];
  newArgv[0] = argv[0];

  // --------------------------------------------------------------------------
  // Graphical Setup
  Glib::RefPtr<Gtk::Application> app =
      Gtk::Application::create(newArgc, newArgv, "org.gtkmm.gameboy");

  GameBoyWindow win;

  // ifstream romFile("opus5.gb", ios::in | ios::binary | ios::ate);
  ifstream romFile(argv[1], ios::in | ios::binary | ios::ate);
  streampos size = romFile.tellg();

  rom = new char[size];
  romSize = size;
  romFile.seekg(0, ios::beg);
  romFile.read(rom, size);
  romFile.close();

  romBank = 0;
  cartridgeType = rom[0x147] & 3;
  romSizeMask = romSizeMaskList[rom[0x148]];

  z80 = new Z80(memoryRead, memoryWrite);
  z80->reset();

  g_timeout_add(SCREEN_REFRESH_IN_MS, timeoutUpdateScreen,
                &win.screen);  // 16ms = ~60FPS

  thread emulator(emulatorThread);

  int result = app->run(win);

  // Stop the emulator thread on exit
  running = false;
  if (emulator.joinable())
    emulator.join();

  return result;
}

int getSpritePixel(int currentPixel, int x, int y) {
  int newPixelValue = currentPixel;
  for (int spriteIter = 0; spriteIter < 40; spriteIter++) {
    int spriteY = spriteRAM[spriteIter * 4 + 0] - 16;
    int spriteX = spriteRAM[spriteIter * 4 + 1] - 8;
    if (spriteX + 8 <= x || spriteX > x || spriteY + 8 <= y || spriteY > y)
      continue;

    int options = spriteRAM[spriteIter * 4 + 3];
    if ((options & 0x80) != 0)
      continue;

    int sprTileNumber = spriteRAM[spriteIter * 4 + 2];
    int sprTileAddr = sprTileNumber * 16;

    int sprOffsetX = x - spriteX, sprOffsetY = y - spriteY;
    if ((options & 0x40) != 0)
      sprOffsetY = 7 - sprOffsetY;
    if ((options & 0x20) != 0)
      sprOffsetX = 7 - sprOffsetX;

    char sprByte0 = graphicsRAM[sprTileAddr + sprOffsetY * 2];
    char sprByte1 = graphicsRAM[sprTileAddr + sprOffsetY * 2 + 1];

    char sprCapturePixel0 = sprByte0 >> (7 - sprOffsetX) & 1;
    char sprCapturePixel1 = sprByte1 >> (7 - sprOffsetX) & 1;
    int sprPixel = sprCapturePixel1 * 2 + sprCapturePixel0;

    if (sprPixel != 0)
      newPixelValue = ((options & 0x10) == 0) ? objPalette0[sprPixel]
                                              : objPalette1[sprPixel];
  }

  return newPixelValue;
}

void updateEmulator() {
  if (!z80->halted) {
    z80->doInstruction();
  }

  // Check for and handle interrupts
  if (z80->interrupt_deferred > 0) {
    z80->interrupt_deferred--;
    if (z80->interrupt_deferred == 1) {
      z80->interrupt_deferred = 0;
      z80->FLAG_I = 1;
    }
  }
  z80->checkForInterrupts();

  // Check screen position and set video mode
  // GameBoy runs ~61 instructions per row of the screen
  horizontal = (int)((totalInstructions + 1) % 61);

  if (line >= 145)
    gpuMode = V_BLANK;
  else if (horizontal <= 30)
    gpuMode = H_BLANK;
  else if (31 <= horizontal && horizontal <= 40)
    gpuMode = SPRITE;
  else
    gpuMode = VRAM;

  if (horizontal == 0) {
    line++;

    if (line == 144)
      z80->throwInterrupt(1);
    if (line % 153 == cmpLine && (videoState & 0x40) != 0)
      z80->throwInterrupt(2);
    if (line == 153)
      line = 0;
    if (line >= 0 && line < 144) {
      readScreen(line);
      usleep(EMULATOR_UPDATE_IN_MS);
    }
  }

  totalInstructions++;
}

unsigned char memoryRead(int address) {
  if (0 <= address && address <= 0x3fff)
    return rom[address];
  else if (0x4000 <= address && address <= 0x7fff)
    return rom[romOffset + address % 0x4000];
  else if (0x8000 <= address && address <= 0x9fff)
    return graphicsRAM[address % 0x2000];
  else if (0xc000 <= address && address <= 0xdfff)
    return workingRAM[address % 0x2000];
  else if (0xfe00 <= address && address <= 0xfe9f)
    return spriteRAM[address % 0x100];
  else if (0xff80 <= address && address <= 0xffff)
    return page0RAM[address % 0x80];
  else if (address == 0xff00) {
    if ((keyboardColumn & 0x30) == 0x10) {
      return keys0;
    } else {
      return keys1;
    }
  } else if (address == 0xff41)
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
  // Step 4
  if (cartridgeType == 1 || cartridgeType == 2 || cartridgeType == 3) {
    if (0x2000 <= address && address <= 0x3fff) {
      value = value & 0x1f;
      if (value == 0)
        value = 1;

      romBank = romBank & 0x60;
      romBank += value;

      romOffset = (romBank * 0x4000) & romSizeMask;
    } else if (0x4000 <= address && address <= 0x5fff) {
      value = value & 3;
      romBank = romBank & 0x1f;
      romBank |= value << 5;

      romOffset = (romBank * 0x4000) & romSizeMask;
    }
  }

  if (0 <= address && address <= 0x7fff)
    return;
  else if (0x8000 <= address && address <= 0x9fff)
    graphicsRAM[address % 0x2000] = value;
  else if (0xc000 <= address && address <= 0xdfff)
    workingRAM[address % 0x2000] = value;
  else if (0xfe00 <= address && address <= 0xfe9f)
    spriteRAM[address % 0x100] = value;
  else if (0xff80 <= address && address <= 0xffff)
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
  else if (address == 0xff46)
    dma(value);
  else if (address == 0xff47)
    setPalette(palette, value);
  else if (address == 0xff48)
    setPalette(objPalette0, value);
  else if (address == 0xff49)
    setPalette(objPalette1, value);
}

/*
Sets colors on 4 grade palette and reads into Screen
*/
void readScreen(int pixelY) {
  for (int pixelX = 0; pixelX < SCREEN_W; pixelX++) {
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

void renderAsciiScreen() {
  // Clear terminal
  printf("\033c");

  for (int y = 0; y < SCREEN_H; y++) {
    for (int x = 0; x < SCREEN_W; x++) {
      printf("%s", PIXEL_REPRESENTATION[Screen[x][y]]);
    }
    printf("\n");
  }
}

void dma(int address) {
  address = address << 8;
  for (int i = 0; i < 0xa0; i++) {
    memoryWrite(0xfe00 + i, memoryRead(address + i));
  }
}

void setControlByte(unsigned char b) {
  tileMap = (b & 8) != 0 ? 1 : 0;
  tileSet = (b & 16) != 0 ? 1 : 0;
}

void setPalette(int* pal, unsigned char b) {
  pal[0] = b & 3;
  pal[1] = (b >> 2) & 3;
  pal[2] = (b >> 4) & 3;
  pal[3] = (b >> 6) & 3;
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
