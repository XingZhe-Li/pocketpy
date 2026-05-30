#include "pocketpy/objects/base.h"
#include "pocketpy/pocketpy.h"
#include "pocketpy/objects/object.h"
#include "pocketpy/interpreter/vm.h"
#include "pocketpy/common/sstream.h"
#include <stdbool.h>

static c11_vector* bytearray__vec(py_Ref self) {
    return (c11_vector*)PyObject__userdata(self->_obj);
}

static void bytearray__dtor(void* userdata) {
    c11_vector__dtor((c11_vector*)userdata);
}

static void bytearray__new(py_OutRef out) {
    c11_vector* ud = py_newobject(out, tp_bytearray, 0, sizeof(c11_vector));
    c11_vector__ctor(ud, sizeof(unsigned char));
}

static bool bytearray__check_value(py_Ref val, unsigned char* out) {
    if(val->type != tp_int) return TypeError("byte value must be an int");
    int64_t v = py_toint(val);
    if(v < 0 || v > 255) return ValueError("byte must be in range(0, 256)");
    *out = (unsigned char)v;
    return true;
}

static bool bytearray__new__(int argc, py_Ref argv) {
    // bytearray()
    if(argc == 1) { bytearray__new(py_retval()); return true; }
    if(argc > 2) return TypeError("bytearray() takes at most 1 argument");
    py_Ref arg = py_arg(1);

    if(arg->type == tp_int) {
        int64_t n = py_toint(arg);
        if(n < 0) return ValueError("bytearray() length must be >= 0");
        bytearray__new(py_retval());
        c11_vector* vec = py_touserdata(py_retval());
        c11_vector__reserve(vec, (int)n);
        memset(vec->data, 0, (int)n);
        vec->length = (int)n;
        return true;
    }

    if(arg->type == tp_bytes) {
        int size;
        unsigned char* data = py_tobytes(arg, &size);
        bytearray__new(py_retval());
        c11_vector__extend(py_touserdata(py_retval()), data, size);
        return true;
    }

    if(arg->type == tp_bytearray) {
        c11_vector* src = bytearray__vec(arg);
        bytearray__new(py_retval());
        c11_vector__extend(py_touserdata(py_retval()), src->data, src->length);
        return true;
    }

    py_TValue* p;
    int length = pk_arrayview(arg, &p);
    if(length == -1) return TypeError("bytearray() argument must be iterable");
    bytearray__new(py_retval());
    c11_vector* vec = py_touserdata(py_retval());
    c11_vector__reserve(vec, length);
    for(int i = 0; i < length; i++) {
        unsigned char val;
        if(!bytearray__check_value(&p[i], &val)) return false;
        ((unsigned char*)vec->data)[i] = val;
    }
    vec->length = length;
    return true;
}

static bool bytearray__repr__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_vector* self = bytearray__vec(py_arg(0));
    c11_sbuf buf;
    c11_sbuf__ctor(&buf);
    c11_sbuf__write_cstr(&buf, "bytearray(");
    c11_sbuf__write_char(&buf, 'b');
    c11_sbuf__write_quoted(&buf, (c11_sv){(const char*)self->data, self->length}, '\'');
    c11_sbuf__write_char(&buf, ')');
    c11_sbuf__py_submit(&buf, py_retval());
    return true;
}

static bool bytearray__len__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_newint(py_retval(), self->length);
    return true;
}

static bool bytearray__getitem__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_Ref _1 = py_arg(1);
    if(_1->type == tp_int) {
        int index = py_toint(_1);
        if(!pk__normalize_index(&index, self->length)) return false;
        py_newint(py_retval(), c11__getitem(unsigned char, self, index));
        return true;
    } else if(_1->type == tp_slice) {
        int start, stop, step;
        if(!pk__parse_int_slice(_1, self->length, &start, &stop, &step)) return false;
        bytearray__new(py_retval());
        c11_vector* res = py_touserdata(py_retval());
        PK_SLICE_LOOP(i, start, stop, step) {
            c11_vector__push(unsigned char, res, c11__getitem(unsigned char, self, i));
        }
        return true;
    }
    return TypeError("bytearray indices must be integers or slices");
}

static bool bytearray__setitem__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(3);
    c11_vector* self = bytearray__vec(py_arg(0));
    PY_CHECK_ARG_TYPE(1, tp_int);
    int index = py_toint(py_arg(1));
    if(!pk__normalize_index(&index, self->length)) return false;
    unsigned char val;
    if(!bytearray__check_value(py_arg(2), &val)) return false;
    c11__setitem(unsigned char, self, index, val);
    py_newnone(py_retval());
    return true;
}

static bool bytearray__delitem__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    if(py_istype(py_arg(1), tp_slice)) {
        int start, stop, step;
        if(!pk__parse_int_slice(py_arg(1), self->length, &start, &stop, &step)) return false;
        if(step != 1) return ValueError("slice step must be 1 for deletion");
        int n = stop - start;
        if(n > 0) {
            unsigned char* p = (unsigned char*)self->data;
            memmove(p + start, p + stop, self->length - stop);
            self->length -= n;
        }
        py_newnone(py_retval());
        return true;
    }
    PY_CHECK_ARG_TYPE(1, tp_int);
    int index = py_toint(py_arg(1));
    if(!pk__normalize_index(&index, self->length)) return false;
    c11_vector__erase(unsigned char, self, index);
    py_newnone(py_retval());
    return true;
}

