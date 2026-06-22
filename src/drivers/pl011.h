#pragma once

/**
 * Initializes the PL011 UART: sets baud rate to 115200 8N1, enables FIFOs, unmasks RX interrupts,
 * and registers the IRQ with the GIC.
 */
void pl011_init();