# A tiny collection of screensavers

- CoreGraphics private API to show a window.
- OpenGL to render.
- No libc (-nostdlib).
- Had no time for optimizations.
- To exit press Ctrl-C.

## Molecular Wind screensaver
`src/wind.c`

Render up to 1M instanced ~~sprites~~ particles that bounce around the edges,
and are blown by the wind. They change color depending on their speed.

- Sprite update is simulated on CPU without any optimization.
- Ofc compiler fails to autovectorize sprite update.

https://github.com/user-attachments/assets/7478f311-aa52-4cdb-b3cc-6ad01327fff5

## Keyhole Peeker screensaver
`src/peeker.c`

Screen turns black and you can only see your desktop through the flying holes.

### Supported Platforms
- macOS AArch64 (clang)

### Build all
```
./build.sh
```

### Run
Binaries are located in `build` directory.
```
./build/wind
```

### Files
- `src/*.c` - all screensavers, each screensaver is a separate `.c` file.
- `build.sh` - build script.

#### Misc
- `.clangs` - clangd config to suppress warnings
- `.lvimrc` - vim local config. Ignore if you don't use vim.
- `compile_flags.txt` - list of compilation flags used by clangd and `build.sh`.