static bool bytearray__eq__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_Ref other = py_arg(1);
    if(other->type == tp_bytearray) {
        c11_vector* other_vec = bytearray__vec(other);
        bool ok = self->length == other_vec->length && memcmp(self->data, other_vec->data, (size_t)self->length) == 0;
        py_newbool(py_retval(), ok);
        return true;
    }
    if(other->type == tp_bytes) {
        int size;
        unsigned char* data = py_tobytes(other, &size);
        bool ok = self->length == size && memcmp(self->data, data, (size_t)size) == 0;
        py_newbool(py_retval(), ok);
        return true;
    }
    py_newnotimplemented(py_retval());
    return true;
}

static bool bytearray__ne__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_Ref other = py_arg(1);
    if(other->type == tp_bytearray) {
        c11_vector* other_vec = bytearray__vec(other);
        bool ok = self->length == other_vec->length && memcmp(self->data, other_vec->data, (size_t)self->length) == 0;
        py_newbool(py_retval(), !ok);
        return true;
    }
    if(other->type == tp_bytes) {
        int size;
        unsigned char* data = py_tobytes(other, &size);
        bool ok = self->length == size && memcmp(self->data, data, (size_t)size) == 0;
        py_newbool(py_retval(), !ok);
        return true;
    }
    py_newnotimplemented(py_retval());
    return true;
}

static bool bytearray__add__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_Ref other = py_arg(1);

    if(other->type == tp_bytearray) {
        c11_vector* other_vec = bytearray__vec(other);
        bytearray__new(py_retval());
        c11_vector* res = py_touserdata(py_retval());
        c11_vector__extend(res, self->data, self->length);
        c11_vector__extend(res, other_vec->data, other_vec->length);
        return true;
    }
    if(other->type == tp_bytes) {
        int size;
        unsigned char* data = py_tobytes(other, &size);
        bytearray__new(py_retval());
        c11_vector* res = py_touserdata(py_retval());
        c11_vector__extend(res, self->data, self->length);
        c11_vector__extend(res, data, size);
        return true;
    }
    py_newnotimplemented(py_retval());
    return true;
}

static bool bytearray__contains__(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_Ref val = py_arg(1);
    if(val->type != tp_int) {
        py_newbool(py_retval(), false);
        return true;
    }
    int64_t v = py_toint(val);
    if(v < 0 || v > 255) {
        py_newbool(py_retval(), false);
        return true;
    }
    unsigned char byte_val = (unsigned char)v;
    py_newbool(py_retval(), memchr(self->data, byte_val, (size_t)self->length) != NULL);
    return true;
}

static bool bytearray_append(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    unsigned char val;
    if(!bytearray__check_value(py_arg(1), &val)) return false;
    c11_vector__push(unsigned char, self, val);
    py_newnone(py_retval());
    return true;
}

static bool bytearray_extend(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_Ref arg = py_arg(1);

    if(arg->type == tp_bytes) {
        int size;
        unsigned char* data = py_tobytes(arg, &size);
        c11_vector__extend(self, data, size);
        py_newnone(py_retval());
        return true;
    }
    if(arg->type == tp_bytearray) {
        c11_vector* src = bytearray__vec(arg);
        c11_vector__extend(self, src->data, src->length);
        py_newnone(py_retval());
        return true;
    }

    py_TValue* p;
    int length = pk_arrayview(arg, &p);
    if(length == -1) return TypeError("argument must be iterable");
    for(int i = 0; i < length; i++) {
        unsigned char val;
        if(!bytearray__check_value(&p[i], &val)) return false;
        c11_vector__push(unsigned char, self, val);
    }
    py_newnone(py_retval());
    return true;
}

static bool bytearray_insert(int argc, py_Ref argv) {
    PY_CHECK_ARGC(3);
    c11_vector* self = bytearray__vec(py_arg(0));
    int index = py_toint(py_arg(1));
    if(index < 0) index = 0;
    if(index > self->length) index = self->length;
    unsigned char val;
    if(!bytearray__check_value(py_arg(2), &val)) return false;
    c11_vector__insert(unsigned char, self, index, val);
    py_newnone(py_retval());
    return true;
}

static bool bytearray_pop(int argc, py_Ref argv) {
    if(argc < 1 || argc > 2) return TypeError("pop expected at most 1 argument");
    c11_vector* self = bytearray__vec(py_arg(0));
    if(self->length == 0) return IndexError("pop from empty bytearray");
    int index;
    if(argc == 2) {
        index = py_toint(py_arg(1));
        if(!pk__normalize_index(&index, self->length)) return false;
    } else {
        index = self->length - 1;
    }
    unsigned char val = c11__getitem(unsigned char, self, index);
    c11_vector__erase(unsigned char, self, index);
    py_newint(py_retval(), val);
    return true;
}

