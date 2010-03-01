For Linux, we use info gathered from /proc/cpuinfo to determine the
number of processors in the system.  Unfortunately, there are quite a
few different formats of the cpuinfo file for different CPUs and
different O/S revs.  Below is a list of a bunch of them as examples.

The "UNAME:", "PROCESSORS:", "HTHREADS:" "START" and "END" lines in
this file allow for the test code to run against each chunk.  To
enable this, run:
  condor_sysapi README-Linux.txt <uname string>
i.e.
  condor_sysapi README-Linux.txt "nmi-0080"
  condor_sysapi README-Linux.txt "alpha"
Note that this code is pretty crude, and will just run with the first
UNAME string that contains the passed in uname string, so make sure
that you're getting the record you want.

Definitions:
"PROCESSORS"    The total # of processors found.
"HTHREADS"      The total # of "non-primary" hyper threads
"HTHREADS_CORE" # of hyper threads / Core

You can specify either HTHREADS or HTHREADS_CORE; HTHREADS will be
calculated from HTHREADS_CORE if HTHREADS is not specified.


--------------------------------------------------------------------------
UNAME:Linux test-4ht 2.6.9.nrl #1 Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 4
HTHREADS_CORE: 4
START

processor       : 0
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 1
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 2
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 3
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux test-quad-4ht 2.6.9.nrl #1 Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 16
HTHREADS_CORE: 4
START

processor       : 0
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 1
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 2
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 3
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 4
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 5
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 6
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 7
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 8
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 9
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 10
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 11
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 12
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 13
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 14
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 15
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
This one has 4 processors, 2 have HT, 2 don't

UNAME:Linux test-quad-2/0ht 2.6.9.nrl #1 Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 6
HTHREADS: 2
START

processor       : 0
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 1
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 2
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 1
siblings        : 2
core id         : 0
cpu cores       : 0
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 3
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 1
siblings        : 2
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 4
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 2
siblings        : 1
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

processor       : 5
vendor_id       : GenuineNick
cpu family      : 15
model           : 4
model name      : Nick QuadHt 6502 CPU 1.023MHz
physical id     : 3
siblings        : 1
core id         : 0
cpu cores       : 1
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 1

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux test-8cpu-4core-4ht 2.6.9.nrl #1 Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 128
HTHREADS_CORE: 4
START

processor       : 0
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 1
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 2
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 3
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 4
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 5
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 6
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 7
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 8
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 9
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 10
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 11
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 12
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 13
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 14
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 15
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 16
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 17
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 18
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 19
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 20
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 21
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 22
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 23
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 24
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 25
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 26
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 27
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 28
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 29
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 30
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 31
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 32
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 33
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 34
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 35
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 36
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 37
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 38
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 39
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 40
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 41
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 42
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 43
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 44
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 45
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 46
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 47
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 48
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 49
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 50
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 51
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 52
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 53
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 54
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 55
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 56
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 57
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 58
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 59
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 60
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 61
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 62
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 63
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 64
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 65
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 66
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 67
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 68
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 69
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 70
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 71
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 72
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 73
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 74
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 75
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 76
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 77
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 78
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 79
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 80
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 81
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 82
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 83
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 84
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 85
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 86
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 87
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 88
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 89
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 90
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 91
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 92
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 93
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 94
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 95
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 96
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 97
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 98
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 99
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 100
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 101
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 102
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 103
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 104
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 105
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 106
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 107
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 108
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 109
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 110
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 111
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 112
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 113
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 114
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 115
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 116
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 117
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 118
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 119
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 120
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 121
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 122
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 123
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 124
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 125
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 126
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 127
vendor_id       : GenuineNick
model name      : Nicks QuadCore 4HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 4
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux test-quad-2core-2ht 2.6.9.nrl #1 Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 64
HTHREADS_CORE: 2
START

processor       : 0
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 1
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 2
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 3
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 4
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 5
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 6
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 7
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 8
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 9
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 10
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 11
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 12
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 13
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 14
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 15
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 16
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 17
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 18
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 19
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 20
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 21
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 22
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 23
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 2
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 24
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 25
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 26
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 27
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 28
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 29
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 30
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 31
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 32
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 33
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 34
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 35
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 36
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 37
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 38
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 39
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 4
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 40
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 41
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 42
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 43
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 44
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 45
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 46
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 47
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 5
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 48
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 49
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 50
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 51
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 52
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 53
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 54
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 55
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 6
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 56
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 57
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 0
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 58
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 59
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 1
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 60
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 61
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 2
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 62
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

processor       : 63
vendor_id       : GenuineNick
model name      : Nicks QuadCore 2HT SuperHyperCpu
cpu MHz         : 10230
cache size      : 1024 KB
physical id     : 7
siblings        : 2
core id         : 3
cpu cores       : 4
flags           : fpu vme de pse tsc msr pae mce apic fxsr sse sse2 ss ht
bogomips        : 1

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux tonic.cs.wisc.edu 2.6.9-55.0.6.ELsmp #1 SMP Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 4
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 1
cpu MHz         : 2801.828
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 5
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 5605.93

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 4
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 1
cpu MHz         : 2801.828
cache size      : 1024 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 1
fdiv_bug        : no
Hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 5
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe lm constant_tsc pni monitor ds_cpl cid xtpr
bogomips        : 5599.54

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux chopin.cs.wisc.edu 2.6.9-55.0.6.ELhugemem #1 SMP Tue Sep 4 22:06:51 EDT 2007 i686 unknown
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) XEON(TM) CPU 1.80GHz
stepping        : 4
cpu MHz         : 1800.772
cache size      : 512 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm
bogomips        : 3603.13

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) XEON(TM) CPU 1.80GHz
stepping        : 4
cpu MHz         : 1800.772
cache size      : 512 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm
bogomips        : 3599.41

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
Not sure exactly what kernel this is -- I'm guessing 2.0.x or 2.2.x
UNAME:Linux xxxxx.cs.wisc.edu 2.2.x
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor       : 0
cpu             : 686
model           : 3
vendor_id       : GenuineIntel
stepping        : 4
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid           : yes
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic 11 mtrr pge mca cmov mmx
bogomips        : 298.19

