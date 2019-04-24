#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

constexpr off_t BUFSIZE = 4096;

struct io61_file {
    int fd;
    off_t bufsize = BUFSIZE; // or whatever
    unsigned char cbuf[BUFSIZE];
    off_t tag;      // file offset of first byte in cache (0 when file is opened)
    off_t end_tag;  // file offset one past last valid byte in cache
    off_t pos_tag;  // file offset of next char to read in cache
    int mode;       // read or write (no read/write)
    bool is_dirty;  // tracks whether a write file is dirty or clean
};


// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    return f;
}


// io61_close(f)
//    Fill the read cache with new data, starting from file offset `end_tag`.
//    Only called for read caches.

void io61_fill(io61_file* f) {

    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    // Reset the cache to empty.
    f->tag = f->pos_tag = f->end_tag;
    // Read data.
    ssize_t n = read(f->fd, f->cbuf, f->bufsize);
    if (n >= 0) {
        f->end_tag = f->tag + n;
    }

    // Recheck invariants (good practice!).
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    unsigned char buf[1];
    size_t pos = 0;
    size_t ch = 0;
    if (f->pos_tag == f->end_tag) {
        io61_fill(f);
        if (f->pos_tag == f->end_tag) {
            return -1;
        }
    }
    ch = 1;
    buf[0] = f->cbuf[f->pos_tag - f->tag];
    f->pos_tag += ch;
    pos += ch;
    return buf[0];
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    off_t pos = 0;
    off_t ch = 0;
    while (pos < (off_t) sz) {
        if (f->pos_tag == f->end_tag) {
            io61_fill(f);
            if (f->pos_tag == f->end_tag) {
                break;
            }
        }
        if ((off_t) (sz - pos) < f->end_tag - f->pos_tag){
          ch = sz - pos;
          memcpy(buf, &f->cbuf[f->pos_tag - f->tag], ch);
          f->pos_tag += ch;
          pos += ch;
          buf += ch;
        }
        else {
          ch = f->end_tag - f->pos_tag;
          memcpy(buf, &f->cbuf[f->pos_tag - f->tag], ch);
          f->pos_tag += ch;
          pos += ch;
          buf += ch;
        }
    }
    return (ssize_t) pos;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    if (f->end_tag == f->tag + f->bufsize) {
        io61_flush(f);
    }
    // This would be faster if you used `memcpy`!
    size_t count = 1;
    f->cbuf[f->pos_tag - f->tag] = ch;
    f->pos_tag += count;
    f->end_tag += count;
    return 0;
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    // Write cache invariant.
    assert(f->pos_tag == f->end_tag);

    size_t ch = 0;
    size_t pos = 0;
    while (pos < sz) {
        if (f->end_tag == f->tag + f->bufsize) {
            io61_flush(f);
        }
        if((off_t) (sz - pos) < f->bufsize - f->pos_tag + f->tag){
          ch = sz - pos;
        }
        else {
          ch = f->bufsize - f->pos_tag + f->tag;
        }
        memcpy(&f->cbuf[f->pos_tag - f->tag], buf, ch);
        f->pos_tag += ch;
        f->end_tag += ch;
        pos += ch;
        buf += ch;
        f->is_dirty = true;
    }
    return pos;
}


// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize);

    size_t sz = write(f->fd, f->cbuf, f->pos_tag - f->tag);
    f->tag = f->pos_tag;
    f->is_dirty = false;
    return sz;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    int aligned_pos = pos;
    if(f->mode == O_WRONLY) {
      if(f->is_dirty) {
        io61_flush(f);
      }
      off_t r = lseek(f->fd, (off_t) pos, SEEK_SET);
      if (r == pos) {
        f->pos_tag = pos;
        f->end_tag = f->pos_tag;
        f->tag = f->pos_tag;
        return 0;
      }
      else {
        return -1;
      }
    }
    else { 
      if(pos >= f->tag && pos <= f->end_tag){
        f->pos_tag = pos;
        return 0;
      }
      else {
        aligned_pos -= pos % BUFSIZE;
        f->end_tag = aligned_pos;
        f->pos_tag = aligned_pos;
        f->tag = aligned_pos;
        lseek(f->fd, (off_t) aligned_pos, SEEK_SET);
        io61_fill(f);
        f->pos_tag = pos;
        return 0;
      }
    }
}

// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}
