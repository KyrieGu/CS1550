#include "spinlock.h"


struct condvar {
    struct spinlock lk; // a condition variable has a spinlock
};
