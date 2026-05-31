CC      = aarch64-elf-gcc
LD      = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy

CFLAGS  = -ffreestanding -nostdlib -nostdinc \
          -mgeneral-regs-only \
          -O3 -Wall -Wextra \
          -mcpu=cortex-a710 \
          -Isrc \
          -Isrc/lib \
          $(CFLAGS_EXTRA)

SRCS := $(wildcard src/*.c src/*.S src/lib/*.c src/dsa/*.c src/drivers/*.c src/arch/*.c src/mm/*.c src/sched/*.c src/fs/*.c)
OBJS := $(patsubst src/%, build/%, $(SRCS:.c=.o))
OBJS := $(OBJS:.S=.o)

build: kernel.elf kernel.img

all: build init.img

build/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

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
	dd if=/dev/zero of=$@ bs=1M count=100; \
	mkfs.fat -F 32 $@; \
	dev=$$(hdiutil attach $@ | grep -o '/Volumes/.*'); \
	cp -R init/* "$$dev"; \
	dot_clean "$$dev"; \
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
