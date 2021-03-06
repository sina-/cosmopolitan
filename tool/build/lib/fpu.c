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
#include "ape/config.h"
#include "ape/lib/pc.h"
#include "libc/math.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "tool/build/lib/case.h"
#include "tool/build/lib/endian.h"
#include "tool/build/lib/flags.h"
#include "tool/build/lib/fpu.h"
#include "tool/build/lib/machine.h"
#include "tool/build/lib/memory.h"
#include "tool/build/lib/modrm.h"
#include "tool/build/lib/throw.h"
#include "tool/build/lib/x87.h"

#define FPUREG 0
#define MEMORY 1

#define DISP(x, y, z) (((x)&0b111) << 4 | (y) << 3 | (z))

static void OnFpuStackOverflow(struct Machine *m) {
  m->fpu.ie = true;
  m->fpu.c1 = true;
  m->fpu.sf = true;
}

static long double OnFpuStackUnderflow(struct Machine *m) {
  m->fpu.ie = true;
  m->fpu.c1 = false;
  m->fpu.sf = true;
  return -NAN;
}

void FpuPush(struct Machine *m, long double x) {
  if (FpuGetTag(m, -1) != kFpuTagEmpty) OnFpuStackOverflow(m);
  m->fpu.sp -= 1;
  *FpuSt(m, 0) = x;
  FpuSetTag(m, 0, kFpuTagValid);
}

long double FpuPop(struct Machine *m) {
  long double x;
  if (FpuGetTag(m, 0) != kFpuTagEmpty) {
    x = *FpuSt(m, 0);
    FpuSetTag(m, 0, kFpuTagEmpty);
    /* *FpuSt(m, 0) = -NAN; */
  } else {
    x = OnFpuStackUnderflow(m);
  }
  m->fpu.sp += 1;
  return x;
}

static long double St(struct Machine *m, int i) {
  if (FpuGetTag(m, i) == kFpuTagEmpty) OnFpuStackUnderflow(m);
  return *FpuSt(m, i);
}

static long double St0(struct Machine *m) {
  return St(m, 0);
}

static long double St1(struct Machine *m) {
  return St(m, 1);
}

static long double StRm(struct Machine *m) {
  return St(m, ModrmRm(m->xedd));
}

static void FpuClearRoundup(struct Machine *m) {
  m->fpu.c1 = false;
}

static void FpuClearOutOfRangeIndicator(struct Machine *m) {
  m->fpu.c2 = false;
}

static void FpuSetSt0(struct Machine *m, long double x) {
  *FpuSt(m, 0) = x;
}

static void FpuSetStRm(struct Machine *m, long double x) {
  *FpuSt(m, ModrmRm(m->xedd)) = x;
}

static void FpuSetStPop(struct Machine *m, int i, long double x) {
  *FpuSt(m, i) = x;
  FpuPop(m);
}

static void FpuSetStRmPop(struct Machine *m, long double x) {
  FpuSetStPop(m, ModrmRm(m->xedd), x);
}

static int16_t GetMemoryShort(struct Machine *m) {
  uint8_t b[2];
  return Read16(Load(m, m->fpu.dp, 2, b));
}

static int32_t GetMemoryInt(struct Machine *m) {
  uint8_t b[4];
  return Read32(Load(m, m->fpu.dp, 4, b));
}

static int64_t GetMemoryLong(struct Machine *m) {
  uint8_t b[8];
  return Read64(Load(m, m->fpu.dp, 8, b));
}

static float GetMemoryFloat(struct Machine *m) {
  float f;
  uint8_t b[4];
  memcpy(&f, Load(m, m->fpu.dp, 4, b), 4);
  return f;
}

static double GetMemoryDouble(struct Machine *m) {
  double f;
  uint8_t b[8];
  memcpy(&f, Load(m, m->fpu.dp, 8, b), 8);
  return f;
}

static long double GetMemoryLongDouble(struct Machine *m) {
  long double f;
  uint8_t b[10];
  memcpy(&f, Load(m, m->fpu.dp, 10, b), 10);
  return f;
}

static void SetMemoryShort(struct Machine *m, int16_t i) {
  void *p[2];
  uint8_t b[2];
  Write16(BeginStore(m, m->fpu.dp, 2, p, b), i);
  EndStore(m, m->fpu.dp, 2, p, b);
}

static void SetMemoryInt(struct Machine *m, int32_t i) {
  void *p[2];
  uint8_t b[4];
  Write32(BeginStore(m, m->fpu.dp, 4, p, b), i);
  EndStore(m, m->fpu.dp, 4, p, b);
}

