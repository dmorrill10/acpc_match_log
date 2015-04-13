#ifndef __TEST_HELPER__
#define __TEST_HELPER__

#include <stdio.h>

#include <catch.hpp>

#define DEBUG_PRINT(...) \
    printf(__VA_ARGS__); \
    fflush(NULL);

#endif
