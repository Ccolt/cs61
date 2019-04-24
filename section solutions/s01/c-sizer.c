#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>


/// align_to(x, alignment)
///    Return the smallest number `y` so that `y >= x` and `y % alignment == 0`.
///    Requires that `alignment` is a power of two.
static inline size_t align_to(size_t x, size_t alignment) {
    assert((alignment & (alignment - 1)) == 0); // power of 2
    if ((x & (alignment - 1)) != 0) {           // same as `x % alignment != 0`
        x += alignment - (x & (alignment - 1)); // same as `x += alignment - (x % alignment)
    }
    return x;
}


/// spec_charsize(ch)
///    Return the size (and alignment) represented by type specification char `ch`.
///    Returns 0 if `ch` is not one of `csilzfdp`.
static inline size_t spec_charsize(char ch) {
    switch (ch) {
    case 'c':
        return 1;
    case 's':
        return 2;
    case 'i':
    case 'f':
        return 4;
    case 'l':
    case 'z':
    case 'd':
    case 'p':
        return 8;
    default:
        return 0;
    }
}


/// spec_size(spec)
///    Return the size of the C struct specified by the type specification `spec`,
///    as compiled on an x86-64 machine. A type specification is a string of
///    characters where each character corresponds to a struct member of some
///    fundamental type:
///
///         `c`    char (or signed char, unsigned char)
///         `s`    short (or unsigned short)
///         `i`    int (or unsigned int)
///         `l`    long (or unsigned long)
///         `z`    size_t
///         `f`    float
///         `d`    double
///         `p`    pointer
///
///    See `README.md` or the Section 1 wiki page for hints and advanced features.

size_t spec_size(const char* spec) {
    size_t pos = 0;
    size_t max_align = 1;

    for (; *spec != '\0'; ++spec) {
        size_t this_size = spec_charsize(*spec);
        assert(this_size != 0);
        pos = align_to(pos, this_size) + this_size;
        if (max_align < this_size) {
            max_align = this_size;
        }
    }

    return align_to(pos, max_align);
}


/// spec_print(spec, ptr)
///    Print the contents of the memory located at `ptr` as interpreted
///    by type specification `spec`. For example:
///
///        int i = 10;
///        spec_print("i", &i);
///
///    might print something like
///
///        7fffffabc int 10
///
///    (assuming `i` is located at address 0x7fffffabc), and
///
///        struct { char c; int d; } x = { 'A', 24 };
///        spec_print("ci", &x);
///
///    might print something like
///
///        7ffffffabc char A
///        7ffffffac0 int 24
///
///    You'll need to add your own tests of `spec_print`.

void spec_print(const char* spec, const void* data) {
    size_t pos = 0;

    for (; *spec != '\0'; ++spec) {
        size_t this_size = spec_charsize(*spec);
        assert(this_size != 0);
        pos = align_to(pos, this_size);
        const char* this_data = (const char*) data + pos;

        switch (*spec) {
        case 'c':
            printf("%p char %c\n", this_data, *this_data);
            break;
        case 's':
            printf("%p char %hd\n", this_data, *(const short*) this_data);
            break;
        case 'i':
            printf("%p int %d\n", this_data, *(const int*) this_data);
            break;
        case 'f':
            printf("%p float %g\n", this_data, *(const float*) this_data);
            break;
        case 'l':
            printf("%p long %ld\n", this_data, *(const long*) this_data);
            break;
        case 'z':
            printf("%p size_t %zu\n", this_data, *(const size_t*) this_data);
            break;
        case 'd':
            printf("%p double %g\n", this_data, *(const double*) this_data);
            break;
        case 'p':
            printf("%p ptr %p\n", this_data, *(const void**) this_data);
            break;
        }

        pos += this_size;
    }
}


int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%8zu %s\n", spec_size(argv[i]), argv[i]);
    }

    // Run some tests of spec_print
    {
        struct { char c; int d; } x = { 'A', 24 };
        spec_print("ci", &x);

        int a[4] = { 0, 1, 2, 3 };
        spec_print("iiii", &a);
        spec_print("ffff", &a); // !!
        spec_print("pp", &a); // !!
    }
}
