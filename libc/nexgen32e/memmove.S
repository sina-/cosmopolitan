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

/	Copies memory.
/
/	DEST and SRC may overlap.
/
/	@param	rdi is dest
/	@param	rsi is src
/	@param	rdx is number of bytes
/	@return	original rdi copied to rax
/	@asyncsignalsafe
memmove:
	mov	%rdi,%rax
/	𝑠𝑙𝑖𝑑𝑒
	.endfn	MemMove,globl,hidden

MemMove:
	.leafprologue
	.profilable
	push	%rcx
	push	%rdi
	push	%rsi
	mov	%rdx,%rcx
	cmp	%rsi,%rdi
	jb	1f
	lea	-1(%rdi,%rcx),%rdi
	lea	-1(%rsi,%rcx),%rsi
	std
1:	rep movsb
	cld
	pop	%rsi
	pop	%rdi
	pop	%rcx
	.leafepilogue
	.endfn	memmove,globl
	.source	__FILE__
