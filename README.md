# A tiny collection of screensavers

- CoreGraphics private API to show a window.
- OpenGL to render.
- No libc (-nostdlib).
- Not optimizated.
- Taps to keyboard events. Requests Input Monitoring perimssion.
- To exit press ESC.

### Supported Platforms
- macOS AArch64 (clang)

## Molecular Wind screensaver
`src/wind.c`

Render up to 1M instanced ~~sprites~~ particles that bounce around the edges,
and are blown by the wind. They change color depending on their speed.

- Particle update is simulated on CPU without any optimizations.
- Ofc compiler fails to autovectorize the update.

https://github.com/user-attachments/assets/cd8e88c2-d747-4fd0-91fe-a7a5d4a00b77

## Keys and Keyholes screensaver
`src/keyhole.c`

Keys and Keyholes flying around and colliding.
Matching keys and keyholes dissapear.

- Simple cirle elastic collision simulated on CPU.

https://github.com/user-attachments/assets/eb9552e4-32e3-4b9c-8be6-1a401453ae10

## Lovers minigame
`src/lovers.c`

Help lovers (with flower bouquets) find each other.
Arrows on the keyboard control blue lover.

- Sprite animation.
- Some simple game logic.

https://github.com/user-attachments/assets/745f3da5-ec2c-4ae3-abec-6852187c4f96

## Bitmap font
`src/font.c`

Bitmap font.
- 16x16 characters ASCII R8 bitmap tilemap.
- ASCII frame animation.

https://github.com/user-attachments/assets/d2f55f67-390f-4e15-8367-cb0785a34665

## ASCII Donut
`src/donut.c`

Rotating torus rendered into char buffer.
- Text buffer is rendered using bitmap font.

### Build all
```
./build.sh
```

### Run
Binaries are located in `build` directory.
```
./build/keyhole
./build/wind
./build/lovers
./build/font
./build/donut
```

### Files
- `src/*.c` - all screensavers, each screensaver is a separate `.c` file.
- `src/common.h` - common code for screensavers.
- `src/res_*.h` - resource files with binary data.
- `build.sh` - build script.
- `compile_flags.txt` - list of compilation flags used by clangd and `build.sh`.

#### Misc
- `.clangd` - clangd config.
- `.lvimrc` - vim local config.
