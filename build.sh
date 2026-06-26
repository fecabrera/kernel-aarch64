#!/bin/bash
MCC="python -m mcc"
CC="aarch64-elf-gcc"
MCFLAGS="--target aarch64-unknown-none-elf \
		 --nostdlib --freestanding \
         --general-regs-only \
		 -O3"
CFLAGS="-ffreestanding -nostdlib -nostdinc \
        -mgeneral-regs-only \
        -O3 -Wall -Wextra \
        -mcpu=cortex-a710 \
        -Ilib/include \
        -Ilibc/include"
LD="aarch64-elf-ld"
AR="aarch64-elf-ar"
USER_FLAGS="-I libmc"
KERNEL_FLAGS="-I kernel -I libmc -D IS_KERNEL"

kernel_compile() {
    # create object files
    PYTHONPATH=~/Documents/lang $MCC $MCFLAGS $KERNEL_FLAGS $1 -o "${1%.mc}.o"
}

asm_compile() {
    $CC $CFLAGS -c $1 -o "${1%.S}.o"
}

c_compile() {
    $CC $CFLAGS -c $1 -o "${1%.c}.o"
}

user_lib_compile() {
    # create object files
    PYTHONPATH=~/Documents/lang $MCC $MCFLAGS $USER_FLAGS $1 -o "user/${1%.mc}.o"
    
    # create interfaces
    PYTHONPATH=~/Documents/lang $MCC $MCFLAGS $USER_FLAGS --emit-interface $1 -o "user/${1%.mc}.mci"
}

user_app_compile() {
    # create object files
    PYTHONPATH=~/Documents/lang $MCC $MCFLAGS -I user/libmc $1/*.mc

    # link
	$LD -r ${1}/*.o user/libmc.a user/libc.a -o init/bin/$(basename -- "$1")
}

# lib
for file in lib/src/*.c; do c_compile $file; done

# libc
for file in libc/src/*.c; do c_compile $file; done

# kernel
for file in kernel/*.mc; do kernel_compile $file; done
for file in kernel/**/*.mc; do kernel_compile $file; done
for file in kernel/**/**/*.mc; do kernel_compile $file; done

# kernel-side libmc
for file in libmc/*.mc; do kernel_compile $file; done
for file in libmc/**/*.mc; do kernel_compile $file; done

# user-side libmc
for file in libmc/*.mc; do user_lib_compile $file; done
for file in libmc/**/*.mc; do user_lib_compile $file; done

# libmc.a
$AR rcs user/libmc.a user/libmc/*.o user/libmc/**/*.o

# libc.a
$AR rcs user/libc.a libc/src/*.o

# user apps
for dir in user/apps/*; do user_app_compile $dir; done

# src
for file in src/*.S; do asm_compile $file; done
for file in src/*.mc; do kernel_compile $file; done

# link kernel
$LD -T linker.ld -o kernel.elf src/*.o lib/src/*.o libc/src/*.o kernel/*.o kernel/**/*.o kernel/**/**/*.o libmc/*.o libmc/**/*.o

# init.img
dd if=/dev/zero of=init.img bs=1M count=100
mkfs.fat -F 32 init.img
dev=$(hdiutil attach init.img | grep -o '/Volumes/.*')
dot_clean -n init
cp -R README.md init/* "$dev"
dot_clean -n "$dev"
hdiutil detach "$dev"