static void SetMemoryLong(struct Machine *m, int64_t i) {
  void *p[2];
  uint8_t b[8];
  Write64(BeginStore(m, m->fpu.dp, 8, p, b), i);
  EndStore(m, m->fpu.dp, 8, p, b);
}

static void SetMemoryFloat(struct Machine *m, float f) {
  void *p[2];
  uint8_t b[4];
  memcpy(BeginStore(m, m->fpu.dp, 4, p, b), &f, 4);
  EndStore(m, m->fpu.dp, 4, p, b);
}

static void SetMemoryDouble(struct Machine *m, double f) {
  void *p[2];
  uint8_t b[8];
  memcpy(BeginStore(m, m->fpu.dp, 8, p, b), &f, 8);
  EndStore(m, m->fpu.dp, 8, p, b);
}

static void SetMemoryLdbl(struct Machine *m, long double f) {
  void *p[2];
  uint8_t b[10];
  memcpy(BeginStore(m, m->fpu.dp, 10, p, b), &f, 10);
  EndStore(m, m->fpu.dp, 10, p, b);
}

static long double FpuDivide(struct Machine *m, long double x, long double y) {
  if (y) {
    return x / y;
  } else {
    m->fpu.ze = true;
    return signbit(x) ? -INFINITY : INFINITY;
  }
}

static long double FpuRound(struct Machine *m, long double x) {
  switch (m->fpu.rc) {
    case 0:
      return nearbyintl(x);
    case 1:
      return floorl(x);
    case 2:
      return ceill(x);
    case 3:
      return truncl(x);
    default:
      unreachable;
  }
}

static void FpuCompare(struct Machine *m, long double y) {
  long double x;
  x = St0(m);
  m->fpu.c1 = false;
  if (!isnan(x) && !isnan(y)) {
    m->fpu.c0 = x < y;
    m->fpu.c2 = false;
    m->fpu.c3 = x == y;
  } else {
    m->fpu.c0 = true;
    m->fpu.c2 = true;
    m->fpu.c3 = true;
    m->fpu.ie = true;
  }
}

static void OpFxam(struct Machine *m) {
  long double x;
  x = *FpuSt(m, 0);
  m->fpu.c1 = !!signbit(x);
  if (FpuGetTag(m, 0) == kFpuTagEmpty) {
    m->fpu.c0 = true;
    m->fpu.c2 = false;
    m->fpu.c3 = true;
  } else {
    switch (fpclassify(x)) {
      case FP_NAN:
        m->fpu.c0 = true;
        m->fpu.c2 = false;
        m->fpu.c3 = false;
        break;
      case FP_INFINITE:
        m->fpu.c0 = true;
        m->fpu.c2 = true;
        m->fpu.c3 = false;
        break;
      case FP_ZERO:
        m->fpu.c0 = false;
        m->fpu.c2 = false;
        m->fpu.c3 = true;
        break;
      case FP_SUBNORMAL:
        m->fpu.c0 = false;
        m->fpu.c2 = true;
        m->fpu.c3 = true;
        break;
      case FP_NORMAL:
        m->fpu.c0 = false;
        m->fpu.c2 = true;
        m->fpu.c3 = false;
        break;
      default:
        abort();
    }
  }
}

static void OpFtst(struct Machine *m) {
  FpuCompare(m, 0);
}

