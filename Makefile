CC      = aarch64-elf-gcc
LD      = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy

CFLAGS  = -ffreestanding -nostdlib -nostdinc \
          -mgeneral-regs-only \
          -O2 -Wall -Wextra \
          -mcpu=cortex-a710 \
          -Isrc \
          -Isrc/lib

SRCS := $(wildcard src/*.c src/*.S src/lib/*.c)
OBJS := $(patsubst src/%, build/%, $(SRCS:.c=.o))
OBJS := $(OBJS:.S=.o)

all: kernel.elf kernel.img

build/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/lib/%.o: src/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJS) linker.ld
	$(LD) -T linker.ld -o $@ $(OBJS)

kernel.img: kernel.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf build kernel.elf kernel.img
