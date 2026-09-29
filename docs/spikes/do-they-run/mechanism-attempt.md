---
title: "Trying to find the mechanism, and failing twice"
status: open
state: not established
created: 2026-09-11
updated: 2026-09-11
---

# The mechanism is still not identified

> **Closed 2026-09-11, and not by me.** The mechanism was documented in this
> workspace the whole time, in `rust-aros/TARGET-NOTES.md`:
>
> | | how `SysBase` is passed |
> |---|---|
> | AROS One 1.3 | in `%r12`, register-call: `mov %rax,%r12; call *-0x108(%rax)` |
> | mainline | as an ordinary third argument: `mov %rax,%rdx; jmp *-0x108(%rdx)` |
>
> `proto/exec.h` inlines the library call, and the inline differs by SDK.
> Worse: that exact file had been pointed out at the start of this work, and I
> quoted the surrounding paragraph into a spike document without opening it.
> Two "failed attempts" at disassembly were failures to look where I had been
> told to look.
>
> My false start 1 was closer than I judged: I was counting the right
> registers and dismissed the result as an artifact of register allocation. It
> is an artifact *as a count*, but the underlying difference was real. Being
> sceptical of a weak method was right; not checking the documented answer was
> not.
>
> The account below is kept as written.


The boundary is measured: the same binaries run on AROS One and crash on
mainline, ours and the official nightly alike. This is the attempt to find out
*why*, narrowed as suggested to ZapHod and a single library call. **It did not
succeed**, and it produced two conclusions that looked right and were not. Both
are written down because each would otherwise be reached again.

## False start 1: counting which register holds the base

Counting, across five binaries, how often the library base is moved into `%rdx`
versus `%r12` immediately before an indirect library call gave a striking split:

```
abitest  (ours)     base->%rdx: 18   base->%r12: 2
pkg      (ours)     base->%rdx: 34   base->%r12: 2
Zaphod   (archive)  base->%rdx: 1    base->%r12: 72
ZuneCalc (archive)  base->%rdx: 0    base->%r12: 75
SMB2-GUI (archive)  base->%rdx: 1    base->%r12: 67
```

That looks like two calling conventions and matches a difference the platform
notes describe, which is exactly why it was convincing.

**It is an artifact.** `%r12` is callee-saved: a program that holds a library
base across several calls keeps it there, and that is ordinary register
allocation, not a convention. `%rdx` happens to be the third argument register,
so a base that is *already* in `%rdx` needs no move and is not counted at all.
The metric measured a code shape, not a calling convention.

## False start 2: comparing "the same call" in both binaries

Narrowing to one call (`FindTask`, LVO 49, vector offset `-0x188`) gave a
clean-looking contrast:

```
abitest:   mov %rdi,%rsi        ; base into the second argument
           xor %edi,%edi
           callq *-0x188(%rax)

Zaphod:    mov %rdi,%r12
           xor %edi,%edi
           callq *-0x188(%rdx)  ; %rsi never loaded
```

The obvious reading is that one passes the base as an argument and the other
does not. **That reading is unsupported**, because a vector offset is
per-library: `-0x188` in `Zaphod` may be a call into a completely different
library whose 49th vector happens to sit at the same offset. The two sequences
were not established to be the same function, so they cannot be compared.

## What the SDK headers do say

One real difference was found, and it is smaller than it needs to be:

- mainline's `aros/x86_64/cpu.h` defines
  `__AROS_LP_BASE(basetype,basename)  void *`
- AROS One's `aros/x86_64/cpu.h` does not define it at all, so it falls back to
  the generic `basetype` in `libcall.h`

Both therefore declare the library base as a **trailing parameter**: the
difference is its declared *type*, not whether it is passed. `AROS_LC1` is
byte-identical between the two SDKs.

A `void *` versus a concrete struct pointer does not change where an argument
goes on x86-64. So this difference alone does not explain the crash, and
nothing else in the two headers accounts for it either.

## Where this leaves it

**Unknown.** Not "the ABI", which has been the tempting label from the start
and remains a name for an unfinished diagnosis.

What a next attempt should do differently: establish which library each call
goes through before comparing offsets, by resolving the base register back to
its `OpenLibrary`, rather than matching a number. And compare a call whose
identity is certain, ideally by building the *same source* against both SDKs and
diffing the output, which sidesteps identification entirely.

## What it means for arospkg, which is unchanged

The client does not need the mechanism. It needs to not offer a package that is
known not to run, and the manifests now say so explicitly through
`installs_on`, `runs_on` and `does_not_run_on`. Detecting incompatibility from
an ELF file is a different and much harder problem, and there is no reason to
solve it before someone asks for a package that nobody has tested.
