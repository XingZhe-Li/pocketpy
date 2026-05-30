#include "pocketpy/interpreter/vm.h"
#include <string.h>

/* Format character codes */
typedef enum {
    struct__FORMAT_PAD,       // x
    struct__FORMAT_CHAR,      // c
    struct__FORMAT_SBYTE,     // b
    struct__FORMAT_BYTE,      // B
    struct__FORMAT_BOOL,      // ?
    struct__FORMAT_SHORT,     // h
    struct__FORMAT_USHORT,    // H
    struct__FORMAT_INT,       // i
    struct__FORMAT_UINT,      // I
    struct__FORMAT_LONG,      // l
    struct__FORMAT_ULONG,     // L
    struct__FORMAT_LONGLONG,  // q
    struct__FORMAT_ULONGLONG, // Q
    struct__FORMAT_FLOAT,     // f
    struct__FORMAT_DOUBLE,    // d
    struct__FORMAT_STRING,    // s
    struct__FORMAT_PSTRING,   // p
} struct__FormatCode;

typedef enum {
    struct__ENDIAN_NATIVE,   // @ =
    struct__ENDIAN_LITTLE,   // <
    struct__ENDIAN_BIG,      // > !
} struct__Endianness;

typedef struct {
    struct__FormatCode code;
    int count;
} struct__Field;

typedef struct {
    struct__Field* fields;
    int nfields;
    int size;
    struct__Endianness endian;
} struct__Format;

static int struct__format_field_size(struct__FormatCode code, int count) {
    switch(code) {
        case struct__FORMAT_PAD:    return count;
        case struct__FORMAT_CHAR:   return count;
        case struct__FORMAT_SBYTE:  return count;
        case struct__FORMAT_BYTE:   return count;
        case struct__FORMAT_BOOL:   return count;
        case struct__FORMAT_SHORT:  return count * 2;
        case struct__FORMAT_USHORT: return count * 2;
        case struct__FORMAT_INT:    return count * 4;
        case struct__FORMAT_UINT:   return count * 4;
        case struct__FORMAT_LONG:   return count * 4;
        case struct__FORMAT_ULONG:  return count * 4;
        case struct__FORMAT_LONGLONG:  return count * 8;
        case struct__FORMAT_ULONGLONG: return count * 8;
        case struct__FORMAT_FLOAT:  return count * 4;
        case struct__FORMAT_DOUBLE: return count * 8;
        case struct__FORMAT_STRING: return count;
        case struct__FORMAT_PSTRING: return count;
    }
    return 0;
}

static bool struct__parse_format(const char* fmt, int len, struct__Format* info) {
    memset(info, 0, sizeof(struct__Format));
    info->endian = struct__ENDIAN_NATIVE;

    if(len == 0) return false;

    int i = 0;
    switch(fmt[0]) {
        case '@': case '=': info->endian = struct__ENDIAN_NATIVE; i = 1; break;
        case '<':           info->endian = struct__ENDIAN_LITTLE; i = 1; break;
        case '>': case '!': info->endian = struct__ENDIAN_BIG; i = 1; break;
    }

    // Count fields
    int nfields = 0;
    for(int j = i; j < len; ) {
        if(fmt[j] >= '0' && fmt[j] <= '9') {
            while(j < len && fmt[j] >= '0' && fmt[j] <= '9') j++;
        } else {
            if(fmt[j] == ' ' || fmt[j] == '\t') { j++; continue; }
            if(!strchr("xcbB?hHiIlLqQfdspP", fmt[j])) return false;
            nfields++;
            j++;
        }
    }
    if(i >= len) { info->nfields = 0; info->size = 0; return true; }
    if(nfields == 0) return false;

    info->fields = (struct__Field*)py_malloc(nfields * sizeof(struct__Field));
    info->nfields = nfields;
    int idx = 0;

    for(int j = i; j < len; ) {
        if(fmt[j] == ' ' || fmt[j] == '\t') { j++; continue; }

        int count = 1;
        if(fmt[j] >= '0' && fmt[j] <= '9') {
            count = 0;
            while(j < len && fmt[j] >= '0' && fmt[j] <= '9') {
                count = count * 10 + (fmt[j] - '0');
                j++;
            }
            if(count <= 0) { py_free(info->fields); info->fields = NULL; return false; }
        }
        if(j >= len) { py_free(info->fields); info->fields = NULL; return false; }

        char c = fmt[j++];
        struct__FormatCode code;
        switch(c) {
            case 'x': code = struct__FORMAT_PAD; break;
            case 'c': code = struct__FORMAT_CHAR; break;
            case 'b': code = struct__FORMAT_SBYTE; break;
            case 'B': code = struct__FORMAT_BYTE; break;
            case '?': code = struct__FORMAT_BOOL; break;
            case 'h': code = struct__FORMAT_SHORT; break;
            case 'H': code = struct__FORMAT_USHORT; break;
            case 'i': case 'l': code = struct__FORMAT_INT; break;
            case 'I': case 'L': code = struct__FORMAT_UINT; break;
            case 'q': code = struct__FORMAT_LONGLONG; break;
            case 'Q': case 'P': code = struct__FORMAT_ULONGLONG; break;
            case 'f': code = struct__FORMAT_FLOAT; break;
            case 'd': code = struct__FORMAT_DOUBLE; break;
            case 's': code = struct__FORMAT_STRING; break;
            case 'p': code = struct__FORMAT_PSTRING; break;
            default: py_free(info->fields); info->fields = NULL; return false;
        }

        info->fields[idx].code = code;
        info->fields[idx].count = count;
        idx++;
        info->size += struct__format_field_size(code, count);
    }
    return true;
}