processor       : 1
cpu             : 686
model           : 3
vendor_id       : GenuineIntel
stepping        : 4
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid           : yes
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic 11 mtrr pge mca cmov mmx
bogomips        : 299.01

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux nmi-0080.cs.wisc.edu 2.4.18-27.7.x.hp #1 Mon May 12 23:54:38 EDT 2003 alpha unknown
PROCESSORS: 1
HTHREADS_CORE: 1
START

cpu                     : Alpha
cpu model               : EV56
cpu variation           : 7
cpu revision            : 0
cpu serial number       :
system type             : EB164
system variation        : LX164
system revision         : 0
system serial number    :
cycle frequency [Hz]    : 533333333
timer frequency [Hz]    : 1024.00
page size [bytes]       : 8192
phys. address bits      : 40
max. addr. space #      : 127
BogoMIPS                : 1059.80
kernel unaligned acc    : 174 (pc=fffffc00011c9d48,va=fffffc000ecabf16)
user unaligned acc      : 10 (pc=120092ce8,va=20000000056)
platform string         : Digital AlphaPC 164LX 533 MHz
cpus detected           : 1

END
--------------------------------------------------------------------------


--------------------------------------------------------------------------
UNAME:Linux nmi-0020.cs.wisc.edu 2.6.9-22.0.1.EL #1 SMP Tue Oct 18 18:29:56 EDT 2005 ia64 ia64 ia64 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor  : 0
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 997.308000
itc MHz    : 997.308000
BogoMIPS   : 1493.17

processor  : 1
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 997.308000
itc MHz    : 997.308000
BogoMIPS   : 1488.97

END
--------------------------------------------------------------------------


--------------------------------------------------------------------------
UNAME:Linux nmi-0058.cs.wisc.edu 2.4.21-47.EL #1 SMP Wed Jul 5 20:30:48 EDT 2006 ia64 ia64 ia64 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor  : 0
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 997.364000
itc MHz    : 997.364000
BogoMIPS   : 1493.17
siblings   : 1

processor  : 1
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 997.364000
itc MHz    : 997.364000
BogoMIPS   : 1488.97
siblings   : 1

END
--------------------------------------------------------------------------


--------------------------------------------------------------------------
UNAME:Linux nmi-0065 2.6.5-7.97-default #1 SMP Fri Jul 2 14:21:59 UTC 2004 ia64 ia64 ia64 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor  : 0
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1396.318994
itc MHz    : 1396.318994
BogoMIPS   : 2088.76

processor  : 1
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1396.318994
itc MHz    : 1396.318994
BogoMIPS   : 2088.76

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux nmi-build33 2.4.19-SMP #1 SMP Mon Jan 27 18:35:48 UTC 2003 ia64 unknown
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor  : 0
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1396.316999
itc MHz    : 1396.316999
BogoMIPS   : 2088.76

processor  : 1
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 1
revision   : 5
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1396.316999
itc MHz    : 1396.316999
BogoMIPS   : 1606.41

END
--------------------------------------------------------------------------


--------------------------------------------------------------------------
UNAME:Linux nmi-ia64-1.cs.wisc.edu 2.4.21-32.0.1.EL.cern #1 SMP Thu May 26 11:51:54 CEST 2005 ia64 ia64 ia64 GNU/Linux
PROCESSORS: 1
HTHREADS_CORE: 1
START

processor  : 0
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 0
revision   : 7
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 900.000000
itc MHz    : 900.000000
BogoMIPS   : 535.82

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux nmi-0013 2.6.18-4-686 #1 SMP Mon Mar 26 17:17:36 UTC 2007 i686 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 2
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.66GHz
stepping        : 5
cpu MHz         : 2657.851
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5320.08

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.66GHz
stepping        : 9
cpu MHz         : 2657.851
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5316.19

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.66GHz
stepping        : 9
cpu MHz         : 2657.851
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5316.11

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.66GHz
stepping        : 5
cpu MHz         : 2657.851
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5316.05

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux nmi-0042.cs.wisc.edu 2.4.22-2f #1 Sun Nov 9 16:49:49 EST 2003 ppc ppc ppc GNU/Linux
PROCESSORS: 1
HTHREADS_CORE: 1
START

cpu             : 7400, altivec supported
temperature     : 30-32 C (uncalibrated)
clock           : 400MHz
revision        : 2.9 (pvr 000c 0209)
bogomips        : 796.26
machine         : PowerMac3,1
motherboard     : PowerMac3,1 MacRISC Power Macintosh
board revision  : 00000001
detected as     : 65 (PowerMac G4 AGP Graphics)
pmac flags      : 00000004
L2 cache        : 1024K unified
memory          : 640MB
pmac-generation : NewWorld

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux nmi-0154 2.6.5-7.97-pseries64 #1 SMP Fri Jul 2 14:21:59 UTC 2004 ppc64 ppc64 ppc64 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor       : 0
cpu             : POWER5 (gs)
clock           : 2096.901000MHz
revision        : 3.1

processor       : 1
cpu             : POWER5 (gs)
clock           : 2096.901000MHz
revision        : 3.1

processor       : 2
cpu             : POWER5 (gs)
clock           : 2096.901000MHz
revision        : 3.1

processor       : 3
cpu             : POWER5 (gs)
clock           : 2096.901000MHz
revision        : 3.1