static bool bytearray_remove(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_vector* self = bytearray__vec(py_arg(0));
    unsigned char val;
    if(!bytearray__check_value(py_arg(1), &val)) return false;
    unsigned char* p = (unsigned char*)self->data;
    for(int i = 0; i < self->length; i++) {
        if(p[i] == val) {
            c11_vector__erase(unsigned char, self, i);
            py_newnone(py_retval());
            return true;
        }
    }
    return ValueError("value not found in bytearray");
}

static bool bytearray_reverse(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_vector* self = bytearray__vec(py_arg(0));
    c11__reverse(unsigned char, self);
    py_newnone(py_retval());
    return true;
}

static bool bytearray_decode(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_vector* self = bytearray__vec(py_arg(0));
    py_newstrv(py_retval(), (c11_sv){(const char*)self->data, self->length});
    return true;
}

static bool bytearray_copy(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_vector* self = bytearray__vec(py_arg(0));
    bytearray__new(py_retval());
    c11_vector__extend(py_touserdata(py_retval()), self->data, self->length);
    return true;
}

static bool bytearray_clear(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_vector* self = bytearray__vec(py_arg(0));
    self->length = 0;
    py_newnone(py_retval());
    return true;
}

static bool bytearray_find(int argc, py_Ref argv) {
    if(argc < 2 || argc > 4) return TypeError("find takes 1-3 arguments");
    c11_vector* self = bytearray__vec(py_arg(0));

    c11_sv sub = {NULL, 0};
    if(py_arg(1)->type == tp_bytearray) {
        c11_vector* sub_vec = bytearray__vec(py_arg(1));
        sub = (c11_sv){sub_vec->data, sub_vec->length};
    } else if(py_arg(1)->type == tp_bytes) {
        int size;
        sub.data = (const char*)py_tobytes(py_arg(1), &size);
        sub.size = size;
    } else {
        return TypeError("argument must be bytes or bytearray");
    }

    int start = 0, end = self->length;
    if(argc >= 3) {
        start = py_toint(py_arg(2));
        if(start < 0) start = 0;
        if(start > self->length) start = self->length;
    }
    if(argc >= 4) {
        end = py_toint(py_arg(3));
        if(end < 0) end = 0;
        if(end > self->length) end = self->length;
    }

    if(sub.size == 0) { py_newint(py_retval(), start); return true; }

    unsigned char* p = (unsigned char*)self->data;
    for(int i = start; i <= end - sub.size; i++) {
        if(memcmp(p + i, sub.data, (size_t)sub.size) == 0) {
            py_newint(py_retval(), i);
            return true;
        }
    }
    py_newint(py_retval(), -1);
    return true;
}

static bool bytearray_hex(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_vector* self = bytearray__vec(py_arg(0));
    c11_sbuf buf;
    c11_sbuf__ctor(&buf);
    unsigned char* p = (unsigned char*)self->data;
    for(int i = 0; i < self->length; i++) {
        c11_sbuf__write_hex(&buf, p[i], false);
    }
    c11_sbuf__py_submit(&buf, py_retval());
    return true;
}

py_Type pk_bytearray__register() {
    pk_newtype("bytearray", tp_object, NULL, bytearray__dtor, false, true);

    py_bindmagic(tp_bytearray, __new__, bytearray__new__);
    py_bindmagic(tp_bytearray, __repr__, bytearray__repr__);
    py_bindmagic(tp_bytearray, __len__, bytearray__len__);
    py_bindmagic(tp_bytearray, __getitem__, bytearray__getitem__);
    py_bindmagic(tp_bytearray, __setitem__, bytearray__setitem__);
    py_bindmagic(tp_bytearray, __delitem__, bytearray__delitem__);
    py_bindmagic(tp_bytearray, __eq__, bytearray__eq__);
    py_bindmagic(tp_bytearray, __ne__, bytearray__ne__);
    py_bindmagic(tp_bytearray, __add__, bytearray__add__);
    py_bindmagic(tp_bytearray, __contains__, bytearray__contains__);

    py_bindmethod(tp_bytearray, "append", bytearray_append);
    py_bindmethod(tp_bytearray, "extend", bytearray_extend);
    py_bindmethod(tp_bytearray, "insert", bytearray_insert);
    py_bindmethod(tp_bytearray, "pop", bytearray_pop);
    py_bindmethod(tp_bytearray, "remove", bytearray_remove);
    py_bindmethod(tp_bytearray, "reverse", bytearray_reverse);
    py_bindmethod(tp_bytearray, "decode", bytearray_decode);
    py_bindmethod(tp_bytearray, "copy", bytearray_copy);
    py_bindmethod(tp_bytearray, "clear", bytearray_clear);
    py_bindmethod(tp_bytearray, "hex", bytearray_hex);
    py_bindmethod(tp_bytearray, "find", bytearray_find);

    return tp_bytearray;
}
