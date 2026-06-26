#!/bin/bash
MCCPATH=${MCCPATH:-~/Documents/lang}
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
USER_FLAGS="-I libmc -I libcrt"
KERNEL_FLAGS="-I kernel -I libmc -D IS_KERNEL"

kernel_compile() {
    # create object files
    PYTHONPATH=$MCCPATH $MCC $MCFLAGS $KERNEL_FLAGS $1 -o "${1%.mc}.o" || exit 1
}

asm_compile() {
    $CC $CFLAGS -c $1 -o "${1%.S}.o" || exit 1
}

c_compile() {
    $CC $CFLAGS -c $1 -o "${1%.c}.o" || exit 1
}

user_lib_compile() {
    # create object files
    PYTHONPATH=$MCCPATH $MCC $MCFLAGS $USER_FLAGS $1 -o "user/${1%.mc}.o" || exit 1
    
    # create interfaces
    PYTHONPATH=$MCCPATH $MCC $MCFLAGS $USER_FLAGS --emit-interface $1 -o "user/${1%.mc}.mci" || exit 1
}

user_app_compile() {
    # create object files
    PYTHONPATH=$MCCPATH $MCC $MCFLAGS -I user/libmc $1/*.mc || exit 1;

    # link
	$LD -e entry -r ${1}/*.o user/libmc.a user/libc.a user/libcrt.a -o init/bin/$(basename -- "$1") || exit 1
}

# user lib dirs
mkdir -p user/libcrt
mkdir -p user/libc
mkdir -p user/libmc
mkdir -p user/libmc/hashing
mkdir -p user/libmc/iteration
mkdir -p user/libmc/libc
mkdir -p user/libmc/system

# user bin dir
mkdir -p init/bin

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

# libcrt
for file in libcrt/*.mc; do user_lib_compile $file; done

# libmc.a
$AR rcs user/libmc.a user/libmc/*.o user/libmc/**/*.o || exit 1

# libc.a
$AR rcs user/libc.a libc/src/*.o || exit 1

# libcrt.a
$AR rcs user/libcrt.a user/libcrt/*.o || exit 1

# user apps
for dir in user/apps/*; do user_app_compile $dir; done

# src
for file in src/*.S; do asm_compile $file; done
for file in src/*.mc; do kernel_compile $file; done

# link kernel
$LD -T linker.ld -o kernel.elf src/*.o lib/src/*.o libc/src/*.o kernel/*.o kernel/**/*.o kernel/**/**/*.o libmc/*.o libmc/**/*.o || exit 1

# init.img
dd if=/dev/zero of=init.img bs=1M count=100
mkfs.fat -F 32 init.img
dev=$(hdiutil attach init.img | grep -o '/Volumes/.*')
dot_clean -n init
cp -R README.md init/* "$dev"
dot_clean -n "$dev"
hdiutil detach "$dev"