static void struct__free_format(struct__Format* info) {
    if(info->fields) py_free(info->fields);
}

/* Count values needed by format */
static int struct__count_values(struct__Format* info) {
    int n = 0;
    for(int i = 0; i < info->nfields; i++) {
        struct__FormatCode code = info->fields[i].code;
        if(code == struct__FORMAT_PAD) continue;
        if(code == struct__FORMAT_STRING || code == struct__FORMAT_PSTRING) { n++; continue; }
        if(code == struct__FORMAT_CHAR) { n += info->fields[i].count; continue; }
        n += info->fields[i].count;
    }
    return n;
}

/* -------------------------------------------------------------------------- */
/* Internal pack/unpack helpers                                               */
/* -------------------------------------------------------------------------- */

static bool struct__pack_value(struct__FormatCode code, unsigned char* buf, int* offset,
                       int count, py_Ref argv, int* argi, int argc,
                       struct__Endianness endian) {
    if(code == struct__FORMAT_PAD) {
        memset(buf + *offset, 0, count);
        *offset += count;
        return true;
    }
    if(code == struct__FORMAT_STRING) {
        py_Ref arg = py_arg(*argi); (*argi)++;
        int src_size = 0;
        unsigned char* src = NULL;
        if(py_istype(arg, tp_str)) {
            c11_sv sv = py_tosv(arg);
            src = (unsigned char*)sv.data; src_size = sv.size;
        } else if(py_istype(arg, tp_bytes)) {
            src = py_tobytes(arg, &src_size);
        } else {
            return TypeError("expected bytes or str, got %t", arg->type);
        }
        int pad = count;
        int copy = src_size < pad ? src_size : pad;
        if(copy > 0) memcpy(buf + *offset, src, copy);
        if(pad > copy) memset(buf + *offset + copy, 0, pad - copy);
        *offset += pad;
        return true;
    }
    if(code == struct__FORMAT_PSTRING) {
        py_Ref arg = py_arg(*argi); (*argi)++;
        int src_size = 0;
        unsigned char* src = NULL;
        if(py_istype(arg, tp_str)) {
            c11_sv sv = py_tosv(arg);
            src = (unsigned char*)sv.data; src_size = sv.size;
        } else if(py_istype(arg, tp_bytes)) {
            src = py_tobytes(arg, &src_size);
        } else {
            return TypeError("expected bytes or str, got %t", arg->type);
        }
        int maxlen = count - 1;
        if(maxlen <= 0) return TypeError("p format requires count > 0");
        int slen = src_size < maxlen ? src_size : maxlen;
        buf[(*offset)++] = (unsigned char)slen;
        if(slen > 0) memcpy(buf + *offset, src, slen);
        if(maxlen > slen) memset(buf + *offset + slen, 0, maxlen - slen);
        *offset += maxlen;
        return true;
    }

    for(int i = 0; i < count; i++) {
        if(*argi >= argc) return true;
        py_Ref arg = py_arg(*argi); (*argi)++;

        switch(code) {
            case struct__FORMAT_CHAR: {
                int sz; unsigned char* d = py_tobytes(arg, &sz);
                if(sz < 1) return TypeError("char format requires bytes of length 1");
                buf[(*offset)++] = d[0];
                break;
            }
            case struct__FORMAT_SBYTE: {
                py_i64 v = py_toint(arg);
                buf[(*offset)++] = (unsigned char)(v & 0xFF);
                break;
            }
            case struct__FORMAT_BYTE: {
                py_i64 v = py_toint(arg);
                buf[(*offset)++] = (unsigned char)(v & 0xFF);
                break;
            }
            case struct__FORMAT_BOOL: {
                buf[(*offset)++] = py_tobool(arg) ? 1 : 0;
                break;
            }
            case struct__FORMAT_SHORT: case struct__FORMAT_USHORT: {
                unsigned short us = (unsigned short)(py_toint(arg) & 0xFFFF);
                if(endian == struct__ENDIAN_LITTLE) {
                    buf[(*offset)++] = us & 0xFF;
                    buf[(*offset)++] = (us >> 8) & 0xFF;
                } else {
                    buf[(*offset)++] = (us >> 8) & 0xFF;
                    buf[(*offset)++] = us & 0xFF;
                }
                break;
            }
            case struct__FORMAT_INT: case struct__FORMAT_UINT:
            case struct__FORMAT_LONG: case struct__FORMAT_ULONG: {
                unsigned int ui = (unsigned int)(py_toint(arg) & 0xFFFFFFFFULL);
                if(endian == struct__ENDIAN_LITTLE) {
                    for(int b = 0; b < 4; b++) buf[(*offset)++] = (ui >> (b*8)) & 0xFF;
                } else {
                    for(int b = 3; b >= 0; b--) buf[(*offset)++] = (ui >> (b*8)) & 0xFF;
                }
                break;
            }
            case struct__FORMAT_LONGLONG: case struct__FORMAT_ULONGLONG: {
                unsigned long long ull = (unsigned long long)py_toint(arg);
                if(endian == struct__ENDIAN_LITTLE) {
                    for(int b = 0; b < 8; b++) buf[(*offset)++] = (ull >> (b*8)) & 0xFF;
                } else {
                    for(int b = 7; b >= 0; b--) buf[(*offset)++] = (ull >> (b*8)) & 0xFF;
                }
                break;
            }
            case struct__FORMAT_FLOAT: {
                float f = (float)py_tofloat(arg);
                unsigned int ui; memcpy(&ui, &f, sizeof(ui));
                if(endian == struct__ENDIAN_LITTLE) {
                    for(int b = 0; b < 4; b++) buf[(*offset)++] = (ui >> (b*8)) & 0xFF;
                } else {
                    for(int b = 3; b >= 0; b--) buf[(*offset)++] = (ui >> (b*8)) & 0xFF;
                }
                break;
            }
            case struct__FORMAT_DOUBLE: {
                double d = py_tofloat(arg);
                unsigned long long ull; memcpy(&ull, &d, sizeof(ull));
                if(endian == struct__ENDIAN_LITTLE) {
                    for(int b = 0; b < 8; b++) buf[(*offset)++] = (ull >> (b*8)) & 0xFF;
                } else {
                    for(int b = 7; b >= 0; b--) buf[(*offset)++] = (ull >> (b*8)) & 0xFF;
                }
                break;
            }
            default: return false;
        }
    }
    return true;
}

