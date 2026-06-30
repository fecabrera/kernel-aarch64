#!/bin/bash
MCC=${MCC:-"mcc"}
CC="aarch64-elf-gcc"

MCFLAGS="--target aarch64-unknown-none-elf \
		 --nostdlib --freestanding \
         --general-regs-only \
         --strict-align \
		 -O3"

CFLAGS="-ffreestanding -nostdlib -nostdinc \
        -mgeneral-regs-only \
        -mstrict-align \
        -O3 -Wall -Wextra \
        -mcpu=cortex-a710 \
        -Ilib/include \
        -Ilibc/include"

LD="aarch64-elf-ld"
AR="aarch64-elf-ar"

USER_FLAGS="-I libmc -I libcrt -I utils"
KERNEL_FLAGS="-I kernel -I libmc -I utils -D IS_KERNEL"

run_echo() {
    echo "$@"
    $@ || exit 1
}

kernel_compile() {
    # create object files
    run_echo $MCC $MCFLAGS $KERNEL_FLAGS $1 -o "${1%.mc}.o"
}

asm_compile() {
    run_echo $CC $CFLAGS -c $1 -o "${1%.S}.o"
}

c_compile() {
    run_echo $CC $CFLAGS -c $1 -o "${1%.c}.o"
}

user_lib_compile() {
    # create object files
    run_echo $MCC $MCFLAGS $USER_FLAGS $1 -o "user/${1%.mc}.o"
    
    # create interfaces
    run_echo $MCC $MCFLAGS $USER_FLAGS --emit-interface $1 -o "user/${1%.mc}.mci"
}

user_app_compile() {
    # create object files
    for f in $1/*.mc; do
        run_echo $MCC $MCFLAGS $USER_FLAGS $f
    done

    # link
	run_echo $LD -e entry -r ${1}/*.o user/utils/*.o user/libmc.a user/libc.a user/libcrt.a -o init/bin/$(basename -- "$1")
}

# user lib dirs
mkdir -p user/utils
mkdir -p user/libcrt
mkdir -p user/libc
mkdir -p user/libmc
mkdir -p user/libmc/hashing
mkdir -p user/libmc/iteration
mkdir -p user/libmc/libc
mkdir -p user/libmc/system

# user bin dir
mkdir -p init/bin

# utils
for file in utils/*.mc; do user_lib_compile $file; done

# lib
for file in lib/src/*.c; do c_compile $file; done

# libc
for file in libc/src/*.c; do c_compile $file; done

# kernel
for file in kernel/*.mc; do kernel_compile $file; done
for file in kernel/**/*.mc; do kernel_compile $file; done
for file in kernel/**/**/*.mc; do kernel_compile $file; done

# kernel-side utils
for file in utils/*.mc; do kernel_compile $file; done

# kernel-side libmc
for file in libmc/*.mc; do kernel_compile $file; done
for file in libmc/**/*.mc; do kernel_compile $file; done

# user-side libmc
for file in libmc/*.mc; do user_lib_compile $file; done
for file in libmc/**/*.mc; do user_lib_compile $file; done

# libcrt
for file in libcrt/*.mc; do user_lib_compile $file; done

# libmc.a
run_echo $AR rcs user/libmc.a user/libmc/*.o user/libmc/**/*.o

# libc.a
run_echo $AR rcs user/libc.a libc/src/*.o

# libcrt.a
run_echo $AR rcs user/libcrt.a user/libcrt/*.o

# user apps
for dir in user/apps/*; do user_app_compile $dir; done

# src
for file in src/*.S; do asm_compile $file; done
for file in src/*.mc; do kernel_compile $file; done

# link kernel
run_echo $LD -T linker.ld -o kernel.elf src/*.o utils/*.o lib/src/*.o libc/src/*.o kernel/*.o kernel/**/*.o kernel/**/**/*.o libmc/*.o libmc/**/*.o

# init.img
dd if=/dev/zero of=init.img bs=1M count=100
mkfs.fat -F 32 init.img
dev=$(hdiutil attach init.img | grep -o '/Volumes/.*')
dot_clean -n init
cp -R README.md init/* "$dev"
dot_clean -n "$dev"
hdiutil detach "$dev"
