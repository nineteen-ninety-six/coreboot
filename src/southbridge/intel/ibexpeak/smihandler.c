/* SPDX-License-Identifier: GPL-2.0-only */

#include <types.h>
#include <arch/io.h>
#include <device/pci_ops.h>
#include <console/console.h>
#include <device/pci_def.h>
#include <cpu/intel/em64t101_save_state.h>
#include <cpu/intel/model_2065x/model_2065x.h>
#include <gpio.h>
#include <southbridge/intel/common/finalize.h>
#include <southbridge/intel/common/pmbase.h>
#include <southbridge/intel/ibexpeak/me.h>
#include "pch.h"

/* We are using PCIe accesses for now
 *  1. the chipset can do it
 *  2. we don't need to worry about how we leave 0xcf8/0xcfc behind
 */
#include <northbridge/intel/ironlake/ironlake.h>
#include <southbridge/intel/common/pmutil.h>

/*
 * Drive reset gate GPIO low to gate memory reset in S3.
 *
 * Intel reference designs all use GPIO 60 but it is
 * not a requirement and boards could use a different pin.
 */
void southbridge_gate_memory_reset(void)
{

	gpio_output(CONFIG_DRAM_RESET_GATE_GPIO, 0);
}

void southbridge_smi_monitor(void)
{
#define IOTRAP(x) (trap_sts & (1 << x))
	u32 trap_sts, trap_cycle;
	u32 data, mask = 0;
	int i;

	trap_sts = RCBA32(0x1e00); // TRSR - Trap Status Register
	RCBA32(0x1e00) = trap_sts; // Clear trap(s) in TRSR

	trap_cycle = RCBA32(0x1e10);
	for (i = 16; i < 20; i++) {
		if (trap_cycle & (1 << i))
			mask |= (0xff << ((i - 16) << 3));
	}

	/* IOTRAP(3) SMI function call */
	if (IOTRAP(3)) {
		return;
	}

	/* IOTRAP(2) currently unused
	 * IOTRAP(1) currently unused */

	/* IOTRAP(0) SMIC */
	if (IOTRAP(0)) {
		if (!(trap_cycle & (1 << 24))) { // It's a write
			printk(BIOS_DEBUG, "SMI1 command\n");
			data = RCBA32(0x1e18);
			data &= mask;
			// if (smi1)
			// 	southbridge_smi_command(data);
			// return;
		}
		// Fall through to debug
	}

	printk(BIOS_DEBUG, "  trapped io address = 0x%x\n", trap_cycle & 0xfffc);
	for (i = 0; i < 4; i++) {
		if (IOTRAP(i))
			printk(BIOS_DEBUG, "  TRAP = %d\n", i);
	}
	printk(BIOS_DEBUG, "  AHBE = %x\n", (trap_cycle >> 16) & 0xf);
	printk(BIOS_DEBUG, "  MASK = 0x%08x\n", mask);
	printk(BIOS_DEBUG, "  read/write: %s\n", (trap_cycle & (1 << 24)) ? "read" : "write");

	if (!(trap_cycle & (1 << 24))) {
		/* Write Cycle */
		data = RCBA32(0x1e18);
		printk(BIOS_DEBUG, "  iotrap written data = 0x%08x\n", data);
	}
#undef IOTRAP
}

void southbridge_finalize_all(void)
{
	/* TODO: Finalize ME */
	intel_pch_finalize_smm();
	intel_ironlake_finalize_smm();
	intel_model_2065x_finalize_smm();
}
