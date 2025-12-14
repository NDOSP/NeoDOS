#include "memory.h"

UINTN align_up(UINTN size, UINTN align) {
    return (size + align - 1) & ~(align - 1);
}