static bool struct__unpack_value(struct__FormatCode code, const unsigned char* buf, int* offset,
                         int count, py_TValue* data, int* outi,
                         struct__Endianness endian) {
    if(code == struct__FORMAT_PAD) { *offset += count; return true; }

    for(int i = 0; i < count; i++) {
        py_Ref out = py_pushtmp();

        switch(code) {
            case struct__FORMAT_CHAR: {
                unsigned char* dst = py_newbytes(out, 1);
                dst[0] = buf[(*offset)++];
                break;
            }
            case struct__FORMAT_SBYTE: {
                py_newint(out, (py_i64)(signed char)buf[*offset]); (*offset)++;
                break;
            }
            case struct__FORMAT_BYTE: {
                py_newint(out, (py_i64)buf[*offset]); (*offset)++;
                break;
            }
            case struct__FORMAT_BOOL: {
                py_newbool(out, buf[*offset] != 0); (*offset)++;
                break;
            }
            case struct__FORMAT_SHORT: {
                unsigned short us;
                if(endian == struct__ENDIAN_LITTLE)
                    us = (unsigned)buf[*offset] | ((unsigned)buf[*offset+1] << 8);
                else
                    us = ((unsigned)buf[*offset] << 8) | (unsigned)buf[*offset+1];
                py_newint(out, (py_i64)(short)us); *offset += 2;
                break;
            }
            case struct__FORMAT_USHORT: {
                unsigned short us;
                if(endian == struct__ENDIAN_LITTLE)
                    us = (unsigned)buf[*offset] | ((unsigned)buf[*offset+1] << 8);
                else
                    us = ((unsigned)buf[*offset] << 8) | (unsigned)buf[*offset+1];
                py_newint(out, (py_i64)us); *offset += 2;
                break;
            }
            case struct__FORMAT_INT: case struct__FORMAT_LONG: {
                unsigned int ui = 0;
                if(endian == struct__ENDIAN_LITTLE)
                    for(int b=0;b<4;b++) ui |= (unsigned)buf[*offset+b] << (b*8);
                else
                    for(int b=0;b<4;b++) ui |= (unsigned)buf[*offset+b] << ((3-b)*8);
                py_newint(out, (py_i64)(int)ui); *offset += 4;
                break;
            }
            case struct__FORMAT_UINT: case struct__FORMAT_ULONG: {
                unsigned int ui = 0;
                if(endian == struct__ENDIAN_LITTLE)
                    for(int b=0;b<4;b++) ui |= (unsigned)buf[*offset+b] << (b*8);
                else
                    for(int b=0;b<4;b++) ui |= (unsigned)buf[*offset+b] << ((3-b)*8);
                py_newint(out, (py_i64)ui); *offset += 4;
                break;
            }
            case struct__FORMAT_LONGLONG: case struct__FORMAT_ULONGLONG: {
                unsigned long long ull = 0;
                if(endian == struct__ENDIAN_LITTLE)
                    for(int b=0;b<8;b++) ull |= (unsigned long long)buf[*offset+b] << (b*8);
                else
                    for(int b=0;b<8;b++) ull |= (unsigned long long)buf[*offset+b] << ((7-b)*8);
                py_newint(out, (py_i64)ull); *offset += 8;
                break;
            }
            case struct__FORMAT_FLOAT: {
                unsigned int ui = 0;
                if(endian == struct__ENDIAN_LITTLE)
                    for(int b=0;b<4;b++) ui |= (unsigned)buf[*offset+b] << (b*8);
                else
                    for(int b=0;b<4;b++) ui |= (unsigned)buf[*offset+b] << ((3-b)*8);
                float f; memcpy(&f, &ui, sizeof(f));
                py_newfloat(out, (py_f64)f); *offset += 4;
                break;
            }
            case struct__FORMAT_DOUBLE: {
                unsigned long long ull = 0;
                if(endian == struct__ENDIAN_LITTLE)
                    for(int b=0;b<8;b++) ull |= (unsigned long long)buf[*offset+b] << (b*8);
                else
                    for(int b=0;b<8;b++) ull |= (unsigned long long)buf[*offset+b] << ((7-b)*8);
                double d; memcpy(&d, &ull, sizeof(d));
                py_newfloat(out, (py_f64)d); *offset += 8;
                break;
            }
            default: return false;
        }
        data[*outi] = *out;
        py_shrink(1);
        (*outi)++;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/* struct.calcsize                                                            */
/* -------------------------------------------------------------------------- */

static bool struct_calcsize(int argc, py_Ref argv) {
    PY_CHECK_ARGC(1);
    c11_sv sv = py_tosv(py_arg(0));
    struct__Format info;
    if(!struct__parse_format(sv.data, sv.size, &info))
        return TypeError("bad char in struct format");
    py_newint(py_retval(), info.size);
    struct__free_format(&info);
    return true;
}

/* -------------------------------------------------------------------------- */
/* struct.pack                                                                */
/* -------------------------------------------------------------------------- */

static bool struct_pack(int argc, py_Ref argv) {
    if(argc < 1) return TypeError("expected at least 1 argument");
    c11_sv sv = py_tosv(py_arg(0));
    struct__Format info;
    if(!struct__parse_format(sv.data, sv.size, &info))
        return TypeError("bad char in struct format");

    int nvalues = argc - 1;
    int expected = struct__count_values(&info);
    if(nvalues != expected) {
        struct__free_format(&info);
        return TypeError("expected %d arguments for pack, got %d", expected, nvalues);
    }

    unsigned char* buf = py_newbytes(py_retval(), info.size);
    int offset = 0;
    int argi = 1;

    for(int i = 0; i < info.nfields; i++) {
        struct__FormatCode code = info.fields[i].code;
        int cnt = info.fields[i].count;
        if(!struct__pack_value(code, buf, &offset, cnt, argv, &argi, argc, info.endian)) {
            struct__free_format(&info);
            return false;
        }
    }
    struct__free_format(&info);
    return true;
}

/* -------------------------------------------------------------------------- */
/* struct.unpack                                                              */
/* -------------------------------------------------------------------------- */

static bool struct_unpack(int argc, py_Ref argv) {
    PY_CHECK_ARGC(2);
    c11_sv sv = py_tosv(py_arg(0));
    struct__Format info;
    if(!struct__parse_format(sv.data, sv.size, &info))
        return TypeError("bad char in struct format");

    int buf_size;
    unsigned char* buf = py_tobytes(py_arg(1), &buf_size);
    if(buf_size < info.size) {
        struct__free_format(&info);
        return TypeError("unpack requires a buffer of %d bytes", info.size);
    }

    int nvalues = struct__count_values(&info);
    py_Ref header = py_pushtmp();
    py_TValue* data = py_newtuple(header, nvalues);
    int offset = 0;
    int outi = 0;

    for(int i = 0; i < info.nfields; i++) {
        struct__FormatCode code = info.fields[i].code;
        int cnt = info.fields[i].count;
        if(code == struct__FORMAT_PAD) { offset += cnt; continue; }
        if(code == struct__FORMAT_STRING) {
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, cnt);
            memcpy(dst, buf + offset, cnt);
            offset += cnt;
            data[outi] = *tmp;
            py_shrink(1);
            outi++;
            continue;
        }
        if(code == struct__FORMAT_PSTRING) {
            int slen = buf[offset];
            if(slen >= cnt) slen = cnt - 1;
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, slen);
            memcpy(dst, buf + offset + 1, slen);
            offset += cnt;
            data[outi] = *tmp;
            py_shrink(1);
            outi++;
            continue;
        }
        if(code == struct__FORMAT_CHAR) {
            for(int j = 0; j < cnt; j++) {
                py_Ref tmp = py_pushtmp();
                unsigned char* dst = py_newbytes(tmp, 1);
                dst[0] = buf[offset++];
                data[outi] = *tmp;
                py_shrink(1);
                outi++;
            }
            continue;
        }
        for(int j = 0; j < cnt; j++) {
            if(!struct__unpack_value(code, buf, &offset, 1, data, &outi, info.endian)) {
                struct__free_format(&info);
                py_shrink(1);
                return false;
            }
        }
    }

    *py_retval() = *header;
    py_shrink(1);
    struct__free_format(&info);
    return true;
}