timebase        : 512053000
machine         : CHRP IBM,9115-505

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux f05.cs.wisc.edu 2.6.9-55.0.6.ELhugemem #1 SMP Tue Sep 4 22:06:51 EDT 2007 i686 unknown
PROCESSORS: 4
HTHREADS_CORE: 2
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2791.390
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips        : 5585.81

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2791.390
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips        : 5581.29

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2791.390
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips        : 5581.24

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2791.390
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips        : 5581.41

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux c2-005.cs.wisc.edu 2.6.9-55.0.6.ELsmp #1 SMP Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 8
model name      : Pentium III (Coppermine)
stepping        : 6
cpu MHz         : 935.969
cache size      : 256 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 mmx fxsr sse
bogomips        : 1872.65

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 6
model           : 8
model name      : Pentium III (Coppermine)
stepping        : 6
cpu MHz         : 935.969
cache size      : 256 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 mmx fxsr sse
bogomips        : 1870.69

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux p05.cs.wisc.edu 2.6.9-55.0.6.ELsmp #1 SMP Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 1
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Pentium(R) 4 CPU 2.00GHz
stepping        : 4
cpu MHz         : 1993.701
cache size      : 512 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm
bogomips        : 3990.46

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux bio.cs.wisc.edu 2.6.9-55.0.6.ELsmp #1 SMP Tue Sep 4 21:36:00 EDT 2007 i686 unknown
PROCESSORS: 4
HTHREADS_CORE: 2
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 9
cpu MHz         : 2800.481
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5604.06

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 9
cpu MHz         : 2800.481
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5599.42

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 9
cpu MHz         : 2800.481
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5599.59

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 9
cpu MHz         : 2800.481
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5599.72

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux g12n01.hep.wisc.edu 2.6.9-42.0.3.ELsmp #1 SMP Thu Oct 5 16:29:37 CDT 2006 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 8
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5339.30
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 4
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5332.83
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5332.77
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 5
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5332.84
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 4
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5332.80
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 5
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 6
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5332.81
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 6
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5332.81
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 7
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           X5355  @ 2.66GHz
stepping        : 7
cpu MHz         : 2666.806
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 7
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5332.83
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------

UNAME:Linux g9n01.hep.wisc.edu 2.6.9-42.0.3.ELsmp #1 SMP Thu Oct 5 16:29:37 CDT 2006 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 265
stepping        : 2
cpu MHz         : 1794.904
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3595.06
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 1
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 265
stepping        : 2
cpu MHz         : 1794.904
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 1
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3589.27
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 2
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 265
stepping        : 2
cpu MHz         : 1794.904
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 0
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3589.27
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 3
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 265
stepping        : 2
cpu MHz         : 1794.904
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 1
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3589.28
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------

UNAME:Linux g3n01.hep.wisc.edu 2.6.9-42.0.3.ELsmp #1 SMP Thu Oct 5 15:04:03 CDT 2006 i686 i686 i386 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.40GHz
stepping        : 7
cpu MHz         : 2396.912
cache size      : 512 KB
physical id     : 0
siblings        : 1
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips        : 4795.61

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.40GHz
stepping        : 7
cpu MHz         : 2396.912
cache size      : 512 KB
physical id     : 3
siblings        : 1
core id         : 3
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips        : 4791.26

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------

UNAME:Linux clover-01.cs.wisc.edu 2.6.9-55.ELsmp #1 SMP Wed May 2 14:04:42 EDT 2007 x86_64 unknown
PROCESSORS: 8 
HTHREADS_CORE: 1
START


processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4659.98
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 4
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4654.40
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4654.33
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4654.38
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 4
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4654.38
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 5
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 5
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4654.41
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 6
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 6
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4654.40
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 7
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU           E5345  @ 2.33GHz
stepping        : 7
cpu MHz         : 2327.546
cache size      : 4096 KB
physical id     : 1
siblings        : 4
core id         : 7
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4654.40
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------

UNAME:Linux darwin01.stat.wisc.edu 2.6.9-55.ELsmp #1 SMP Wed May 2 14:04:42 EDT 2007 x86_64 unknown
PROCESSORS: 2
HTHREADS_CORE: 1
START


processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Core(TM)2 CPU          6600  @ 2.40GHz
stepping        : 6
cpu MHz         : 2394.072
cache size      : 4096 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4792.50
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Core(TM)2 CPU          6600  @ 2.40GHz
stepping        : 6
cpu MHz         : 2394.072
cache size      : 4096 KB
physical id     : 0
siblings        : 2
core id         : 1
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 4787.32
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:



END
--------------------------------------------------------------------------

--------------------------------------------------------------------------

UNAME:Linux diabetes01.stat.wisc.edu 2.6.9-55.ELsmp #1 SMP Wed May 2 14:04:42 EDT 2007 x86_64 unknown
PROCESSORS: 4
HTHREADS_CORE: 1
START


processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Core(TM)2 Quad CPU           @ 2.66GHz
stepping        : 7
cpu MHz         : 2660.059
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5324.90
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Core(TM)2 Quad CPU           @ 2.66GHz
stepping        : 7
cpu MHz         : 2660.059
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5319.26
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Core(TM)2 Quad CPU           @ 2.66GHz
stepping        : 7
cpu MHz         : 2660.059
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5319.27
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Core(TM)2 Quad CPU           @ 2.66GHz
stepping        : 7
cpu MHz         : 2660.059
cache size      : 4096 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips        : 5319.26
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------

UNAME:Linux s0-01.cs.wisc.edu 2.6.9-55.ELsmp #1 SMP Wed May 2 14:04:42 EDT 2007 x86_64 unknown
PROCESSORS: 2
HTHREADS_CORE: 1
START


processor       : 0
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 5
model name      : AMD Opteron(tm) Processor 248
stepping        : 8
cpu MHz         : 2191.500
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4389.10
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts ttp

processor       : 1
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 5
model name      : AMD Opteron(tm) Processor 248
stepping        : 8
cpu MHz         : 2191.500
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4382.28
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts ttp

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
http://lists.debian.org/debian-user/2004/03/msg03558.html

