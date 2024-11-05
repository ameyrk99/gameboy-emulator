# GameBoy Emulator

This project was made in order to understand how Z80 and Nintendo's GameBoy worked;
And, used opus5 demo ROM for development. See more: <https://www.opusgames.com/games/GBDev/GBDev.html>

Project uses GTKMM3 for GUI.


### Build

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
