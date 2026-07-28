#pragma once

#include "pocketpy/pocketpy.h"

typedef struct PyObject PyObject;
typedef struct VM VM;

/*
 * Experimental: TID-keyed inline cache for `pk_current_vm`.
 *
 * pk_current_vm is a `_Thread_local` pointer. On MinGW-GCC (posix threads)
 * every read expands into a `__emutls_get_address()` function call, and
 * because pocketpy's dispatch loop is a switch with many entry points via
 * fall-through / `goto __ERROR`, GCC cannot CSE those calls -- every opcode
 * handler that touches `pk_current_vm` pays the emutls tax.
 *
 * Idea: read the current thread id straight from the TIB (`%gs:0x48`),
 * index into a tiny direct-mapped cache indexed by low bits of the tid,
 * and if the cache hits, return the stored VM pointer with zero function
 * calls. Slow-path (mismatched or empty slot) falls back to the real TLS
 * variable and refreshes the cache.
 *
 * The cache is a plain (non-TLS) global -- entries are simple TID/VM
 * pairs, so misidentification is impossible: worst case we refresh
 * from the ground-truth TLS variable.
 */
extern _Thread_local VM* pk_current_vm_storage;

#define PK_VM_TID_CACHE_BITS 4
#define PK_VM_TID_CACHE_SIZE (1 << PK_VM_TID_CACHE_BITS)

typedef struct pk_vm_tid_cache_entry {
    uint32_t tid;
    VM* vm;
} pk_vm_tid_cache_entry;

extern pk_vm_tid_cache_entry pk_vm_tid_cache[PK_VM_TID_CACHE_SIZE];

#if defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
    /* Slow path: refill cache from the actual TLS. Kept out-of-line so it
     * does not bloat every call site. */
    __attribute__((noinline))
    VM* pk_getvm_slow(unsigned slot, uint32_t tid);

    __attribute__((always_inline))
    static inline uint32_t pk_read_tid(void) {
        uint32_t tid;
        __asm__ __volatile__ ("mov %%gs:0x48, %0" : "=r"(tid));
        return tid;
    }

    __attribute__((always_inline))
    static inline VM* pk_getvm(void) {
        uint32_t tid = pk_read_tid();
        unsigned slot = tid & (PK_VM_TID_CACHE_SIZE - 1);
        pk_vm_tid_cache_entry e = pk_vm_tid_cache[slot];
        if (__builtin_expect(e.tid == tid, 1)) return e.vm;
        return pk_getvm_slow(slot, tid);
    }
#else
    static inline VM* pk_getvm(void) { return pk_current_vm_storage; }
#endif

#define pk_current_vm (pk_getvm())
typedef struct py_TValue {
    py_Type type;
    bool is_ptr;
    int extra;

    union {
        int64_t _i64;
        double _f64;
        bool _bool;
        py_CFunction _cfunc;
        PyObject* _obj;
        c11_vec2 _vec2;
        c11_vec2i _vec2i;
        c11_vec3 _vec3;
        c11_vec3i _vec3i;
        c11_vec4i _vec4i;
        c11_color32 _color32;
        void* _ptr;
        char _chars[16];
    };
} py_TValue;