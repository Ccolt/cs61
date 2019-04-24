// Testing file for experimentation purposes

#include "m61.hh"
//#include "m61.cc"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

void* main() {
  void* test = base_malloc(10) + 2
  hexdump(test, 10);
}