static void OpFcmovb(struct Machine *m) {
  if (GetFlag(m->flags, FLAGS_CF)) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFcmove(struct Machine *m) {
  if (GetFlag(m->flags, FLAGS_ZF)) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFcmovbe(struct Machine *m) {
  if (GetFlag(m->flags, FLAGS_CF) || GetFlag(m->flags, FLAGS_ZF)) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFcmovu(struct Machine *m) {
  if (GetFlag(m->flags, FLAGS_PF)) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFcmovnb(struct Machine *m) {
  if (!GetFlag(m->flags, FLAGS_CF)) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFcmovne(struct Machine *m) {
  if (!GetFlag(m->flags, FLAGS_ZF)) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFcmovnbe(struct Machine *m) {
  if (!(GetFlag(m->flags, FLAGS_CF) || GetFlag(m->flags, FLAGS_ZF))) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFcmovnu(struct Machine *m) {
  if (!GetFlag(m->flags, FLAGS_PF)) {
    FpuSetSt0(m, StRm(m));
  }
}

static void OpFchs(struct Machine *m) {
  FpuSetSt0(m, -St0(m));
}

static void OpFabs(struct Machine *m) {
  FpuSetSt0(m, fabsl(St0(m)));
}

static void OpF2xm1(struct Machine *m) {
  FpuSetSt0(m, f2xm1(St0(m)));
}

static void OpFyl2x(struct Machine *m) {
  FpuSetStPop(m, 1, fyl2x(St0(m), St1(m)));
}

static void OpFyl2xp1(struct Machine *m) {
  FpuSetStPop(m, 1, fyl2xp1(St0(m), St1(m)));
}

static void OpFcos(struct Machine *m) {
  FpuClearOutOfRangeIndicator(m);
  FpuSetSt0(m, cosl(St0(m)));
}

static void OpFsin(struct Machine *m) {
  FpuClearOutOfRangeIndicator(m);
  FpuSetSt0(m, sinl(St0(m)));
}

static void OpFptan(struct Machine *m) {
  FpuClearOutOfRangeIndicator(m);
  FpuSetSt0(m, tanl(St0(m)));
  FpuPush(m, 1);
}

static void OpFsincos(struct Machine *m) {
  long double tsin, tcos;
  FpuClearOutOfRangeIndicator(m);
  sincosl(St0(m), &tsin, &tcos);
  FpuSetSt0(m, tsin);
  FpuPush(m, tcos);
}

static void OpFpatan(struct Machine *m) {
  FpuClearRoundup(m);
  FpuSetStPop(m, 1, atan2l(St0(m), St1(m)));
}

static void OpFcom(struct Machine *m) {
  FpuCompare(m, StRm(m));
}

static void OpFcomp(struct Machine *m) {
  FpuCompare(m, StRm(m));
  FpuPop(m);
}

static void OpFaddStEst(struct Machine *m) {
  FpuSetSt0(m, St0(m) + StRm(m));
}

static void OpFmulStEst(struct Machine *m) {
  FpuSetSt0(m, St0(m) * StRm(m));
}

static void OpFsubStEst(struct Machine *m) {
  FpuSetSt0(m, St0(m) - StRm(m));
}

static void OpFsubrStEst(struct Machine *m) {
  FpuSetSt0(m, StRm(m) - St0(m));
}

static void OpFdivStEst(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, St0(m), StRm(m)));
}

static void OpFdivrStEst(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, StRm(m), St0(m)));
}

static void OpFaddEstSt(struct Machine *m) {
  FpuSetStRm(m, StRm(m) + St0(m));
}

static void OpFmulEstSt(struct Machine *m) {
  FpuSetStRm(m, StRm(m) * St0(m));
}

static void OpFsubEstSt(struct Machine *m) {
  FpuSetStRm(m, StRm(m) - St0(m));
}

static void OpFsubrEstSt(struct Machine *m) {
  FpuSetStRm(m, St0(m) - StRm(m));
}

static void OpFdivEstSt(struct Machine *m) {
  FpuSetStRm(m, FpuDivide(m, StRm(m), St0(m)));
}

static void OpFdivrEstSt(struct Machine *m) {
  FpuSetStRm(m, FpuDivide(m, St0(m), StRm(m)));
}

static void OpFaddp(struct Machine *m) {
  FpuSetStRmPop(m, St0(m) + StRm(m));
}

static void OpFmulp(struct Machine *m) {
  FpuSetStRmPop(m, St0(m) * StRm(m));
}

static void OpFcompp(struct Machine *m) {
  OpFcomp(m);
  FpuPop(m);
}

static void OpFsubp(struct Machine *m) {
  FpuSetStRmPop(m, St0(m) - StRm(m));
}

static void OpFsubrp(struct Machine *m) {
  FpuSetStPop(m, 1, StRm(m) - St0(m));
}

static void OpFdivp(struct Machine *m) {
  FpuSetStRmPop(m, FpuDivide(m, St0(m), StRm(m)));
}

static void OpFdivrp(struct Machine *m) {
  FpuSetStRmPop(m, FpuDivide(m, StRm(m), St0(m)));
}

static void OpFadds(struct Machine *m) {
  FpuSetSt0(m, St0(m) + GetMemoryFloat(m));
}

static void OpFmuls(struct Machine *m) {
  FpuSetSt0(m, St0(m) * GetMemoryFloat(m));
}

static void OpFcoms(struct Machine *m) {
  FpuCompare(m, GetMemoryFloat(m));
}

static void OpFcomps(struct Machine *m) {
  OpFcoms(m);
  FpuPop(m);
}

static void OpFsubs(struct Machine *m) {
  FpuSetSt0(m, St0(m) - GetMemoryFloat(m));
}

static void OpFsubrs(struct Machine *m) {
  FpuSetSt0(m, GetMemoryFloat(m) - St0(m));
}

static void OpFdivs(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, St0(m), GetMemoryFloat(m)));
}

static void OpFdivrs(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, GetMemoryFloat(m), St0(m)));
}

