CS 61 Section 1: C-Sizer
========================

This section material can be completed in section in flipped-classroom style,
or on your own.

[https://cs61.seas.harvard.edu/wiki/2017/Section1](Instructions)


Skills
------
* Sizes of fundamental C types
* C layout rules
* C alignment
* Simple string parsing
* `sizeof(T)`, `__alignof__(T)`
* Pointer arithmetic


Part 1: c-sizer
---------------

Implement the `spec_size` function, which has the following specification:

> `size_t spec_size(const char* spec)` returns the size of a C struct
> specified by the type specification `spec`, as compiled on an x86-64
> machine. A type specification is built from these characters:
>
> | Char | Description                           |
> |------|---------------------------------------|
> | `c`  | char (or signed char, unsigned char)  |
> | `s`  | short (or unsigned short)             |
> | `i`  | int (or unsigned int)                 |
> | `l`  | long (or unsigned long)               |
> | `z`  | size_t                                |
> | `f`  | float                                 |
> | `d`  | double                                |
> | `p`  | a pointer type                        |

A specification with more than one character describes the C struct with those
components. For instance, this struct:
```c
struct x {
    char a;
    char b;
    int c;
    char d;
    float e;
    struct x* next;
};
```
would have the specification `"ccicfp"`. And since this program:
```c
#include <stdio.h>
struct x {
    char a;
    char b;
    int c;
    char d;
    float e;
    struct x* next;
};
int main(int argc, char* argv[]) {
    printf("%zu\n", sizeof(struct x));
}
```
prints 24 when compiled and run on an x86-64 machine, running `./c-sizer ccicfp`
should also print 24:
```sh
$ ./c-sizer ccicfp
     24 ccicfp
```

To complete this exercise you’ll need to modify the `spec_size` function as
directed. Your function may return 0 if spec is invalid (or it may crash, or
do anything else).


Part 2: C printer
-----------------

Implement the `spec_print` function, which has the following specification:

> `void spec_print(const char* spec, const void* ptr)` print the contents of
> the memory located at `ptr` as interpreted by type specification `spec`. For
> each member in the struct, the function should print the address of that
> member (as a hex pointer), the type of that member, and the value of that
> member.

For example,
```c
int i = 10;
spec_print("i", &i);
```
might print something like
```c
7fffffabc int 10
```

This will help you practice using pointer arithmetic in combination with size
and alignment conventions.


Advanced features
-----------------

If you have time, or you need more practice, implement some of the following
advanced features.

1. Support nested structs, which are set off by braces. For example, `"c{ci}"`
   declares a struct with two components, a char and a nested struct with a
   char and an int. You might declare this struct like so:
   ```c
   struct x {
       char c1;
       struct {
           char c2;
           int i;
       } nested_struct;
   };
   ```
2. Support unions, which are set off by angle brackets. For example, `"<ci>"`
   declares a union with two components, a char and an int.
3. Support arrays, which are defined by a repeat count after a spec component.
   For example, `"c20"` is an array of 20 chars, and `"{ci}20"` is an array of
   20 `{ci}` structures.

Although you should be able to complete the assignment using computer
arithmetic, it is also instructive to complete the assignment using the C
compiler as a helper process. Change spec_size (or make a copy of spec_size)
to compute its result using the C compiler. It should (1) write a simple C
program to a temporary file, (2) run the C compiler on that file, (3) run the
corresponding executable, (4) parse its output, and (5) return the result.
This will teach you lots of useful things about running helper programs from
C. Some interesting library functions (try reading their manuals): `mkstemp`,
`popen`/`pclose`, `system`, `unlink` (to clean up). gcc’s -x c option may
be of use.
