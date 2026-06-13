MCC		= PYTHONPATH=~/Documents/lang python -m mcc
CC      = aarch64-elf-gcc
LD      = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy

MCFLAGS = --target aarch64-unknown-none-elf \
          --general-regs-only \
		  --naked \
		  -O3 \
		  -I src/kernel \
		  -I src/libmc

CFLAGS  = -ffreestanding -nostdlib -nostdinc \
          -mgeneral-regs-only \
          -O3 -Wall -Wextra \
          -mcpu=cortex-a710 \
          -Isrc \
          -Isrc/lib \
          $(CFLAGS_EXTRA)

SRCS := $(wildcard \
		  src/*.c \
		  src/*.mc \
 		  src/*.S \
  		  src/lib/*.c \
  		  src/libmc/*.mc \
  		  src/libmc/hashing/*.mc \
		  src/dsa/*.c \
		  src/dsa/deque/*.c \
		  src/dsa/hashmap/*.c \
		  src/dsa/ordered_set/*.c \
		  src/dsa/queue/*.c \
		  src/dsa/set/*.c \
		  src/dsa/stack/*.c \
		  src/dsa/vector/*.c \
		  src/drivers/*.c \
		  src/arch/*.c \
		  src/mm/*.c \
		  src/sched/*.c \
		  src/fs/*.c \
		  src/io/*.c \
		  src/devices/*.c \
		  src/kernel/*.mc \
		  src/kernel/arch/*.mc \
		  src/kernel/devices/*.mc \
		  src/kernel/drivers/*.mc \
		  src/kernel/filesystem/*.mc \
		  src/kernel/io/*.mc \
		  src/kernel/mm/*.mc)
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

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/lib/%.o: src/libmc/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/lib/hashing/%.o: src/libmc/hashing/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/kernel/%.o: src/kernel/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/kernel/arch/%.o: src/kernel/arch/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/kernel/devices/%.o: src/kernel/devices/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/kernel/drivers/%.o: src/kernel/drivers/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/kernel/filesystem/%.o: src/kernel/filesystem/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/kernel/io/%.o: src/kernel/io/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/kernel/mm/%.o: src/kernel/mm/%.mc
	@mkdir -p $(dir $@)
	$(MCC) $(MCFLAGS) $< -o $@

build/lib/%.o: src/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/dsa/%.o: src/dsa/%.c
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