/* -------------------------------------------------------------------------- */
/* struct.pack_into                                                           */
/* -------------------------------------------------------------------------- */

static bool struct_pack_into(int argc, py_Ref argv) {
    if(argc < 3) return TypeError("expected at least 3 arguments");
    c11_sv sv = py_tosv(py_arg(0));
    struct__Format info;
    if(!struct__parse_format(sv.data, sv.size, &info))
        return TypeError("bad char in struct format");

    int buf_size;
    unsigned char* buf = py_tobytes(py_arg(1), &buf_size);
    py_i64 off = py_toint(py_arg(2));
    if(off < 0) { struct__free_format(&info); return TypeError("offset must be non-negative"); }
    if(off + info.size > buf_size) {
        struct__free_format(&info);
        return TypeError("pack_into requires a buffer of at least %d bytes", (int)(off + info.size));
    }

    int nvalues = argc - 3;
    if(nvalues != struct__count_values(&info)) {
        struct__free_format(&info);
        return TypeError("expected %d arguments for pack, got %d", struct__count_values(&info), nvalues);
    }

    int offset = (int)off;
    int argi = 3;
    for(int i = 0; i < info.nfields; i++) {
        struct__FormatCode code = info.fields[i].code;
        int cnt = info.fields[i].count;
        if(!struct__pack_value(code, buf, &offset, cnt, argv, &argi, argc, info.endian)) {
            struct__free_format(&info);
            return false;
        }
    }
    struct__free_format(&info);
    py_newnone(py_retval());
    return true;
}

