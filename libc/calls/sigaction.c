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
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/calls/struct/sigaction-freebsd.h"
#include "libc/calls/struct/sigaction-linux.h"
#include "libc/calls/struct/sigaction-openbsd.h"
#include "libc/calls/struct/sigaction-xnu.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/typedef/sigaction_f.h"
#include "libc/calls/ucontext.h"
#include "libc/dce.h"
#include "libc/limits.h"
#include "libc/macros.h"
#include "libc/mem/mem.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/sa.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/errfuns.h"

union metasigaction {
  struct sigaction cosmo;
  struct sigaction$linux linux;
  struct sigaction$freebsd freebsd;
  struct sigaction$openbsd openbsd;
  struct sigaction$xnu_in xnu_in;
  struct sigaction$xnu_out xnu_out;
};

#define SWITCHEROO(S1, S2, A, B, C, D)                     \
  do {                                                     \
    autotype((S2).A) a = (typeof((S2).A))(S1).A;           \
    autotype((S2).B) b = (typeof((S2).B))(S1).B;           \
    autotype((S2).C) c = (typeof((S2).C))(S1).C;           \
    typeof((S2).D) d;                                      \
    memset(&d, 0, sizeof(d));                              \
    memcpy(&d, &((S1).D), MIN(sizeof(d), sizeof((S1).D))); \
    (S2).A = a;                                            \
    (S2).B = b;                                            \
    (S2).C = c;                                            \
    memset(&((S2).D), 0, sizeof((S2).D));                  \
    memcpy(&((S2).D), &d, MIN(sizeof(d), sizeof((S2).D))); \
  } while (0);

static void sigaction$cosmo2native(union metasigaction *sa) {
  if (!sa) return;
  switch (hostos) {
    case LINUX:
      SWITCHEROO(sa->cosmo, sa->linux, sa_handler, sa_flags, sa_restorer,
                 sa_mask);
      break;
    case XNU:
      SWITCHEROO(sa->cosmo, sa->xnu_in, sa_handler, sa_flags, sa_restorer,
                 sa_mask);
      break;
    case FREEBSD:
      SWITCHEROO(sa->cosmo, sa->freebsd, sa_handler, sa_flags, sa_flags,
                 sa_mask);
      break;
    case OPENBSD:
      SWITCHEROO(sa->cosmo, sa->openbsd, sa_handler, sa_flags, sa_flags,
                 sa_mask);
      break;
    default:
      abort();
  }
}

static void sigaction$native2cosmo(union metasigaction *sa) {
  if (!sa) return;
  switch (hostos) {
    case LINUX:
      SWITCHEROO(sa->linux, sa->cosmo, sa_handler, sa_flags, sa_restorer,
                 sa_mask);
      break;
    case XNU:
      SWITCHEROO(sa->xnu_out, sa->cosmo, sa_handler, sa_flags, sa_flags,
                 sa_mask);
      break;
    case FREEBSD:
      SWITCHEROO(sa->freebsd, sa->cosmo, sa_handler, sa_flags, sa_flags,
                 sa_mask);
      break;
    case OPENBSD:
      SWITCHEROO(sa->openbsd, sa->cosmo, sa_handler, sa_flags, sa_flags,
                 sa_mask);
      break;
    default:
      abort();
  }
}

/**
 * Installs handler for kernel interrupt, e.g.:
 *
 *   void GotCtrlC(int sig, siginfo_t *si, ucontext_t *ctx);
 *   struct sigaction sa = {.sa_sigaction = GotCtrlC,
 *                          .sa_flags = SA_RESETHAND|SA_RESTART|SA_SIGINFO};
 *   CHECK_NE(-1, sigaction(SIGINT, &sa, NULL));
 *
 * @see xsigaction() for a much better api
 * @asyncsignalsafe
 */
int(sigaction)(int sig, const struct sigaction *act, struct sigaction *oldact) {
  static_assert(sizeof(struct sigaction) > sizeof(struct sigaction$linux) &&
                sizeof(struct sigaction) > sizeof(struct sigaction$xnu_in) &&
                sizeof(struct sigaction) > sizeof(struct sigaction$xnu_out) &&
                sizeof(struct sigaction) > sizeof(struct sigaction$freebsd) &&
                sizeof(struct sigaction) > sizeof(struct sigaction$openbsd));
  int rc, rva, oldrva;
  struct sigaction *ap, copy;
  if (!(0 < sig && sig < NSIG) || sig == SIGKILL || sig == SIGSTOP) {
    return einval();
  }
  if (!act) {
    rva = (int32_t)(intptr_t)SIG_DFL;
  } else if ((intptr_t)act->sa_handler < kSigactionMinRva) {
    rva = (int)(intptr_t)act->sa_handler;
  } else if ((intptr_t)act->sa_handler >= (intptr_t)&_base + kSigactionMinRva &&
             (intptr_t)act->sa_handler < (intptr_t)&_base + INT_MAX) {
    rva = (int)((uintptr_t)act->sa_handler - (uintptr_t)&_base);
  } else {
    return efault();
  }
  if (!IsWindows()) {
    if (act) {
      memcpy(&copy, act, sizeof(copy));
      ap = &copy;
      if (IsXnu()) {
        ap->sa_restorer = (void *)&xnutrampoline;
        ap->sa_handler = (void *)&xnutrampoline;
      } else {
        if (IsLinux()) {
          if (!(ap->sa_flags & SA_RESTORER)) {
            ap->sa_flags |= SA_RESTORER;
            ap->sa_restorer = &__restore_rt;
          }
        }
        if (rva >= 0) {
          ap->sa_sigaction = (sigaction_f)__sigenter;
        }
      }
      sigaction$cosmo2native((union metasigaction *)ap);
    } else {
      ap = NULL;
    }
    rc = sigaction$sysv(
        sig, ap, oldact,
        (!IsXnu() ? 8 /* or linux whines */
                  : (int64_t)(intptr_t)oldact /* from go code */));
    if (rc != -1) sigaction$native2cosmo((union metasigaction *)oldact);
  } else {
    if (oldact) {
      memset(oldact, 0, sizeof(*oldact));
    }
    rc = 0;
  }
  if (rc != -1) {
    if (oldact) {
      oldrva = g_sighandrvas[sig];
      oldact->sa_sigaction = (sigaction_f)(
          oldrva < kSigactionMinRva ? oldrva : (intptr_t)&_base + oldrva);
    }
    if (act) {
      g_sighandrvas[sig] = rva;
    }
  }
  return rc;
}
