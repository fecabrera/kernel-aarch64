MCCPATH = ~/Documents/lang
MCC		= PYTHONPATH=$(MCCPATH) python -m mcc
CC      = aarch64-elf-gcc
LD      = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy

MCFLAGS = --target aarch64-unknown-none-elf \
		  --nostdlib --freestanding \
          --general-regs-only \
		  -O3 \
		  -I src/kernel \
		  -I src/libmc \
          $(CFLAGS_EXTRA)

CFLAGS  = -ffreestanding -nostdlib -nostdinc \
          -mgeneral-regs-only \
          -O3 -Wall -Wextra \
          -mcpu=cortex-a710 \
          -Isrc \
          -Isrc/lib \
          $(CFLAGS_EXTRA)

SRCS := $(wildcard \
		  src/*.c \
 		  src/*.S \
  		  src/lib/*.c \
		  src/mm/*.c \
		  src/*.mc \
  		  src/libmc/*.mc \
  		  src/libmc/iteration/*.mc \
  		  src/libmc/hashing/*.mc \
  		  src/libmc/libc/*.mc \
		  src/kernel/*.mc \
		  src/kernel/devices/*.mc \
		  src/kernel/filesystem/*.mc \
		  src/kernel/interrupts/*.mc \
		  src/kernel/interrupts/drivers/*.mc \
		  src/kernel/mm/*.mc \
		  src/kernel/system/*.mc)
OBJS := $(patsubst src/%, build/%, $(SRCS:.c=.o))
OBJS := $(OBJS:.S=.o)
OBJS := $(OBJS:.mc=.o)

build: kernel.elf kernel.img

all: build init.img

build/%.o: src/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/lib/%.o: src/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/mm/%.o: src/mm/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJS) linker.ld
	$(LD) -T linker.ld -o $@ $(OBJS)

kernel.img: kernel.elf
	$(OBJCOPY) -O binary $< $@

init.img:
	dd if=/dev/zero of=$@ bs=1M count=100 && \
	mkfs.fat -F 32 $@ && \
	dev=$$(hdiutil attach $@ | grep -o '/Volumes/.*') && \
	dot_clean -n init && \
  	cp -R README.md init/* "$$dev" && \
	dot_clean -n "$$dev" && \
	hdiutil detach "$$dev"

run: all
	qemu-system-aarch64 \
		-machine virt \
		-cpu cortex-a710 \
		-kernel kernel.elf \
		-serial stdio \
		-device virtio-gpu-pci \
		-drive file=init.img,format=raw,if=none,id=hd0 \
		-device virtio-blk-device,drive=hd0 \
		-global virtio-mmio.force-legacy=false \
		-m 128M

clean:
	rm -rf build kernel.elf kernel.img init.img