/* -------------------------------------------------------------------------- */
/* struct.unpack_from                                                         */
/* -------------------------------------------------------------------------- */

static bool struct_unpack_from(int argc, py_Ref argv) {
    if(argc < 2 || argc > 3) return TypeError("expected 2 or 3 arguments");
    c11_sv sv = py_tosv(py_arg(0));
    struct__Format info;
    if(!struct__parse_format(sv.data, sv.size, &info))
        return TypeError("bad char in struct format");

    py_i64 off = 0;
    if(argc == 3) {
        off = py_toint(py_arg(2));
        if(off < 0) { struct__free_format(&info); return TypeError("offset must be non-negative"); }
    }

    int buf_size;
    unsigned char* buf = py_tobytes(py_arg(1), &buf_size);
    if(off + info.size > buf_size) {
        struct__free_format(&info);
        return TypeError("unpack_from requires a buffer of at least %d bytes", (int)(off + info.size));
    }

    int nvalues = struct__count_values(&info);
    py_Ref header = py_pushtmp();
    py_TValue* data = py_newtuple(header, nvalues);
    int offset = (int)off;
    int outi = 0;

    for(int i = 0; i < info.nfields; i++) {
        struct__FormatCode code = info.fields[i].code;
        int cnt = info.fields[i].count;
        if(code == struct__FORMAT_PAD) { offset += cnt; continue; }
        if(code == struct__FORMAT_STRING) {
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, cnt);
            memcpy(dst, buf + offset, cnt);
            offset += cnt;
            data[outi] = *tmp;
            py_shrink(1);
            outi++;
            continue;
        }
        if(code == struct__FORMAT_PSTRING) {
            int slen = buf[offset];
            if(slen >= cnt) slen = cnt - 1;
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, slen);
            memcpy(dst, buf + offset + 1, slen);
            offset += cnt;
            data[outi] = *tmp;
            py_shrink(1);
            outi++;
            continue;
        }
        if(code == struct__FORMAT_CHAR) {
            for(int j = 0; j < cnt; j++) {
                py_Ref tmp = py_pushtmp();
                unsigned char* dst = py_newbytes(tmp, 1);
                dst[0] = buf[offset++];
                data[outi] = *tmp;
                py_shrink(1);
                outi++;
            }
            continue;
        }
        for(int j = 0; j < cnt; j++) {
            if(!struct__unpack_value(code, buf, &offset, 1, data, &outi, info.endian)) {
                struct__free_format(&info);
                py_shrink(1);
                return false;
            }
        }
    }

    *py_retval() = *header;
    py_shrink(1);
    struct__free_format(&info);
    return true;
}