UNAME:Linux Soleil 2.4.25 #3 SMP mar mar 2 15:49:56 CET 2004 i686 unknown
PROCESSORS:
HTHREADS_CORE:
START

processor     : 0
vendor_id     : GenuineIntel
cpu family    : 15
model         : 2
model name    : Intel(R) Xeon(TM) CPU 2.80GHz
stepping      : 9
cpu MHz       : 2783.659
cache size    : 512 KB
fdiv_bug      : no
hlt_bug       : no
f00f_bug      : no
coma_bug      : no
fpu           : yes
fpu_exception : yes
cpuid level   : 2
wp            : yes
flags         : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips      : 5557.45

processor     : 1
vendor_id     : GenuineIntel
cpu family    : 15
model         : 2
model name    : Intel(R) Xeon(TM) CPU 2.80GHz
stepping      : 9
cpu MHz       : 2783.659
cache size    : 512 KB
fdiv_bug      : no
hlt_bug       : no
f00f_bug      : no
coma_bug      : no
fpu           : yes
fpu_exception : yes
cpuid level   : 2
wp            : yes
flags         : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips      : 5557.45

processor     : 2
vendor_id     : GenuineIntel
cpu family    : 15
model         : 2
model name    : Intel(R) Xeon(TM) CPU 2.80GHz
stepping      : 9
cpu MHz       : 2783.659
cache size    : 512 KB
fdiv_bug      : no
hlt_bug       : no
f00f_bug      : no
coma_bug      : no
fpu           : yes
fpu_exception : yes
cpuid level   : 2
wp            : yes
flags         : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips      : 5557.45

processor     : 3
vendor_id     : GenuineIntel
cpu family    : 15
model         : 2
model name    : Intel(R) Xeon(TM) CPU 2.80GHz
stepping      : 9
cpu MHz       : 2783.659
cache size    : 512 KB
fdiv_bug      : no
hlt_bug       : no
f00f_bug      : no
coma_bug      : no
fpu           : yes
fpu_exception : yes
cpuid level   : 2
wp            : yes
flags         : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid
bogomips      : 5557.45

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
According to the URL, this machine has 2 processors, and hyper threading,
but I have no idea how to detect that from the /proc/cpuinfo given
http://lists.us.dell.com/pipermail/linux-poweredge/2004-January/012184.html

UNAME:Linux random1 2.4.x #3 SMP mar mar 2 15:49:56 CET 2004 i686 unknown
#PROCESSORS: 4
#HTHREADS_CORE: 2
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.40GHz
stepping        : 9
cpu MHz         : 2392.324
cache size      : 512 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm
bogomips        : 4771.02

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.40GHz
stepping        : 9
cpu MHz         : 2392.324
cache size      : 512 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm
bogomips        : 4784.12

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.40GHz
stepping        : 9
cpu MHz         : 2392.324
cache size      : 512 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm
bogomips        : 4784.12

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.40GHz
stepping        : 9
cpu MHz         : 2392.324
cache size      : 512 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm
bogomips        : 4784.12

END
--------------------------------------------------------------------------


--------------------------------------------------------------------------
UNAME:Linux valletta--escience 2.6.18-5-amd64 #1 SMP Wed Sep 26 04:51:25 UTC 2007 x86_64 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU            5150  @ 2.66GHz
stepping        : 6
cpu MHz         : 2660.033
cache size      : 4096 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm constant_tsc pni monitor ds_cpl vmx est tm2 cx16 xtpr lahf_lm
bogomips        : 5324.70
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU            5150  @ 2.66GHz
stepping        : 6
cpu MHz         : 2660.033
cache size      : 4096 KB
physical id     : 3
siblings        : 2
core id         : 0
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm constant_tsc pni monitor ds_cpl vmx est tm2 cx16 xtpr lahf_lm
bogomips        : 5319.98
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU            5150  @ 2.66GHz
stepping        : 6
cpu MHz         : 2660.033
cache size      : 4096 KB
physical id     : 0
siblings        : 2
core id         : 1
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm constant_tsc pni monitor ds_cpl vmx est tm2 cx16 xtpr lahf_lm
bogomips        : 5319.96
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 6
model           : 15
model name      : Intel(R) Xeon(R) CPU            5150  @ 2.66GHz
stepping        : 6
cpu MHz         : 2660.033
cache size      : 4096 KB
physical id     : 3
siblings        : 2
core id         : 1
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 10
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm constant_tsc pni monitor ds_cpl vmx est tm2 cx16 xtpr lahf_lm
bogomips        : 5319.96
clflush size    : 64
cache_alignment : 64
address sizes   : 36 bits physical, 48 bits virtual
power management:

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux sapphire 2.6.5-7.286-rtgfx #1 SMP Sat Jun 30 04:51:41 PDT 2007 ia64 ia64 ia64 GNU/Linux
PROCESSORS: 20
HTHREADS_CORE: 1
START


processor  : 0
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2214.59
siblings   : 1

processor  : 1
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 2
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 3
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 4
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 5
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 6
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 7
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 8
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 9
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 10
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 11
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 12
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 13
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 14
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 15
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 2
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 16
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 1
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 17
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 1
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 18
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 1
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

