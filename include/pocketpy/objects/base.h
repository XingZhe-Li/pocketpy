#pragma once

#include "pocketpy/pocketpy.h"

typedef struct PyObject PyObject;
typedef struct VM VM;

/* pk_current_vm: TID-keyed cache to avoid emutls overhead on MinGW-GCC */
extern PK_THREAD_LOCAL VM* pk_current_vm_storage;

#define PK_VM_TID_CACHE_BITS 4
#define PK_VM_TID_CACHE_SIZE (1 << PK_VM_TID_CACHE_BITS)

typedef struct pk_vm_tid_cache_entry {
    uint32_t tid;
    VM* vm;
} pk_vm_tid_cache_entry;

extern pk_vm_tid_cache_entry pk_vm_tid_cache[PK_VM_TID_CACHE_SIZE];

VM* pk_getvm_slow(unsigned slot, uint32_t tid);
VM* pk_getvm(void);

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
