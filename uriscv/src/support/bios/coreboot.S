/*
 * uRISCV - A general purpose computer system simulator
 *
 * Copyright (C) 2004 Mauro Morsiani
 * Copyright (C) 2020 Mattia Biondi, Mikey Goldweber
 * Copyright (C) 2023 Gianmaria Rovelli
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "uriscv/arch.h"
#include "uriscv/bios.h"



 	/* Constants */

	STATUSMASK 	= 0xFFBFFFF3
	BIOSPANIC	= 0x00000080
	VECTAREANUM	= 8
	VECTPCPOS	= 12
	VECTSIZE	= 140
  VECTSTART = 0x0
	PROGVSTART	= 0x20001004

	PADDINGTLB	= 0x100 - 8
	PADDINGEXC	= 0x80 - 16

	.text
	.align	2
	.globl	coreboot
	.type	coreboot,@function
	// .ent	coreboot

coreboot:
/*
	.frame	fp,0,ra
	.mask	0x00000000,0
	.fmask	0x00000000,0
	.set noat

	.set noreorder
	.set nomacro
  */

	/* boot starts here */
	j	LCoreBootStart
	nop

	.space PADDINGTLB

	/* TLB miss should not happen during boot phase: jumps to BIOS PANIC */
	lui	t0, 0x0000
	ori	t0, t0, 0x80
	jr	t0
	nop

	.space PADDINGEXC

	/* no other exceptions should happen during boot phase:
	   jumps to BIOS PANIC */
	lui	t0, 0x0000
	ori	t0, t0, 0x80
	jr	t0
	nop

/*
	.set reorder
	.set macro
  */

LCoreBootStart:

  /*
  * Setting Exception PCs, both for general exceptions and for TLB
  * refills to cause a kernel panic(). This is done by loading
  * panic() BIOS routine address into both PC fields
  */

	li 	  a0, BIOS_EXEC_HANDLERS_ADDRS
	li 	  a1, BIOSPANIC      /* panic() address */
  sw    a1, 0(a0)         /* set TLB_Refill handler addr */
  sw    a1, 8(a0)         /* Set exception handler addr  */

	/* We use per/cpu bios reserved registers to point to the   */
	/*  BIOS exception vector and the Exception PCs/SPs */
	li	t0, BIOS_EXCPT_VECT_BASE
	li	t1, BIOS_DATA_PAGE_BASE
	sw	t1, 0(t0)

	li	t0, BIOS_PC_AREA_BASE
	li	t1, BIOS_EXEC_HANDLERS_ADDRS
	sw	t1, 0(t0)

	/*
	 * finally, we set EntryHI and STATUS for kernel:
	 * being EntryHI = 0, and STATUS good as it is now,
	 * do not touch anything
	 */

	/* Start the kernel: we find its starting address at a fixed
	   place in the aout. */
	li	a0, PROGVSTART
	lw	ra, 0(a0)

  li t0, VECTSTART
  csrrw zero,mtvec,t0

	jr	ra
	mret /* rfe */

	.end coreboot
	.size	 coreboot, . - coreboot