/* -------------------------------------------------------------------------- */
/* Struct class                                                               */
/* -------------------------------------------------------------------------- */

static py_Type Struct_type;

typedef struct {
    char* fmt_str;
    struct__Format info;
} struct__Data;

static void Struct__dtor(void* userdata) {
    struct__Data* self = (struct__Data*)userdata;
    if(self) {
        struct__free_format(&self->info);
        if(self->fmt_str) py_free(self->fmt_str);
    }
}

static bool Struct__new__(int argc, py_Ref argv) {
    struct__Data* data = (struct__Data*)py_newobject(py_retval(), py_totype(argv), 4, sizeof(struct__Data));
    memset(data, 0, sizeof(struct__Data));
    return true;
}

static bool Struct__init__(int argc, py_Ref argv) {
    // argc includes self
    if(argc != 2) return TypeError("expected 1 arguments, got %d", argc - 1);
    struct__Data* data = (struct__Data*)PyObject__userdata(argv->_obj);
    c11_sv sv = py_tosv(py_arg(1));

    struct__free_format(&data->info);
    if(data->fmt_str) { py_free(data->fmt_str); data->fmt_str = NULL; }

    if(!struct__parse_format(sv.data, sv.size, &data->info))
        return TypeError("bad char in struct format");

    data->fmt_str = (char*)py_malloc(sv.size + 1);
    memcpy(data->fmt_str, sv.data, sv.size);
    data->fmt_str[sv.size] = '\0';
    return true;
}

