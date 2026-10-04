# Astro

<p align="center">
  <img src="assets/astro-launcher.svg" width="128" alt="Astro Launcher icon">
</p>

Astro is a small x86 operating system written in C (with a little assembly for boot and interrupts).

It does **not** replace Windows. You can try it in two safe ways:

- **Astro OS Simulator** (`astro-sim.exe`): a normal Windows program that runs Astro inside a console window.
  It reuses the same launcher, shell and text-mode code as the real kernel, but talks to Windows instead of hardware.
- **Virtual machine**: boot the real kernel in QEMU, VirtualBox or VMware.

## Download

Every push builds both targets on GitHub Actions. Open the **Actions** tab, pick the latest run and download:

- `astro-sim-windows` - the simulator (`astro-sim.exe`), just double-click it
- `astro-kernel` - the bare-metal kernel (`astro.elf`) for QEMU

## Features

- Multiboot boot (GRUB or `qemu -kernel`)
- GDT, IDT, PIC remapping and exception handling (kernel panic screen)
- Timer (PIT, 100 Hz) and PS/2 keyboard (US layout, arrows, Shift, Caps Lock)
- VGA text mode 80x25 driver and a minimal `kprintf`
- **Astro Launcher**: home screen with the Astro icon and an app menu
- Terminal with the commands: `help clear echo ver uptime mem reboot shutdown exit`

## Project layout

```text
src/kernel/    portable core: VGA text logic, kprintf, shell, boot log, platform interfaces
src/apps/      launcher and its icon
src/arch/x86/  bare-metal backend: GDT, IDT, PIC, PIT timer, PS/2 keyboard, VGA memory, power
src/sim/       Windows simulator backend: console display, keyboard, timer, power
src/boot/      Multiboot entry (boot.S) and CPU/interrupt stubs (cpu.S)
assets/        launcher icon (SVG) and the simulator's .ico
tools/         helper scripts (make-icon.ps1 regenerates assets/astro.ico)
linker.ld      kernel memory layout (loaded at 1 MiB)
```

The platform interfaces are `vga_hw_*` (display), `keyboard_*`, `timer_*` and `power_*`.
Each backend implements them, so the launcher and shell compile unchanged for both targets.

## Building

### Windows simulator

Requires MSYS2 (MinGW-w64 gcc and make):

```sh
# in an MSYS2 MINGW64 shell
pacman -S make mingw-w64-x86_64-gcc
make sim
./build/sim/astro-sim.exe
```

Controls: arrow keys and Enter in the launcher; `exit` leaves the terminal; Ctrl+C or "Shut down" closes the simulator.

### Bare-metal kernel

Requires `make`, a 32-bit capable C compiler and QEMU. On Windows use WSL (Ubuntu):

```sh
sudo apt install build-essential gcc-multilib qemu-system-x86
make CROSS= run
```

With an `i686-elf-gcc` cross-compiler, use plain `make run`.
`make iso` builds a bootable ISO (needs `grub-mkrescue` and `xorriso`).

## Launcher icon

- `assets/astro-launcher.svg`: vector version (512x512), a ringed planet with stars.
- `src/apps/launcher_icon.c`: the same art as a 16x16 grid, drawn in text mode with half blocks.
- `assets/astro.ico`: generated from that grid by `tools/make-icon.ps1` and embedded in `astro-sim.exe`.

## License

See [LICENSE](LICENSE).
