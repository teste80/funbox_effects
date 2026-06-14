# funbox_effects

`funbox_effects` is a Funbox-targeted multi-effect firmware repo. It keeps the
shared Funbox hardware glue in one place and lets you add individual effects
under `effects/` such as distortion, delay, reverb, and more.

## Credit

This project is adapted from the original Funbox project by Keith Bloemer /
GuitarML.

Original repo:
- https://github.com/GuitarML/Funbox

If you are building on this platform, the original Funbox repo is well worth
exploring. It includes finished pedals, experiments, and the hardware context
behind the control mapping used here.


## Prerequisites

Before building, install the Daisy toolchain and make sure `make` works in your
shell environment.

You will also need these dependencies in the repo root:

- `libDaisy`
- `DaisySP`

They are expected to live at:

```text
funbox_effects/
  libDaisy/
  DaisySP/
```

## Setup

If you are starting from a fresh clone:

```sh
git submodule update --init --recursive
```

If you are creating the repo structure yourself, add the required submodules:

```sh
git submodule add https://github.com/electro-smith/libDaisy.git libDaisy
git submodule add https://github.com/electro-smith/DaisySP.git DaisySP
git submodule update --init --recursive
```

## Funbox Hardware Mapping

This repo uses modified `daisy_petal` files so the Funbox hardware controls map
correctly.

Before building `libDaisy`, replace the files in `libDaisy/src` with the ones
in `mod/`:

```sh
cp mod/daisy_petal.h libDaisy/src/daisy_petal.h
cp mod/daisy_petal.cpp libDaisy/src/daisy_petal.cpp
```

This step must happen before:

```sh
make -C libDaisy
```

If you build `libDaisy` first and patch it later, the Funbox control mapping
will be wrong until you rebuild `libDaisy`.

## Build The Daisy Libraries

After installing the Daisy toolchain and replacing the `daisy_petal` files,
build the shared libraries:

```sh
make -C libDaisy
make -C DaisySP
```

## Build An Effect

From the repo root, build an effect by name:

```sh
make EFFECT=distortion
```

If you omit `EFFECT`, the root `Makefile` defaults to:

```sh
make EFFECT=distortion
```

You can also build directly inside an effect folder:

```sh
cd effects/distortion
make
```

## Program The Daisy Seed

From the repo root:

```sh
make EFFECT=distortion program-dfu
```

Or:

```sh
make EFFECT=distortion program
```

Typical usage:

- `program-dfu`: upload over USB in bootloader mode
- `program`: upload with a SWD/JTAG programmer such as ST-Link

If you need SRAM / bootloader-based deployment for a larger effect later, add
the appropriate `APP_TYPE` setting inside that effect's `Makefile`.

## Program With The Web Programmer

You can also flash firmware with Electrosmith's Web Programmer:

- https://flash.daisy.audio/

Official Daisy tutorial:
- https://daisy.audio/tutorials/web-programmer/

Basic Daisy Seed / Funbox flashing flow:

1. Connect the Daisy Seed to your computer with a micro USB cable.
2. Open the Web Programmer in Google Chrome or another browser that supports
   the required WebUSB flow.
3. Put the Daisy into flashable DFU mode:
   hold `BOOT`, press `RESET`, release `RESET`, then release `BOOT`.
4. In the Web Programmer, click `Connect`.
5. In the device picker, choose `DFU in FS Mode`.
6. Select your built firmware file if you are flashing your own binary.
7. Start the flash process and wait for it to finish.

If the board entered DFU mode correctly, the Daisy Seed user LED should stop
flashing while it is ready to be programmed, as described in the official Daisy
tutorial.

For larger projects that need SRAM / bootloader workflows, configure that per
effect and follow the corresponding Daisy bootloader flow rather than the basic
flash-memory path.


## Adding More Effects

Add a new folder under `effects/`, for example:

```text
effects/
  distortion/
  delay/
  reverb/
```

Each effect should typically contain:

- its own `Makefile`
- one or more source files
- a local `README.md`

The simplest path is to copy `effects/distortion/` and then rename:

- `TARGET`
- source filename(s)
- control descriptions
- DSP code

After that, build from the repo root with:

```sh
make EFFECT=delay
```

## Adding More Dependencies

Some future effects may need extra libraries beyond `libDaisy` and `DaisySP`,
for example:

- `eigen`
- `gcem`
- `RTNeural`
- `q`
- `infra`

When that happens, add those dependencies at the repo root as shared
submodules, for example:

```text
funbox_effects/
  libDaisy/
  DaisySP/
  eigen/
  gcem/
  RTNeural/
  q/
  infra/
  effects/
```

Then let each effect opt into only the dependencies it needs in its own
`Makefile`.

For example:

```make
C_INCLUDES += -I../../include
C_INCLUDES += -I../../eigen
C_INCLUDES += -I../../RTNeural
```

This keeps the repo flexible:

- simple effects stay simple
- heavier effects can add advanced DSP or ML dependencies
- shared libraries are cloned once and reused across effects

## Notes

- The shared Funbox mapping lives in `include/funbox.h` and `mod/daisy_petal.*`.
- If you want expression support later, you can add an `expressionHandler`
  helper like the one used in the original Funbox repo.