static bool Struct__size_getter(int argc, py_Ref argv) {
    // property getter: argc includes self
    PY_CHECK_ARGC(1);
    struct__Data* data = (struct__Data*)PyObject__userdata(argv->_obj);
    py_newint(py_retval(), data->info.size);
    return true;
}

static bool Struct_pack(int argc, py_Ref argv) {
    struct__Data* data = (struct__Data*)PyObject__userdata(argv->_obj);
    int nvalues = argc - 1;
    if(nvalues != struct__count_values(&data->info))
        return TypeError("expected %d arguments for pack, got %d", struct__count_values(&data->info), nvalues);

    unsigned char* buf = py_newbytes(py_retval(), data->info.size);
    int offset = 0;
    int argi = 1;
    for(int i = 0; i < data->info.nfields; i++) {
        struct__FormatCode code = data->info.fields[i].code;
        int cnt = data->info.fields[i].count;
        if(!struct__pack_value(code, buf, &offset, cnt, argv, &argi, argc, data->info.endian))
            return false;
    }
    return true;
}

static bool Struct_unpack(int argc, py_Ref argv) {
    // argc includes self
    if(argc != 2) return TypeError("expected 1 arguments, got %d", argc - 1);
    struct__Data* data = (struct__Data*)PyObject__userdata(argv->_obj);
    int buf_size;
    unsigned char* buf = py_tobytes(py_arg(1), &buf_size);
    if(buf_size < data->info.size)
        return TypeError("unpack requires a buffer of %d bytes", data->info.size);

    int nvalues = struct__count_values(&data->info);
    py_Ref header = py_pushtmp();
    py_TValue* slot_data = py_newtuple(header, nvalues);
    int offset = 0;
    int outi = 0;

    for(int i = 0; i < data->info.nfields; i++) {
        struct__FormatCode code = data->info.fields[i].code;
        int cnt = data->info.fields[i].count;
        if(code == struct__FORMAT_PAD) { offset += cnt; continue; }
        if(code == struct__FORMAT_STRING) {
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, cnt);
            memcpy(dst, buf + offset, cnt); offset += cnt;
            slot_data[outi] = *tmp; py_shrink(1); outi++;
            continue;
        }
        if(code == struct__FORMAT_PSTRING) {
            int slen = buf[offset]; if(slen >= cnt) slen = cnt - 1;
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, slen);
            memcpy(dst, buf + offset + 1, slen); offset += cnt;
            slot_data[outi] = *tmp; py_shrink(1); outi++;
            continue;
        }
        if(code == struct__FORMAT_CHAR) {
            for(int j = 0; j < cnt; j++) {
                py_Ref tmp = py_pushtmp();
                unsigned char* dst = py_newbytes(tmp, 1);
                dst[0] = buf[offset++];
                slot_data[outi] = *tmp; py_shrink(1); outi++;
            }
            continue;
        }
        for(int j = 0; j < cnt; j++) {
            if(!struct__unpack_value(code, buf, &offset, 1, slot_data, &outi, data->info.endian)) {
                py_shrink(1);
                return false;
            }
        }
    }

    *py_retval() = *header;
    py_shrink(1);
    return true;
}

