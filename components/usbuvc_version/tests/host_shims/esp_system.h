#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#define MALLOC_CAP_DEFAULT 0
#define heap_caps_malloc(size, caps) malloc(size)
