# GameBoy Emulator

### Files included:
- `CMakeLists.txt`
- `gameboy.cpp`
- `z80.cpp`
- `z80.h`
- `README.md`

### Project Statement

I used `gtkmm3` (`DrawingArea` in particular) and provided z80 files to emulate Nintendo GameBoy as a GUI.
Chose `gtkmm3` my operating system PopOS 22.04 came with GTK3. It worked well with MacOS as well.

I followed the instructions while refactoring or tweaking the code as necessary as the instructions were either for ASCII output or QT. For example, instead of screen updates synced to `line`, only the pixel mapping is updated in `Screen` matrix. The GUI instead updates independently on a separate thread every 16ms reading the `Screen` matrix.

I also got all the parts working and was able to play several ROMs such as:
- Opus5: correctly moves the spaceship around (or updates the background to give that illusion)
- Mario Super Land: plays mario but I couldn't make the first checkpoint; Not good at platformers

### How to build

Configure project
```sh
cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++ --no-warn-unused-cli -S${SOURCE_DIR} -B${BUILD_DIR} -G "Unix Makefiles"
```

Build
```sh
cmake --build ${BUILD_DIR} --config Debug --target all -j 10
```

Run
```sh
cd build
./gameboy ${PATH_TO_ROM}
```

Example:
```sh
# Configure
cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++ --no-warn-unused-cli -S/Users/ameyrk/Code/gameboy-emulator -B/Users/ameyrk/Code/gameboy-emulator/build -G "Unix Makefiles"

# Build
cmake --build /Users/ameyrk/Code/gameboy-emulator/build --config Debug --target all -j 10

# Run
cd build
./gameboy ../opus5.gb
```