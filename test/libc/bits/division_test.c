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
#include "libc/bits/bits.h"
#include "libc/bits/progn.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"

#define L(x) ((int64_t)(x))
#define S(x) ((int128_t)(x))
#define U(x) ((uint128_t)(x))

TEST(division, testUnsigned) {
  volatile uint128_t x;
  EXPECT_EQ(U(20769187431582143),
            PROGN(x = U(1000000000123123123), (U(1125899906842624) << 64) / x));
  EXPECT_EQ((U(26807140639110) << 64) | U(1756832768924719201),
            PROGN(x = U(42), (U(1125899906842624) << 64) / x));
}

TEST(division, testSigned) {
  volatile int128_t x;
  EXPECT_EQ(S(20769187431582143),
            PROGN(x = S(1000000000123123123), (S(1125899906842624) << 64) / x));
  EXPECT_EQ(S(26807140639110) << 64 | S(1756832768924719201),
            PROGN(x = S(42), (S(1125899906842624) << 64) / x));
}

BENCH(divmodti4, bench) {
  volatile int128_t x;
  EZBENCH2("divmodti4 small / small", donothing,
           PROGN(x = S(42), x = S(112589990684) / x));
  EZBENCH2("divmodti4 small / large", donothing,
           PROGN(x = U(123) << 64 | 123, x = S(112589990684) / x));
  EZBENCH2("divmodti4 large / small", donothing,
           PROGN(x = 123, x = (S(1125899906842624) << 64 | 334) / x));
  EZBENCH2(
      "divmodti4 large / large", donothing,
      PROGN(x = U(123) << 64 | 123, x = (S(1125899906842624) << 64 | 334) / x));
}

BENCH(idiv32, bench) {
  volatile int32_t x;
  EZBENCH2("idiv32", donothing,
           PROGN(x = L(1000000000123123123), x = L(1125899906842624) / x));
}

BENCH(idiv64, bench) {
  volatile int64_t x;
  EZBENCH2("idiv64", donothing,
           PROGN(x = L(1000000000123123123), x = L(1125899906842624) / x));
}