static void OpFaddl(struct Machine *m) {
  FpuSetSt0(m, St0(m) + GetMemoryDouble(m));
}

static void OpFmull(struct Machine *m) {
  FpuSetSt0(m, St0(m) * GetMemoryDouble(m));
}

static void OpFcoml(struct Machine *m) {
  FpuCompare(m, GetMemoryDouble(m));
}

static void OpFcompl(struct Machine *m) {
  FpuCompare(m, GetMemoryDouble(m));
  FpuPop(m);
}

static void OpFsubl(struct Machine *m) {
  FpuSetSt0(m, St0(m) - GetMemoryDouble(m));
}

static void OpFsubrl(struct Machine *m) {
  FpuSetSt0(m, GetMemoryDouble(m) - St0(m));
}

static void OpFdivl(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, St0(m), GetMemoryDouble(m)));
}

static void OpFdivrl(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, GetMemoryDouble(m), St0(m)));
}

static void OpFiaddl(struct Machine *m) {
  FpuSetSt0(m, St0(m) + GetMemoryInt(m));
}

static void OpFimull(struct Machine *m) {
  FpuSetSt0(m, St0(m) * GetMemoryInt(m));
}

static void OpFicoml(struct Machine *m) {
  FpuCompare(m, GetMemoryInt(m));
}

static void OpFicompl(struct Machine *m) {
  OpFicoml(m);
  FpuPop(m);
}

static void OpFisubl(struct Machine *m) {
  FpuSetSt0(m, St0(m) - GetMemoryInt(m));
}

static void OpFisubrl(struct Machine *m) {
  FpuSetSt0(m, GetMemoryInt(m) - St0(m));
}

static void OpFidivl(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, St0(m), GetMemoryInt(m)));
}

static void OpFidivrl(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, GetMemoryInt(m), St0(m)));
}

static void OpFiadds(struct Machine *m) {
  FpuSetSt0(m, St0(m) + GetMemoryShort(m));
}

static void OpFimuls(struct Machine *m) {
  FpuSetSt0(m, St0(m) * GetMemoryShort(m));
}

static void OpFicoms(struct Machine *m) {
  FpuCompare(m, GetMemoryShort(m));
}

static void OpFicomps(struct Machine *m) {
  OpFicoms(m);
  FpuPop(m);
}

static void OpFisubs(struct Machine *m) {
  FpuSetSt0(m, St0(m) - GetMemoryShort(m));
}

static void OpFisubrs(struct Machine *m) {
  FpuSetSt0(m, GetMemoryShort(m) - St0(m));
}

static void OpFidivs(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, St0(m), GetMemoryShort(m)));
}

static void OpFidivrs(struct Machine *m) {
  FpuSetSt0(m, FpuDivide(m, GetMemoryShort(m), St0(m)));
}

static void OpFsqrt(struct Machine *m) {
  FpuClearRoundup(m);
  FpuSetSt0(m, sqrtl(St0(m)));
}

static void OpFrndint(struct Machine *m) {
  FpuSetSt0(m, FpuRound(m, St0(m)));
}

static void OpFscale(struct Machine *m) {
  FpuClearRoundup(m);
  FpuSetSt0(m, fscale(St0(m), St1(m)));
}

static void OpFprem(struct Machine *m) {
  FpuSetSt0(m, fprem(St0(m), St1(m), &m->fpu.sw));
}

static void OpFprem1(struct Machine *m) {
  FpuSetSt0(m, fprem1(St0(m), St1(m), &m->fpu.sw));
}

static void OpFdecstp(struct Machine *m) {
  --m->fpu.sp;
}

static void OpFincstp(struct Machine *m) {
  ++m->fpu.sp;
}

static void OpFxtract(struct Machine *m) {
  long double x;
  x = St0(m);
  FpuSetSt0(m, logbl(x));
  FpuPush(m, significandl(x));
}

static void OpFld(struct Machine *m) {
  FpuPush(m, StRm(m));
}

static void OpFlds(struct Machine *m) {
  FpuPush(m, GetMemoryFloat(m));
}

static void OpFsts(struct Machine *m) {
  SetMemoryFloat(m, St0(m));
}

static void OpFstps(struct Machine *m) {
  OpFsts(m);
  FpuPop(m);
}

static void OpFstpt(struct Machine *m) {
  SetMemoryLdbl(m, FpuPop(m));
}

static void OpFstl(struct Machine *m) {
  SetMemoryDouble(m, St0(m));
}

static void OpFstpl(struct Machine *m) {
  OpFstl(m);
  FpuPop(m);
}

