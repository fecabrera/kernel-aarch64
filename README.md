# AArch64 Bare-Metal Kernel

A minimal bare-metal kernel for AArch64, targeting the QEMU `virt` machine.

## Requirements

- `aarch64-elf-gcc` and `aarch64-elf-ld` (cross-compiler toolchain)
- `qemu-system-aarch64`

On macOS:
```sh
brew install aarch64-elf-gcc qemu
```

## Build

```sh
make
```

Output: `kernel.elf` and `kernel.img`.

## Run

```sh
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a710 \
  -nographic \
  -kernel kernel.img \
  -serial mon:stdio \
  -m 128M
```

Press `Ctrl+A` then `X` to exit QEMU.

## Memory map (QEMU virt)

| Address | Device |
|---------|--------|
| `0x40000000` | Kernel load address |
| `0x09000000` | PL011 UART0 |
| `0x08000000` | GIC distributor |
| `0x08010000` | GIC CPU interface |
