/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include "libc/limits.h"
#include "libc/macros.h"
#include "libc/str/str.h"
#include "libc/str/tpdecode.h"

compatfn int mbtowc(wchar_t *wc, const char *s, size_t n) {
  if (!s) return 0;
  alignas(8) char alt[ROUNDUP(MB_CUR_MAX, 8)];
  if (n < MB_CUR_MAX) {
    memset(alt, 0, sizeof(alt));
    memcpy(alt, s, n);
    s = &alt[0];
  }
  wint_t wi;
  int rc = tpdecode(s, &wi);
  if (wc) *wc = (wchar_t)wi;
  return rc;
}