processor  : 19
vendor     : GenuineIntel
arch       : IA-64
family     : Itanium 2
model      : 2
revision   : 1
archrev    : 0
features   : branchlong
cpu number : 0
cpu regs   : 4
cpu MHz    : 1500.000000
itc MHz    : 1500.000000
BogoMIPS   : 2239.75
siblings   : 1

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux stones 2.6.18-8.1.8.el5 #1 SMP Mon Jun 25 17:06:07 EDT 2007 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 2210
stepping        : 2
cpu MHz         : 1000.000
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 1999.74
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 1
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 2210
stepping        : 2
cpu MHz         : 1000.000
cache size      : 1024 KB
physical id     : 0
siblings        : 2
core id         : 1
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 1999.74
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 2
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 2210
stepping        : 2
cpu MHz         : 1000.000
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 0
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 1999.74
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 3
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 2210
stepping        : 2
cpu MHz         : 1000.000
cache size      : 1024 KB
physical id     : 1
siblings        : 2
core id         : 1
cpu cores       : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 1999.74
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
Power management: ts fid vid ttp tm stc

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux beatles 2.6.9-22.0.2 #1 SMP Wed Feb 8 11:21:05 EST 2006 i686 athlon i386 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 270
stepping        : 2
cpu MHz         : 1994.947
cache size      : 1024 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3858.43

processor       : 1
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 270
stepping        : 2
cpu MHz         : 1994.947
cache size      : 1024 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
pfpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3981.31

processor       : 2
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 270
stepping        : 2
cpu MHz         : 1994.947
cache size      : 1024 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3981.31

processor       : 3
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : Dual Core AMD Opteron(tm) Processor 270
stepping        : 2
cpu MHz         : 1994.947
cache size      : 1024 KB
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3981.31

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux  unknown-dual-xeon 2.6.9-22.0.2 #1 SMP Wed Feb 8 11:21:05 EST 2006 i686 athlon i386 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 2
START

processor : 0
vendor_id : GenuineIntel
cpu family : 15
model : 2
model name : Intel(R) Xeon(TM) CPU 2.40GHz
stepping : 9
cpu MHz : 2400.627
cache size : 512 KB
physical id : 0
siblings : 2
fdiv_bug : no
hlt_bug : no
f00f_bug : no
coma_bug : no
fpu : yes
fpu_exception : yes
cpuid level : 2
wp : yes
flags : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips : 4734.97

processor : 1
vendor_id : GenuineIntel
cpu family : 15
model : 2
model name : Intel(R) Xeon(TM) CPU 2.40GHz
stepping : 9
cpu MHz : 2400.627
cache size : 512 KB
physical id : 0
siblings : 2
fdiv_bug : no
hlt_bug : no
f00f_bug : no
coma_bug : no
fpu : yes
fpu_exception : yes
cpuid level : 2
wp : yes
flags : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips : 4784.12

processor : 2
vendor_id : GenuineIntel
cpu family : 15
model : 2
model name : Intel(R) Xeon(TM) CPU 2.40GHz
stepping : 9
cpu MHz : 2400.627
cache size : 512 KB
physical id : 3
siblings : 2
fdiv_bug : no
hlt_bug : no
f00f_bug : no
coma_bug : no
fpu : yes
fpu_exception : yes
cpuid level : 2
wp : yes
flags : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips : 4784.12

processor : 3
vendor_id : GenuineIntel
cpu family : 15
model : 2
model name : Intel(R) Xeon(TM) CPU 2.40GHz
stepping : 9
cpu MHz : 2400.627
cache size : 512 KB
physical id : 3
siblings : 2
fdiv_bug : no
hlt_bug : no
f00f_bug : no
coma_bug : no
fpu : yes
fpu_exception : yes
cpuid level : 2
wp : yes
flags : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips : 4784.12

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux Linux cox 2.6.18-8.el5xen #1 SMP Fri Jan 26 14:29:35 EST 2007 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 8
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 0
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 1
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 1
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 2
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 2
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 3
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 3
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 4
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 4
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 5
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 5
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 6
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 6
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

processor       : 7
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 65
model name      : Dual-Core AMD Opteron(tm) Processor 8218 HE
stepping        : 3
cpu MHz         : 2613.432
cache size      : 1024 KB
physical id     : 7
siblings        : 1
core id         : 0
cpu cores       : 1
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu tsc msr pae mce cx8 apic mtrr mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt rdtscp lm 3dnowext 3dnow pni cx16 lahf_lm cmp_legacy svm cr8_legacy
bogomips        : 6535.21
TLB size        : 1024 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp tm stc

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux ttc-bs2200-020 2.6.9-42.0.0.0.1.ELsmp #1 SMP Sun Oct 15 15:13:57 PDT 2006 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 275
stepping	: 2
cpu MHz		: 2193.789
cache size	: 1024 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips	: 4393.84
TLB size	: 1088 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 1
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 275
stepping	: 2
cpu MHz		: 2193.789
cache size	: 1024 KB
physical id	: 0
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips	: 4386.88
TLB size	: 1088 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 2
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 275
stepping	: 2
cpu MHz		: 2193.789
cache size	: 1024 KB
physical id	: 1
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips	: 4386.90
TLB size	: 1088 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 3
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 275
stepping	: 2
cpu MHz		: 2193.789
cache size	: 1024 KB
physical id	: 1
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow pni
bogomips	: 4386.90
TLB size	: 1088 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

END
--------------------------------------------------------------------------


--------------------------------------------------------------------------
UNAME:Linux TTC-BS3000-020 2.6.9-42.0.0.0.1.ELsmp #1 SMP Sun Oct 15 15:13:57 PDT 2006 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5160  @ 3.00GHz
stepping	: 6
cpu MHz		: 2992.505
cache size	: 4096 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips	: 5989.03
clflush size	: 64
cache_alignment	: 64
address sizes	: 36 bits physical, 48 bits virtual
power management:

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5160  @ 3.00GHz
stepping	: 6
cpu MHz		: 2992.505
cache size	: 4096 KB
physical id	: 3
siblings	: 2
core id		: 6
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips	: 5985.04
clflush size	: 64
cache_alignment	: 64
address sizes	: 36 bits physical, 48 bits virtual
power management:

processor	: 2
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5160  @ 3.00GHz
stepping	: 6
cpu MHz		: 2992.505
cache size	: 4096 KB
physical id	: 0
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips	: 5984.98
clflush size	: 64
cache_alignment	: 64
address sizes	: 36 bits physical, 48 bits virtual
power management:

processor	: 3
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5160  @ 3.00GHz
stepping	: 6
cpu MHz		: 2992.505
cache size	: 4096 KB
physical id	: 3
siblings	: 2
core id		: 7
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr
bogomips	: 5985.04
clflush size	: 64
cache_alignment	: 64
address sizes	: 36 bits physical, 48 bits virtual
power management:

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux impala 2.4.21-50.ELsmp #1 SMP Tue May 8 17:10:20 EDT 2007 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 8
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4377.80
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 1
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 1
siblings        : 2
core id         : 0
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4390.91
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 2
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 2
siblings        : 2
core id         : 0
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4390.91
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 3
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 3
siblings        : 2
core id         : 0
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4404.01
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 4
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 0
siblings        : 2
core id         : 1
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4390.91
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 5
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 1
siblings        : 2
core id         : 1
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4390.91
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 6
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 2
siblings        : 2
core id         : 1
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4390.91
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 7
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 33
model name      : AMD Opteron (tm) Processor 850
physical id     : 3
siblings        : 2
core id         : 1
cpu cores       : 2
stepping        : 0
cpu MHz         : 2197.147
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext lm 3dnowext 3dnow
bogomips        : 4390.91
TLB size        : 1088 4K pages
clflush size    : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

END
--------------------------------------------------------------------------


--------------------------------------------------------------------------
UNAME:Linux imp28.agresearch.co.nz 2.6.9-55.0.2.ELsmp #1 SMP Tue Jun 26 14:30:58 EDT 2007 i686 i686 i386 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 2
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2799.337
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5602.67

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2799.337
cache size      : 512 KB
physical id     : 0
siblings        : 2
core id         : 0
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5597.89

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2799.337
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5597.72

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 15
model           : 2
model name      : Intel(R) Xeon(TM) CPU 2.80GHz
stepping        : 7
cpu MHz         : 2799.337
cache size      : 512 KB
physical id     : 3
siblings        : 2
core id         : 3
cpu cores       : 1
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
coma_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid level     : 2
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips        : 5597.78

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux imp69.agresearch.co.nz 2.6.9-55.0.2.ELsmp #1 SMP Tue Jun 26 14:14:47 EDT 2007 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor       : 0
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 37
model name      : AMD Opteron(tm) Processor 252
stepping        : 1
cpu MHz         : 1804.125
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3613.25
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor       : 1
vendor_id       : AuthenticAMD
cpu family      : 15
model           : 37
model name      : AMD Opteron(tm) Processor 252
stepping        : 1
cpu MHz         : 1804.125
cache size      : 1024 KB
fpu             : yes
fpu_exception   : yes
cpuid level     : 1
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx mmxext lm 3dnowext 3dnow pni
bogomips        : 3613.25
TLB size        : 1088 4K pages
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type1 2.6.19.3-generic #1 SMP Thu Feb 15 13:31:20 CET 2007 i686 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Core(TM)2 CPU          6300  @ 1.86GHz
stepping	: 6
cpu MHz		: 1866.733
cache size	: 2048 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr lahf_lm
bogomips	: 3735.60

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Core(TM)2 CPU          6300  @ 1.86GHz
stepping	: 6
cpu MHz		: 1866.733
cache size	: 2048 KB
physical id	: 0
siblings	: 2
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr lahf_lm
bogomips	: 3733.39


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type2 2.6.19.3-generic #1 SMP Thu Feb 15 13:31:20 CET 2007 i686 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz
stepping	: 11
cpu MHz		: 2400.098
cache size	: 4096 KB
physical id	: 0
siblings	: 4
core id		: 0
cpu cores	: 4
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr lahf_lm
bogomips	: 4802.67

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz
stepping	: 11
cpu MHz		: 2400.098
cache size	: 4096 KB
physical id	: 0
siblings	: 4
core id		: 1
cpu cores	: 4
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr lahf_lm
bogomips	: 4800.04

processor	: 2
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz
stepping	: 11
cpu MHz		: 2400.098
cache size	: 4096 KB
physical id	: 0
siblings	: 4
core id		: 2
cpu cores	: 4
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr lahf_lm
bogomips	: 4800.13

processor	: 3
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz
stepping	: 11
cpu MHz		: 2400.098
cache size	: 4096 KB
physical id	: 0
siblings	: 4
core id		: 3
cpu cores	: 4
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr lahf_lm
bogomips	: 4800.14


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type3 2.6.19.3-generic #1 SMP Thu Feb 15 13:31:20 CET 2007 i686 GNU/Linux
PROCESSORS: 4
HTHREADS_CORE: 2
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 15
model		: 2
model name	: Intel(R) Xeon(TM) CPU 2.40GHz
stepping	: 5
cpu MHz		: 2400.209
cache size	: 512 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 1
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 2
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips	: 4803.58

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 15
model		: 2
model name	: Intel(R) Xeon(TM) CPU 2.40GHz
stepping	: 5
cpu MHz		: 2400.209
cache size	: 512 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 1
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 2
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips	: 4800.36

processor	: 2
vendor_id	: GenuineIntel
cpu family	: 15
model		: 2
model name	: Intel(R) Xeon(TM) CPU 2.40GHz
stepping	: 5
cpu MHz		: 2400.209
cache size	: 512 KB
physical id	: 3
siblings	: 2
core id		: 0
cpu cores	: 1
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 2
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips	: 4800.37

