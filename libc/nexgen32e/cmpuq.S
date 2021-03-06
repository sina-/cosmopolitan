/*-*- mode:unix-assembly; indent-tabs-mode:t; tab-width:8; coding:utf-8     -*-│
│vi: set et ft=asm ts=8 tw=8 fenc=utf-8                                     :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ This program is free software; you can redistribute it and/or modify         │
│ it under the terms of the GNU General Public License as published by         │
│ the Free Software Foundation; version 2 of the License.                      │
│                                                                              │
│ This program is distributed in the hope that it will be useful, but          │
│ WITHOUT ANY WARRANTY; without even the implied warranty of                   │
│ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU             │
│ General Public License for more details.                                     │
│                                                                              │
│ You should have received a copy of the GNU General Public License            │
│ along with this program; if not, write to the Free Software                  │
│ Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA                │
│ 02110-1301 USA                                                               │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/macros.h"

/	Compares 64-bit unsigned integers.
/
/	@param	rdi points to left integer
/	@param	rsi points to right integer
/	@return	<0, 0, or >0 based on comparison
	.align	16
cmpuq:	.leafprologue
	.profilable
	xor	%eax,%eax
	cmpsq
	cmovb	.Lone(%rip),%eax
	cmova	.Lneg1(%rip),%eax
	.leafepilogue
	.endfn	cmpuq,globl

	.rodata.cst4
.Lone:	.long	1
	.endobj	.Lone
.Lneg1:	.long	-1
	.endobj	.Lneg1
	.previous
	.source	__FILE__