static bool Struct_pack_into(int argc, py_Ref argv) {
    if(argc < 2) return TypeError("expected at least 2 arguments");
    struct__Data* data = (struct__Data*)PyObject__userdata(argv->_obj);
    int buf_size;
    unsigned char* buf = py_tobytes(py_arg(1), &buf_size);
    py_i64 off = py_toint(py_arg(2));
    if(off < 0) return TypeError("offset must be non-negative");
    if(off + data->info.size > buf_size)
        return TypeError("pack_into requires a buffer of at least %d bytes", (int)(off + data->info.size));

    int nvalues = argc - 3;
    if(nvalues != struct__count_values(&data->info))
        return TypeError("expected %d arguments for pack, got %d", struct__count_values(&data->info), nvalues);

    int offset = (int)off;
    int argi = 3;
    for(int i = 0; i < data->info.nfields; i++) {
        struct__FormatCode code = data->info.fields[i].code;
        int cnt = data->info.fields[i].count;
        if(!struct__pack_value(code, buf, &offset, cnt, argv, &argi, argc, data->info.endian))
            return false;
    }
    py_newnone(py_retval());
    return true;
}

static bool Struct_unpack_from(int argc, py_Ref argv) {
    // argc includes self
    if(argc < 2 || argc > 3) return TypeError("expected 1 or 2 arguments");
    struct__Data* data = (struct__Data*)PyObject__userdata(argv->_obj);
    py_i64 off = 0;
    if(argc == 3) { off = py_toint(py_arg(2)); if(off < 0) return TypeError("offset must be non-negative"); }

    int buf_size;
    unsigned char* buf = py_tobytes(py_arg(1), &buf_size);
    if(off + data->info.size > buf_size)
        return TypeError("unpack_from requires a buffer of at least %d bytes", (int)(off + data->info.size));

    int nvalues = struct__count_values(&data->info);
    py_Ref header = py_pushtmp();
    py_TValue* slot_data = py_newtuple(header, nvalues);
    int offset = (int)off;
    int outi = 0;

    for(int i = 0; i < data->info.nfields; i++) {
        struct__FormatCode code = data->info.fields[i].code;
        int cnt = data->info.fields[i].count;
        if(code == struct__FORMAT_PAD) { offset += cnt; continue; }
        if(code == struct__FORMAT_STRING) {
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, cnt);
            memcpy(dst, buf + offset, cnt); offset += cnt;
            slot_data[outi] = *tmp; py_shrink(1); outi++;
            continue;
        }
        if(code == struct__FORMAT_PSTRING) {
            int slen = buf[offset]; if(slen >= cnt) slen = cnt - 1;
            py_Ref tmp = py_pushtmp();
            unsigned char* dst = py_newbytes(tmp, slen);
            memcpy(dst, buf + offset + 1, slen); offset += cnt;
            slot_data[outi] = *tmp; py_shrink(1); outi++;
            continue;
        }
        if(code == struct__FORMAT_CHAR) {
            for(int j = 0; j < cnt; j++) {
                py_Ref tmp = py_pushtmp();
                unsigned char* dst = py_newbytes(tmp, 1);
                dst[0] = buf[offset++];
                slot_data[outi] = *tmp; py_shrink(1); outi++;
            }
            continue;
        }
        for(int j = 0; j < cnt; j++) {
            if(!struct__unpack_value(code, buf, &offset, 1, slot_data, &outi, data->info.endian)) {
                py_shrink(1);
                return false;
            }
        }
    }

    *py_retval() = *header;
    py_shrink(1);
    return true;
}

/* -------------------------------------------------------------------------- */
/* Module registration                                                        */
/* -------------------------------------------------------------------------- */

void pk__add_module_struct() {
    py_Ref mod = py_newmodule("struct");

    // struct.error (subclass of Exception)
    py_newtype("error", tp_Exception, mod, NULL);

    py_bindfunc(mod, "calcsize", struct_calcsize);
    py_bindfunc(mod, "pack", struct_pack);
    py_bindfunc(mod, "unpack", struct_unpack);
    py_bindfunc(mod, "pack_into", struct_pack_into);
    py_bindfunc(mod, "unpack_from", struct_unpack_from);

    // Struct class
    Struct_type = py_newtype("Struct", tp_object, mod, Struct__dtor);

    py_bindmagic(Struct_type, __new__, Struct__new__);
    py_bindmagic(Struct_type, __init__, Struct__init__);
    py_bindproperty(Struct_type, "size", Struct__size_getter, NULL);
    py_bindmethod(Struct_type, "pack", Struct_pack);
    py_bindmethod(Struct_type, "unpack", Struct_unpack);
    py_bindmethod(Struct_type, "pack_into", Struct_pack_into);
    py_bindmethod(Struct_type, "unpack_from", Struct_unpack_from);
}