processor	: 3
vendor_id	: GenuineIntel
cpu family	: 15
model		: 2
model name	: Intel(R) Xeon(TM) CPU 2.40GHz
stepping	: 5
cpu MHz		: 2400.209
cache size	: 512 KB
physical id	: 3
siblings	: 2
core id		: 0
cpu cores	: 1
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 2
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
bogomips	: 4800.34


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type4 2.6.14-generic #1 SMP Tue Nov 15 10:24:06 CET 2005 i686 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 2
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 15
model		: 4
model name	: Intel(R) Pentium(R) 4 CPU 3.40GHz
stepping	: 3
cpu MHz		: 3400.712
cache size	: 2048 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 1
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 5
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm pni monitor ds_cpl est cid cx16 xtpr
bogomips	: 6810.81

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 15
model		: 4
model name	: Intel(R) Pentium(R) 4 CPU 3.40GHz
stepping	: 3
cpu MHz		: 3400.712
cache size	: 2048 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 1
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 5
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm pni monitor ds_cpl est cid cx16 xtpr
bogomips	: 6800.83


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type5 2.6.19.3-generic #1 SMP Thu Feb 15 13:31:20 CET 2007 i686 GNU/Linux
PROCESSORS: 4 
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5140  @ 2.33GHz
stepping	: 6
cpu MHz		: 2327.564
cache size	: 4096 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr dca lahf_lm
bogomips	: 4657.82

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5140  @ 2.33GHz
stepping	: 6
cpu MHz		: 2327.564
cache size	: 4096 KB
physical id	: 0
siblings	: 2
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr dca lahf_lm
bogomips	: 4655.12

processor	: 2
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5140  @ 2.33GHz
stepping	: 6
cpu MHz		: 2327.564
cache size	: 4096 KB
physical id	: 3
siblings	: 2
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr dca lahf_lm
bogomips	: 4655.16

processor	: 3
vendor_id	: GenuineIntel
cpu family	: 6
model		: 15
model name	: Intel(R) Xeon(R) CPU            5140  @ 2.33GHz
stepping	: 6
cpu MHz		: 2327.564
cache size	: 4096 KB
physical id	: 3
siblings	: 2
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr dca lahf_lm
bogomips	: 4655.17


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type6 2.6.19.3-generic #1 SMP Thu Feb 15 13:31:20 CET 2007 i686 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 5
model name	: AMD Opteron(tm) Processor 244
stepping	: 10
cpu MHz		: 1804.195
cache size	: 1024 KB
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx mmxext lm 3dnowext 3dnow ts fid vid ttp
bogomips	: 3611.64

processor	: 1
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 5
model name	: AMD Opteron(tm) Processor 244
stepping	: 10
cpu MHz		: 1804.195
cache size	: 1024 KB
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx mmxext lm 3dnowext 3dnow ts fid vid ttp
bogomips	: 3608.37


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type7 2.6.19.3-generic #1 SMP Thu Feb 15 13:31:20 CET 2007 i686 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START


processor	: 0
vendor_id	: AuthenticAMD
cpu family	: 6
model		: 8
model name	: AMD Athlon(tm) MP 2400+
stepping	: 1
cpu MHz		: 2000.161
cache size	: 256 KB
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 mmx fxsr sse syscall mp mmxext 3dnowext 3dnow ts
bogomips	: 4003.41

processor	: 1
vendor_id	: AuthenticAMD
cpu family	: 6
model		: 8
model name	: AMD Athlon(tm) MP
stepping	: 1
cpu MHz		: 2000.161
cache size	: 256 KB
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 mmx fxsr sse syscall mp mmxext 3dnowext 3dnow ts
bogomips	: 4000.33


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux type8 2.6.19.3-generic #1 SMP Thu Feb 15 13:31:20 CET 2007 i686 GNU/Linux
PROCESSORS: 2
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 35
model name	: AMD Athlon(tm) 64 X2 Dual Core Processor 4400+
stepping	: 2
cpu MHz		: 2210.811
cache size	: 1024 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy ts fid vid ttp
bogomips	: 4423.59

processor	: 1
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 35
model name	: AMD Athlon(tm) 64 X2 Dual Core Processor 4400+
stepping	: 2
cpu MHz		: 2210.811
cache size	: 1024 KB
physical id	: 0
siblings	: 2
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy ts fid vid ttp
bogomips	: 4421.62

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux ee-lightning 2.6.16.53-0.8-smp #1 SMP Fri Aug 31 13:07:27 UTC 2007 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 16
HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 0
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 1
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 0
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 2
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 1
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 3
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 1
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 4
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 2
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 5
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 2
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 6
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 3
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 7
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 3
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 8
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 4
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 9
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 4
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 10
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 5
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 11
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 5
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 12
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 6
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 13
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 6
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 14
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 7
siblings	: 2
core id		: 0
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp

processor	: 15
vendor_id	: AuthenticAMD
cpu family	: 15
model		: 33
model name	: Dual Core AMD Opteron(tm) Processor 865
stepping	: 2
cpu MHz		: 1000.000
cache size	: 1024 KB
physical id	: 7
siblings	: 2
core id		: 1
cpu cores	: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 1
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt lm 3dnowext 3dnow pni lahf_lm cmp_legacy
bogomips	: 2012.60
TLB size	: 1024 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: ts fid vid ttp


END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux laviathon 2.6.16.53-0.8-smp #1 SMP Fri Aug 31 13:07:27 UTC 2007 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 8
HTHREADS: 
HTHREADS_CORE: 2
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 0
siblings	: 4
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6389.20

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 0
siblings	: 4
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6383.80

processor	: 2
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 0
siblings	: 4
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6383.92

processor	: 3
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 0
siblings	: 4
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6383.91

processor	: 4
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 1
siblings	: 4
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6383.92

processor	: 5
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 1
siblings	: 4
core id		: 0
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6383.93

processor	: 6
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 1
siblings	: 4
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6383.95

