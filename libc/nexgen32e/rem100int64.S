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

/	Returns 𝑥 % 100.
/
/	@param	rdi int64 𝑥
/	@return	rax has remainder
rem100int64:
	mov	%rdi,%rax
	movabsq	$-6640827866535438581,%rdx
	imul	%rdx
	lea	(%rdx,%rdi),%rax
	mov	%rdi,%rdx
	sar	$6,%rax
	sar	$63,%rdx
	sub	%rdx,%rax
	imul	$100,%rax,%rax
	sub	%rax,%rdi
	mov	%rdi,%rax
	ret
	.endfn	rem100int64,globl
	.source	__FILE__
