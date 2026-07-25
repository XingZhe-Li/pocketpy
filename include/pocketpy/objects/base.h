#pragma once

#include "pocketpy/pocketpy.h"

typedef struct PyObject PyObject;
typedef struct VM VM;

/*
 * `pk_current_vm` holds the currently-active VM for the calling thread.
 * On Linux/Clang/MSVC it is a native TLS variable that compiles down to a
 * single `%gs:0x58` (or `%fs:...`) indirection, so accesses are cheap. On
 * MinGW-GCC (which is built with `--enable-threads=posix`), however,
 * `_Thread_local` is implemented via *emulated* TLS -- every read
 * expands into a call to `__emutls_get_address()`. Because the bytecode
 * interpreter reads `pk_current_vm` many times per opcode (via
 * `py_retval`, `pk_typeinfo`, `py_push*`, ...), this used to be the
 * dominant cause of the ~2x runtime gap between GCC- and Clang-built
 * binaries on Windows.
 *
 * We can't ask GCC to switch away from emutls, but we CAN tell it that
 * reading the TLS slot is a *pure* operation (no observable side
 * effects, result depends only on TLS state which does not change
 * during a normal function). GCC will then hoist and CSE repeated
 * reads inside the same function, turning N emutls calls per hot
 * function into 1.
 *
 * All reads should go through `pk_current_vm` (which is now a macro
 * that calls a pure inline getter). The raw storage lives in
 * `pk_current_vm_storage` and only `GlobalSetup.c` writes to it.
 */
extern _Thread_local VM* pk_current_vm_storage;

/*
 * Marked `const` (not just `pure`) so GCC treats the read as fully
 * CSE-able even across other function calls: within a normal call
 * chain `pk_current_vm_storage` doesn't change unless the caller
 * explicitly invokes `py_switchvm` / `py_resetvm`, and those are
 * cold-path operations that are never mixed with `pk_current_vm`
 * reads on the same execution path. Callers that DO need a fresh
 * read after switching must read `pk_current_vm_storage` directly.
 *
 * We deliberately keep this `static inline` (per-TU copy) so that the
 * `const` attribute stays visible at every call site without any ODR
 * complications with the DLL-exported wrappers like `py_retval`.
 */
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((const, always_inline))
    static inline VM* pk_getvm(void) { return pk_current_vm_storage; }
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