processor	: 7
vendor_id	: GenuineIntel
cpu family	: 15
model		: 6
model name	: Intel(R) Xeon(TM) CPU 3.20GHz
stepping	: 4
cpu MHz		: 3192.115
cache size	: 2048 KB
physical id	: 1
siblings	: 4
core id		: 1
cpu cores	: 2
fdiv_bug	: no
hlt_bug		: no
f00f_bug	: no
coma_bug	: no
fpu		: yes
fpu_exception	: yes
cpuid level	: 6
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe nx lm constant_tsc
pni monitor ds_cpl vmx cid cx16 xtpr lahf_lm
bogomips	: 6383.95

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
# CHTC node
UNAME:Linux c005.chtc.wisc.edu 2.6.9-67.0.4.ELsmp #1 SMP Thu Jan 31 16:00:56 CST 2008 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 8
HTHREADS: 0
#HTHREADS_CORE: 1
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 0
siblings	: 4
core id		: 0
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5658.28
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 1
siblings	: 4
core id		: 4
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5651.75
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

processor	: 2
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 0
siblings	: 4
core id		: 1
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5651.68
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

processor	: 3
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 0
siblings	: 4
core id		: 2
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5651.74
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

processor	: 4
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 0
siblings	: 4
core id		: 3
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5651.72
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

processor	: 5
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 1
siblings	: 4
core id		: 5
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5651.73
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

processor	: 6
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 1
siblings	: 4
core id		: 6
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5651.75
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

processor	: 7
vendor_id	: GenuineIntel
cpu family	: 6
model		: 7
model name	: Intel(R) Xeon(R) CPU           E5440  @ 2.83GHz
stepping	: 6
cpu MHz		: 2826.306
cache size	: 6144 KB
physical id	: 1
siblings	: 4
core id		: 7
cpu cores	: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 10
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx lm pni monitor ds_cpl est tm2 cx16 xtpr lahf_lm
bogomips	: 5651.77
clflush size	: 64
cache_alignment	: 64
address sizes	: 38 bits physical, 48 bits virtual
power management:

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux aplay2.usatlas.bnl.gov 2.6.18-164.6.1.el5 #1 SMP Tue Nov 3 23:02:51 EST 2009 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 16
# HTHREADS: 8
HTHREADS_CORE: 2
START

processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 0
cpu cores	: 4
apicid		: 16
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.16
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 0
cpu cores	: 4
apicid		: 0
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.02
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 2
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 1
cpu cores	: 4
apicid		: 18
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.05
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 3
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 1
cpu cores	: 4
apicid		: 2
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.02
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 4
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 2
cpu cores	: 4
apicid		: 20
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.03
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 5
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 2
cpu cores	: 4
apicid		: 4
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5319.89
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 6
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 3
cpu cores	: 4
apicid		: 22
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.03
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 7
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 3
cpu cores	: 4
apicid		: 6
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.03
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 8
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 0
cpu cores	: 4
apicid		: 17
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.02
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 9
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 0
cpu cores	: 4
apicid		: 1
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5319.96
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 10
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 1
cpu cores	: 4
apicid		: 19
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.02
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 11
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 1
cpu cores	: 4
apicid		: 3
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.02
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 12
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 2
cpu cores	: 4
apicid		: 21
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5319.96
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 13
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 2
cpu cores	: 4
apicid		: 5
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.03
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 14
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 1
siblings	: 8
core id		: 3
cpu cores	: 4
apicid		: 23
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.03
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

processor	: 15
vendor_id	: GenuineIntel
cpu family	: 6
model		: 26
model name	: Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping	: 5
cpu MHz		: 2660.084
cache size	: 8192 KB
physical id	: 0
siblings	: 8
core id		: 3
cpu cores	: 4
apicid		: 7
fpu		: yes
fpu_exception	: yes
cpuid level	: 11
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips	: 5320.03
clflush size	: 64
cache_alignment	: 64
address sizes	: 40 bits physical, 48 bits virtual
power management: [8]

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:Linux aplay2.usatlas.bnl.gov 2.6.18-164.6.1.el5 #1 SMP Tue Nov 3 23:02:51 EST 2009 x86_64 x86_64 x86_64 GNU/Linux
PROCESSORS: 8
HTHREADS: 0
HTHREADS_CORE: 0
START

processor       : 0
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 1
siblings        : 4
core id         : 0
cpu cores       : 4
apicid          : 16
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.14
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

processor       : 1
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 0
siblings        : 4
core id         : 0
cpu cores       : 4
apicid          : 0
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.03
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

processor       : 2
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 1
siblings        : 4
core id         : 1
cpu cores       : 4
apicid          : 18
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.02
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

processor       : 3
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 0
siblings        : 4
core id         : 1
cpu cores       : 4
apicid          : 2
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.03
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

processor       : 4
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 1
siblings        : 4
core id         : 2
cpu cores       : 4
apicid          : 20
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.02
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

processor       : 5
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 0
siblings        : 4
core id         : 2
cpu cores       : 4
apicid          : 4
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.04
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

processor       : 6
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 1
siblings        : 4
core id         : 3
cpu cores       : 4
apicid          : 22
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.02
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

processor       : 7
vendor_id       : GenuineIntel
cpu family      : 6
model           : 26
model name      : Intel(R) Xeon(R) CPU           X5550  @ 2.67GHz
stepping        : 5
cpu MHz         : 2660.072
cache size      : 8192 KB
physical id     : 0
siblings        : 4
core id         : 3
cpu cores       : 4
apicid          : 6
fpu             : yes
fpu_exception   : yes
cpuid level     : 11
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm syscall nx rdtscp lm constant_tsc nonstop_tsc pni monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr sse4_1 sse4_2 popcnt lahf_lm
bogomips        : 5320.04
clflush size    : 64
cache_alignment : 64
address sizes   : 40 bits physical, 48 bits virtual
power management: [8]

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------

--------------------------------------------------------------------------
UNAME:
PROCESSORS: 
HTHREADS: 
HTHREADS_CORE: 
START

END
--------------------------------------------------------------------------