static void OpFst(struct Machine *m) {
  FpuSetStRm(m, St0(m));
}

static void OpFstp(struct Machine *m) {
  FpuSetStRmPop(m, St0(m));
}

static void OpFxch(struct Machine *m) {
  long double t;
  t = StRm(m);
  FpuSetStRm(m, St0(m));
  FpuSetSt0(m, t);
}

static void OpFldcw(struct Machine *m) {
  m->fpu.cw = GetMemoryShort(m);
}

static void OpFldt(struct Machine *m) {
  FpuPush(m, GetMemoryLongDouble(m));
}

static void OpFldl(struct Machine *m) {
  FpuPush(m, GetMemoryDouble(m));
}

static void OpFldConstant(struct Machine *m) {
  long double x;
  switch (ModrmRm(m->xedd)) {
    case 0:
      x = fld1();
      break;
    case 1:
      x = fldl2t();
      break;
    case 2:
      x = fldl2e();
      break;
    case 3:
      x = fldpi();
      break;
    case 4:
      x = fldlg2();
      break;
    case 5:
      x = fldln2();
      break;
    case 6:
      x = fldz();
      break;
    default:
      OpUd(m);
  }
  FpuPush(m, x);
}

static void OpFstcw(struct Machine *m) {
  SetMemoryShort(m, m->fpu.cw);
}

static void OpFilds(struct Machine *m) {
  FpuPush(m, GetMemoryShort(m));
}

static void OpFildl(struct Machine *m) {
  FpuPush(m, GetMemoryInt(m));
}

static void OpFildll(struct Machine *m) {
  FpuPush(m, GetMemoryLong(m));
}

static void OpFisttpl(struct Machine *m) {
  SetMemoryInt(m, FpuPop(m));
}

static void OpFisttpll(struct Machine *m) {
  SetMemoryLong(m, FpuPop(m));
}

static void OpFisttps(struct Machine *m) {
  SetMemoryShort(m, FpuPop(m));
}

static void OpFists(struct Machine *m) {
  SetMemoryShort(m, FpuRound(m, St0(m)));
}

static void OpFistl(struct Machine *m) {
  SetMemoryInt(m, FpuRound(m, St0(m)));
}

static void OpFistll(struct Machine *m) {
  SetMemoryLong(m, FpuRound(m, St0(m)));
}

static void OpFistpl(struct Machine *m) {
  OpFistl(m);
  FpuPop(m);
}

static void OpFistpll(struct Machine *m) {
  OpFistll(m);
  FpuPop(m);
}

static void OpFistps(struct Machine *m) {
  OpFists(m);
  FpuPop(m);
}

void OpFcomi(struct Machine *m) {
  long double x, y;
  x = St0(m);
  y = StRm(m);
  if (!isnan(x) && !isnan(y)) {
    m->flags = SetFlag(m->flags, FLAGS_ZF, x == y);
    m->flags = SetFlag(m->flags, FLAGS_CF, x < y);
    m->flags = SetFlag(m->flags, FLAGS_PF, false);
  } else {
    m->fpu.ie = true;
    m->flags = SetFlag(m->flags, FLAGS_ZF, true);
    m->flags = SetFlag(m->flags, FLAGS_CF, true);
    m->flags = SetFlag(m->flags, FLAGS_PF, true);
  }
}

static void OpFucom(struct Machine *m) {
  FpuCompare(m, StRm(m));
}

static void OpFucomp(struct Machine *m) {
  FpuCompare(m, StRm(m));
  FpuPop(m);
}

static void OpFcomip(struct Machine *m) {
  OpFcomi(m);
  FpuPop(m);
}

static void OpFucomi(struct Machine *m) {
  OpFcomi(m);
}

static void OpFucomip(struct Machine *m) {
  OpFcomip(m);
}

static void OpFfree(struct Machine *m) {
  FpuSetTag(m, ModrmRm(m->xedd), kFpuTagEmpty);
}

static void OpFfreep(struct Machine *m) {
  OpFfree(m);
  FpuPop(m);
}

static void OpFstswMw(struct Machine *m) {
  SetMemoryShort(m, m->fpu.sw);
}

static void OpFstswAx(struct Machine *m) {
  Write16(m->ax, m->fpu.sw);
}

static void SetFpuEnv(struct Machine *m, uint8_t p[28]) {
  Write16(p + 0, m->fpu.cw);
  Write16(p + 4, m->fpu.sw);
  Write16(p + 8, m->fpu.tw);
  Write64(p + 12, m->fpu.ip);
  Write16(p + 18, m->fpu.op);
  Write64(p + 20, m->fpu.dp);
}

