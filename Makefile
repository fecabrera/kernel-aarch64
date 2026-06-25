MCCPATH = ~/Documents/lang
MCC		= PYTHONPATH=$(MCCPATH) python -m mcc
CC      = aarch64-elf-gcc
LD      = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy
AR      = aarch64-elf-ar

MCFLAGS = --target aarch64-unknown-none-elf \
		  --nostdlib --freestanding \
          --general-regs-only \
		  -O3 \
		  -I kernel \
		  -I libmc \
          $(CFLAGS_EXTRA)

CFLAGS  = -ffreestanding -nostdlib -nostdinc \
          -mgeneral-regs-only \
          -O3 -Wall -Wextra \
          -mcpu=cortex-a710 \
          -Ilib/include \
          -Ilibc/include \
          $(CFLAGS_EXTRA)

SRCS := $(wildcard \
 		  src/*.S \
		  src/*.mc \
  		  lib/src/*.c \
  		  libc/src/*.c \
  		  libsys/*.mc \
  		  libmc/*.mc \
  		  libmc/iteration/*.mc \
  		  libmc/hashing/*.mc \
  		  libmc/libc/*.mc \
		  kernel/*.mc \
		  kernel/devices/*.mc \
		  kernel/filesystem/*.mc \
		  kernel/filesystem/drivers/*.mc \
		  kernel/interrupts/*.mc \
		  kernel/interrupts/drivers/*.mc \
		  kernel/system/*.mc)
OBJS := $(SRCS:.c=.o)
OBJS := $(OBJS:.mc=.o)
OBJS := $(OBJS:.S=.o)
OBJS := $(patsubst lib/src/%, build/lib/%, $(OBJS))
OBJS := $(patsubst libc/src/%, build/libc/%, $(OBJS))
OBJS := $(patsubst libmc/%, build/libmc/%, $(OBJS))
OBJS := $(patsubst kernel/%, build/kernel/%, $(OBJS))
OBJS := $(patsubst src/%, build/src/%, $(OBJS))

build: kernel.elf kernel.img

all: build user init.img

user: helloworld echo stat sleep msleep

build/src/%.o: src/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/src/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel/%.o: kernel/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/lib/%.o: lib/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/libc/%.o: libc/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/libmc/%.o: libmc/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

libmc.a: build
	$(AR) rcs $@ build/libmc/*.o build/libmc/iteration/*.o build/libmc/hashing/*.o

libc.a: build
	$(AR) rcs $@ build/libc/*.o

helloworld: libmc.a libc.a
	$(MCC) $(MCFLAGS) user/helloworld/main.mc
	$(LD) -r user/helloworld/main.o $^ -o init/bin/helloworld

echo: libmc.a libc.a
	$(MCC) $(MCFLAGS) user/echo/main.mc
	$(LD) -r user/echo/main.o $^ -o init/bin/echo

stat: libmc.a libc.a
	$(MCC) $(MCFLAGS) user/stat/main.mc
	$(LD) -r user/stat/main.o $^ -o init/bin/stat

sleep: libmc.a libc.a
	$(MCC) $(MCFLAGS) user/sleep/main.mc
	$(LD) -r user/sleep/main.o $^ -o init/bin/sleep

msleep: libmc.a libc.a
	$(MCC) $(MCFLAGS) user/msleep/main.mc
	$(LD) -r user/msleep/main.o $^ -o init/bin/msleep

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
