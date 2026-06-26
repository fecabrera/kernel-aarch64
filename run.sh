#!/bin/bash
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