static void GetFpuEnv(struct Machine *m, uint8_t p[28]) {
  m->fpu.cw = Read16(p + 0);
  m->fpu.sw = Read16(p + 4);
  m->fpu.tw = Read16(p + 8);
}

static void OpFstenv(struct Machine *m) {
  void *p[2];
  uint8_t b[28];
  SetFpuEnv(m, BeginStore(m, m->fpu.dp, sizeof(b), p, b));
  EndStore(m, m->fpu.dp, sizeof(b), p, b);
}

static void OpFldenv(struct Machine *m) {
  uint8_t b[28];
  GetFpuEnv(m, Load(m, m->fpu.dp, sizeof(b), b));
}

static void OpFsave(struct Machine *m) {
  long i;
  void *p[2];
  long double x;
  uint8_t *a, b[108];
  a = BeginStore(m, m->fpu.dp, sizeof(b), p, b);
  SetFpuEnv(m, a);
  for (i = 0; i < 8; ++i) {
    x = *FpuSt(m, i);
    memcpy(a + 28 + i * 10, &x, 10);
  }
  EndStore(m, m->fpu.dp, sizeof(b), p, b);
  OpFinit(m);
}

static void OpFrstor(struct Machine *m) {
  long i;
  long double x;
  uint8_t *a, b[108];
  a = Load(m, m->fpu.dp, sizeof(b), b);
  GetFpuEnv(m, a);
  for (i = 0; i < 8; ++i) {
    memset(&x, 0, sizeof(x));
    memcpy(&x, a + 28 + i * 10, 10);
    *FpuSt(m, i) = x;
  }
}

static void OpFnclex(struct Machine *m) {
  m->fpu.ie = false;
  m->fpu.de = false;
  m->fpu.ze = false;
  m->fpu.oe = false;
  m->fpu.ue = false;
  m->fpu.pe = false;
  m->fpu.es = false;
  m->fpu.bf = false;
}

void OpFinit(struct Machine *m) {
  m->fpu.cw = X87_NORMAL;
  m->fpu.sw = 0;
  m->fpu.tw = -1;
}

void OpFwait(struct Machine *m) {
  if ((m->fpu.ie & !m->fpu.im) | (m->fpu.de & !m->fpu.dm) |
      (m->fpu.ze & !m->fpu.zm) | (m->fpu.oe & !m->fpu.om) |
      (m->fpu.ue & !m->fpu.um) | (m->fpu.pe & !m->fpu.pm) |
      (m->fpu.sf & !m->fpu.im)) {
    HaltMachine(m, kMachineFpuException);
  }
}

static void OpFnop(struct Machine *m) {
  /* do nothing */
}

