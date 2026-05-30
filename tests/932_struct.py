import struct

def test_calcsize():
    assert struct.calcsize('b') == 1
    assert struct.calcsize('B') == 1
    assert struct.calcsize('?') == 1
    assert struct.calcsize('h') == 2
    assert struct.calcsize('H') == 2
    assert struct.calcsize('i') == 4
    assert struct.calcsize('I') == 4
    assert struct.calcsize('l') == 4
    assert struct.calcsize('L') == 4
    assert struct.calcsize('f') == 4
    assert struct.calcsize('d') == 8
    assert struct.calcsize('q') == 8
    assert struct.calcsize('Q') == 8
    assert struct.calcsize('3i') == 12
    assert struct.calcsize('bhi') == 7
    assert struct.calcsize('<i') == 4
    assert struct.calcsize('>i') == 4
    assert struct.calcsize('!i') == 4
    assert struct.calcsize('=i') == 4

def test_pack_unpack_int():
    for fmt, vals in [
        ('b', (0, -1, -128, 127)),
        ('B', (0, 255, 128, 1)),
        ('h', (0, -32768, 32767, -1)),
        ('H', (0, 65535, 32768)),
        ('i', (0, -2147483648, 2147483647, -1)),
        ('I', (0, 4294967295, 2147483648)),
        ('q', (0, -1 << 40, 1 << 40)),
        ('Q', (0, 1 << 63, (1 << 50))),
    ]:
        for v in vals:
            data = struct.pack(fmt, v)
            r, = struct.unpack(fmt, data)
            assert r == v, f'pack/unpack {fmt} {v}: got {r}'

def test_endianness():
    data_le = struct.pack('<I', 0x12345678)
    data_be = struct.pack('>I', 0x12345678)
    assert data_le != data_be
    assert struct.unpack('<I', data_le) == (0x12345678,)
    assert struct.unpack('>I', data_be) == (0x12345678,)
    assert struct.unpack('!I', data_be) == (0x12345678,)

def test_float():
    data = struct.pack('f', 3.14)
    v, = struct.unpack('f', data)
    assert abs(v - 3.14) < 0.001

    data = struct.pack('d', 3.14159265358979)
    v, = struct.unpack('d', data)
    assert abs(v - 3.14159265358979) < 1e-14

def test_bool():
    data = struct.pack('?', True)
    assert struct.unpack('?', data) == (True,)
    data = struct.pack('?', False)
    assert struct.unpack('?', data) == (False,)

def test_string():
    data = struct.pack('4s', b'test')
    assert struct.unpack('4s', data) == (b'test',)
    data = struct.pack('10s', b'hi')
    assert struct.unpack('10s', data) == (b'hi\x00\x00\x00\x00\x00\x00\x00\x00',)
    data = struct.pack('3s', b'hello')
    assert struct.unpack('3s', data) == (b'hel',)

def test_padding():
    data = struct.pack('bxb', 1, 2)
    a, b = struct.unpack('bxb', data)
    assert a == 1 and b == 2
    assert len(data) == 3

def test_mixed():
    data = struct.pack('bB?hH', -1, 255, True, -100, 50000)
    vals = struct.unpack('bB?hH', data)
    assert vals == (-1, 255, True, -100, 50000)

def test_repeated():
    data = struct.pack('3i', 10, 20, 30)
    assert struct.unpack('3i', data) == (10, 20, 30)
    data = struct.pack('2h', 100, 200)
    assert struct.unpack('2h', data) == (100, 200)

def test_unpack_from():
    data = struct.pack('4i', 1, 2, 3, 4)
    a, b, c = struct.unpack_from('3i', data)
    assert (a, b, c) == (1, 2, 3)
    c, = struct.unpack_from('i', data, 4)
    assert c == 2
    d, = struct.unpack_from('i', data, 12)
    assert d == 4

def test_pack_into():
    buf = struct.pack('4i', 0, 0, 0, 0)
    struct.pack_into('i', buf, 4, 42)
    vals = struct.unpack('4i', buf)
    assert vals == (0, 42, 0, 0)
    struct.pack_into('h', buf, 0, -123)
    a, = struct.unpack('h', buf[:2])
    assert a == -123

def test_struct_class():
    st = struct.Struct('ii')
    assert st.size == 8
    data = st.pack(100, 200)
    assert st.unpack(data) == (100, 200)

    st2 = struct.Struct('>I')
    data = st2.pack(0x12345678)
    assert st2.unpack(data) == (0x12345678,)
    assert data == b'\x12\x34\x56\x78'

    st3 = struct.Struct('<I')
    data = st3.pack(0x12345678)
    assert data == b'\x78\x56\x34\x12'

def test_struct_pack_into():
    buf = struct.pack('3i', 0, 0, 0)
    st = struct.Struct('i')
    st.pack_into(buf, 4, 99)
    assert struct.unpack('3i', buf) == (0, 99, 0)

def test_struct_unpack_from():
    data = struct.pack('III', 1, 2, 3)
    st = struct.Struct('I')
    assert st.unpack_from(data, 0) == (1,)
    assert st.unpack_from(data, 4) == (2,)
    assert st.unpack_from(data, 8) == (3,)

def test_error_type():
    assert issubclass(struct.error, Exception)

def test_network_order():
    data = struct.pack('!i', 0x01020304)
    assert data == b'\x01\x02\x03\x04'
    assert struct.unpack('!i', data) == (0x01020304,)

def test_negative():
    data = struct.pack('b', -128)
    assert struct.unpack('b', data) == (-128,)
    data = struct.pack('h', -32768)
    assert struct.unpack('h', data) == (-32768,)
    data = struct.pack('i', -2147483648)
    assert struct.unpack('i', data) == (-2147483648,)

def test_unsigned_max():
    data = struct.pack('I', 4294967295)
    assert struct.unpack('I', data) == (4294967295,)

def test_char():
    data = struct.pack('3c', b'a', b'b', b'c')
    assert struct.unpack('3c', data) == (b'a', b'b', b'c')

def test_struct_reuse():
    st = struct.Struct('2h')
    data1 = st.pack(10, 20)
    data2 = st.pack(30, 40)
    assert st.unpack(data1) == (10, 20)
    assert st.unpack(data2) == (30, 40)

test_calcsize()
test_pack_unpack_int()
test_endianness()
test_float()
test_bool()
test_string()
test_padding()
test_mixed()
test_repeated()
test_unpack_from()
test_pack_into()
test_struct_class()
test_struct_pack_into()
test_struct_unpack_from()
test_error_type()
test_network_order()
test_negative()
test_unsigned_max()
test_char()
test_struct_reuse()

print('ALL STRUCT TESTS PASSED')
