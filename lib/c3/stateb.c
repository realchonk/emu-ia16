/*
 * c3b knows the largest function c3 itself needs to compile; using the
 * smaller pools here leaves room for libc and the stack in the 64 KiB
 * segment.
 */
#define MAXINS	1100
#define MAXVAL	950
#include "state.c"