void OpFpu(struct Machine *m) {
  unsigned op;
  bool ismemory;
  op = m->xedd->op.opcode & 0b111;
  ismemory = ModrmMod(m->xedd) != 0b11;
  m->fpu.ip = m->ip - m->xedd->length;
  m->fpu.op = op << 8 | m->xedd->op.modrm;
  m->fpu.dp = ismemory ? ComputeAddress(m) : 0;
  switch (DISP(op, ismemory, m->xedd->op.reg)) {
    CASE(DISP(0xD8, FPUREG, 0), OpFaddStEst(m));
    CASE(DISP(0xD8, FPUREG, 1), OpFmulStEst(m));
    CASE(DISP(0xD8, FPUREG, 2), OpFcom(m));
    CASE(DISP(0xD8, FPUREG, 3), OpFcomp(m));
    CASE(DISP(0xD8, FPUREG, 4), OpFsubStEst(m));
    CASE(DISP(0xD8, FPUREG, 5), OpFsubrStEst(m));
    CASE(DISP(0xD8, FPUREG, 6), OpFdivStEst(m));
    CASE(DISP(0xD8, FPUREG, 7), OpFdivrStEst(m));
    CASE(DISP(0xD8, MEMORY, 0), OpFadds(m));
    CASE(DISP(0xD8, MEMORY, 1), OpFmuls(m));
    CASE(DISP(0xD8, MEMORY, 2), OpFcoms(m));
    CASE(DISP(0xD8, MEMORY, 3), OpFcomps(m));
    CASE(DISP(0xD8, MEMORY, 4), OpFsubs(m));
    CASE(DISP(0xD8, MEMORY, 5), OpFsubrs(m));
    CASE(DISP(0xD8, MEMORY, 6), OpFdivs(m));
    CASE(DISP(0xD8, MEMORY, 7), OpFdivrs(m));
    CASE(DISP(0xD9, FPUREG, 0), OpFld(m));
    CASE(DISP(0xD9, FPUREG, 1), OpFxch(m));
    CASE(DISP(0xD9, FPUREG, 2), OpFnop(m));
    CASE(DISP(0xD9, FPUREG, 3), OpFstp(m));
    CASE(DISP(0xD9, FPUREG, 5), OpFldConstant(m));
    CASE(DISP(0xD9, MEMORY, 0), OpFlds(m));
    CASE(DISP(0xD9, MEMORY, 2), OpFsts(m));
    CASE(DISP(0xD9, MEMORY, 3), OpFstps(m));
    CASE(DISP(0xD9, MEMORY, 4), OpFldenv(m));
    CASE(DISP(0xD9, MEMORY, 5), OpFldcw(m));
    CASE(DISP(0xD9, MEMORY, 6), OpFstenv(m));
    CASE(DISP(0xD9, MEMORY, 7), OpFstcw(m));
    CASE(DISP(0xDA, FPUREG, 0), OpFcmovb(m));
    CASE(DISP(0xDA, FPUREG, 1), OpFcmove(m));
    CASE(DISP(0xDA, FPUREG, 2), OpFcmovbe(m));
    CASE(DISP(0xDA, FPUREG, 3), OpFcmovu(m));
    CASE(DISP(0xDA, MEMORY, 0), OpFiaddl(m));
    CASE(DISP(0xDa, MEMORY, 1), OpFimull(m));
    CASE(DISP(0xDa, MEMORY, 2), OpFicoml(m));
    CASE(DISP(0xDa, MEMORY, 3), OpFicompl(m));
    CASE(DISP(0xDa, MEMORY, 4), OpFisubl(m));
    CASE(DISP(0xDa, MEMORY, 5), OpFisubrl(m));
    CASE(DISP(0xDa, MEMORY, 6), OpFidivl(m));
    CASE(DISP(0xDa, MEMORY, 7), OpFidivrl(m));
    CASE(DISP(0xDb, FPUREG, 0), OpFcmovnb(m));
    CASE(DISP(0xDb, FPUREG, 1), OpFcmovne(m));
    CASE(DISP(0xDb, FPUREG, 2), OpFcmovnbe(m));
    CASE(DISP(0xDb, FPUREG, 3), OpFcmovnu(m));
    CASE(DISP(0xDb, FPUREG, 5), OpFucomi(m));
    CASE(DISP(0xDb, FPUREG, 6), OpFcomi(m));
    CASE(DISP(0xDb, MEMORY, 0), OpFildl(m));
    CASE(DISP(0xDb, MEMORY, 1), OpFisttpl(m));
    CASE(DISP(0xDb, MEMORY, 2), OpFistl(m));
    CASE(DISP(0xDb, MEMORY, 3), OpFistpl(m));
    CASE(DISP(0xDb, MEMORY, 5), OpFldt(m));
    CASE(DISP(0xDb, MEMORY, 7), OpFstpt(m));
    CASE(DISP(0xDc, FPUREG, 0), OpFaddEstSt(m));
    CASE(DISP(0xDc, FPUREG, 1), OpFmulEstSt(m));
    CASE(DISP(0xDc, FPUREG, 2), OpFcom(m));
    CASE(DISP(0xDc, FPUREG, 3), OpFcomp(m));
    CASE(DISP(0xDc, FPUREG, 4), OpFsubEstSt(m));
    CASE(DISP(0xDc, FPUREG, 5), OpFsubrEstSt(m));
    CASE(DISP(0xDc, FPUREG, 6), OpFdivEstSt(m));
    CASE(DISP(0xDc, FPUREG, 7), OpFdivrEstSt(m));
    CASE(DISP(0xDc, MEMORY, 0), OpFaddl(m));
    CASE(DISP(0xDc, MEMORY, 1), OpFmull(m));
    CASE(DISP(0xDc, MEMORY, 2), OpFcoml(m));
    CASE(DISP(0xDc, MEMORY, 3), OpFcompl(m));
    CASE(DISP(0xDc, MEMORY, 4), OpFsubl(m));
    CASE(DISP(0xDc, MEMORY, 5), OpFsubrl(m));
    CASE(DISP(0xDc, MEMORY, 6), OpFdivl(m));
    CASE(DISP(0xDc, MEMORY, 7), OpFdivrl(m));
    CASE(DISP(0xDd, FPUREG, 0), OpFfree(m));
    CASE(DISP(0xDd, FPUREG, 1), OpFxch(m));
    CASE(DISP(0xDd, FPUREG, 2), OpFst(m));
    CASE(DISP(0xDd, FPUREG, 3), OpFstp(m));
    CASE(DISP(0xDd, FPUREG, 4), OpFucom(m));
    CASE(DISP(0xDd, FPUREG, 5), OpFucomp(m));
    CASE(DISP(0xDd, MEMORY, 0), OpFldl(m));
    CASE(DISP(0xDd, MEMORY, 1), OpFisttpll(m));
    CASE(DISP(0xDd, MEMORY, 2), OpFstl(m));
    CASE(DISP(0xDd, MEMORY, 3), OpFstpl(m));
    CASE(DISP(0xDd, MEMORY, 4), OpFrstor(m));
    CASE(DISP(0xDd, MEMORY, 6), OpFsave(m));
    CASE(DISP(0xDd, MEMORY, 7), OpFstswMw(m));
    CASE(DISP(0xDe, FPUREG, 0), OpFaddp(m));
    CASE(DISP(0xDe, FPUREG, 1), OpFmulp(m));
    CASE(DISP(0xDe, FPUREG, 2), OpFcomp(m));
    CASE(DISP(0xDe, FPUREG, 3), OpFcompp(m));
    CASE(DISP(0xDe, FPUREG, 4), OpFsubp(m));
    CASE(DISP(0xDe, FPUREG, 5), OpFsubrp(m));
    CASE(DISP(0xDe, FPUREG, 6), OpFdivp(m));
    CASE(DISP(0xDe, FPUREG, 7), OpFdivrp(m));
    CASE(DISP(0xDe, MEMORY, 0), OpFiadds(m));
    CASE(DISP(0xDe, MEMORY, 1), OpFimuls(m));
    CASE(DISP(0xDe, MEMORY, 2), OpFicoms(m));
    CASE(DISP(0xDe, MEMORY, 3), OpFicomps(m));
    CASE(DISP(0xDe, MEMORY, 4), OpFisubs(m));
    CASE(DISP(0xDe, MEMORY, 5), OpFisubrs(m));
    CASE(DISP(0xDe, MEMORY, 6), OpFidivs(m));
    CASE(DISP(0xDe, MEMORY, 7), OpFidivrs(m));
    CASE(DISP(0xDf, FPUREG, 0), OpFfreep(m));
    CASE(DISP(0xDf, FPUREG, 1), OpFxch(m));
    CASE(DISP(0xDf, FPUREG, 2), OpFstp(m));
    CASE(DISP(0xDf, FPUREG, 3), OpFstp(m));
    CASE(DISP(0xDf, FPUREG, 4), OpFstswAx(m));
    CASE(DISP(0xDf, FPUREG, 5), OpFucomip(m));
    CASE(DISP(0xDf, FPUREG, 6), OpFcomip(m));
    CASE(DISP(0xDf, MEMORY, 0), OpFilds(m));
    CASE(DISP(0xDf, MEMORY, 1), OpFisttps(m));
    CASE(DISP(0xDf, MEMORY, 2), OpFists(m));
    CASE(DISP(0xDf, MEMORY, 3), OpFistps(m));
    CASE(DISP(0xDf, MEMORY, 5), OpFildll(m));
    CASE(DISP(0xDf, MEMORY, 7), OpFistpll(m));
    case DISP(0xD9, FPUREG, 4):
      switch (ModrmRm(m->xedd)) {
        CASE(0, OpFchs(m));
        CASE(1, OpFabs(m));
        CASE(4, OpFtst(m));
        CASE(5, OpFxam(m));
        default:
          OpUd(m);
      }
      break;
    case DISP(0xD9, FPUREG, 6):
      switch (ModrmRm(m->xedd)) {
        CASE(0, OpF2xm1(m));
        CASE(1, OpFyl2x(m));
        CASE(2, OpFptan(m));
        CASE(3, OpFpatan(m));
        CASE(4, OpFxtract(m));
        CASE(5, OpFprem1(m));
        CASE(6, OpFdecstp(m));
        CASE(7, OpFincstp(m));
        default:
          unreachable;
      }
      break;
    case DISP(0xD9, FPUREG, 7):
      switch (ModrmRm(m->xedd)) {
        CASE(0, OpFprem(m));
        CASE(1, OpFyl2xp1(m));
        CASE(2, OpFsqrt(m));
        CASE(3, OpFsincos(m));
        CASE(4, OpFrndint(m));
        CASE(5, OpFscale(m));
        CASE(6, OpFsin(m));
        CASE(7, OpFcos(m));
        default:
          unreachable;
      }
      break;
    case DISP(0xDb, FPUREG, 4):
      switch (ModrmRm(m->xedd)) {
        CASE(2, OpFnclex(m));
        CASE(3, OpFinit(m));
        default:
          OpUd(m);
      }
      break;
    default:
      OpUd(m);
  }
}
