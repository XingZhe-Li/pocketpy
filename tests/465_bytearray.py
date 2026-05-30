# bytearray tests

# Basic constructors
assert type(bytearray()) == bytearray
assert len(bytearray()) == 0
assert len(bytearray(5)) == 5
assert bytearray(3) == bytearray(b'\x00\x00\x00')

assert bytearray() == bytearray(b'')
assert bytearray([65, 66, 67]) == bytearray(b'ABC')
assert bytearray(b'abc') == bytearray(b'abc')
ba = bytearray(b'xyz')
assert bytearray(ba) == ba

# repr
r = repr(bytearray(b'test'))
assert r == "bytearray(b'test')", r
assert repr(bytearray()) == "bytearray(b'')"

# len
assert len(bytearray(b'hello')) == 5
assert len(bytearray()) == 0

# bool
assert bytearray(b'\x00')
assert not bytearray()

# getitem
ba = bytearray(b'hello')
assert ba[0] == ord('h')
assert ba[1] == ord('e')
assert ba[-1] == ord('o')
assert ba[:2] == bytearray(b'he')
assert ba[1:4] == bytearray(b'ell')
assert ba[::-1] == bytearray(b'olleh')

# setitem
ba = bytearray(b'test')
ba[0] = 70
assert ba == bytearray(b'Fest')
ba[-1] = 33
assert ba == bytearray(b'Fes!')

# delitem
ba = bytearray(b'abcde')
del ba[0]
assert ba == bytearray(b'bcde')
del ba[-1]
assert ba == bytearray(b'bcd')

# delitem slice
ba = bytearray(b'hello world')
del ba[0:6]
assert ba == bytearray(b'world')

# add
assert bytearray(b'hello ') + bytearray(b'world') == bytearray(b'hello world')
assert bytearray(b'hello ') + b'world' == bytearray(b'hello world')

# contains
ba = bytearray(b'hello')
assert 104 in ba
assert 120 not in ba

# append
ba = bytearray(b'hello')
ba.append(33)
assert ba == bytearray(b'hello!')

# extend
ba = bytearray(b'hello')
ba.extend(b' world')
assert ba == bytearray(b'hello world')

ba2 = bytearray(b'!!!')
ba.extend(ba2)
assert ba == bytearray(b'hello world!!!')

ba3 = bytearray(b'ABC')
ba3.extend([68, 69, 70])
assert ba3 == bytearray(b'ABCDEF')

# insert
ba = bytearray(b'ac')
ba.insert(1, 98)
assert ba == bytearray(b'abc')

# pop
ba = bytearray(b'hello')
last = ba.pop()
assert last == ord('o')
assert ba == bytearray(b'hell')

second = ba.pop(1)
assert second == ord('e')
assert ba == bytearray(b'hll')

# pop from single element
ba = bytearray(b'x')
val = ba.pop()
assert val == 120
assert len(ba) == 0

# remove
ba = bytearray(b'hello')
ba.remove(108)
assert ba == bytearray(b'helo')

# reverse
ba = bytearray(b'hello')
ba.reverse()
assert ba == bytearray(b'olleh')

# decode
ba = bytearray(b'hello world')
assert ba.decode() == 'hello world'

# copy
ba = bytearray(b'hello')
ba2 = ba.copy()
assert ba == ba2
ba2[0] = 70
assert ba[0] == ord('h')

# clear
ba = bytearray(b'hello')
ba.clear()
assert len(ba) == 0

# hex
ba = bytearray(3)
ba[0] = 0
ba[1] = 1
ba[2] = 255
assert ba.hex() == '0001ff'
assert bytearray(b'\x00\x00').hex() == '0000'
assert bytearray(b'\xff').hex() == 'ff'

# find
ba = bytearray(b'hello world')
assert ba.find(b'world') == 6
assert ba.find(b'xyz') == -1
assert ba.find(b'l') == 2
assert ba.find(b'') == 0

# comparison (bytearray vs bytes)
assert bytearray(b'hello') == b'hello'
assert bytearray(b'hello') != b'world'
assert bytearray(b'hello') == bytearray(b'hello')
assert bytearray(b'hello') != bytearray(b'world')

# __add__ with bytes returns bytearray
result = bytearray(b'hello ') + b'world'
assert type(result) == bytearray
assert result == bytearray(b'hello world')

# repr empty
assert repr(bytearray()) == "bytearray(b'')"

# test large extend (stress test for capacity growth)
ba = bytearray(b'\x00')
for i in range(100):
    ba.append(i)
assert len(ba) == 101
assert ba[0] == 0
assert ba[50] == 49
assert ba[100] == 99

# iteration
result = []
for b in bytearray(b'ABC'):
    result.append(b)
assert result == [65, 66, 67]

# list comprehension
result = [b for b in bytearray(b'\x01\x02\x03')]
assert result == [1, 2, 3]

# iteration over empty
count = 0
for b in bytearray():
    count += 1
assert count == 0

# iteration after mutation
ba = bytearray(b'hello')
ba[0] = 72  # 'H'
result = [b for b in ba]
assert result == [72, 101, 108, 108, 111]

# iterator from iter()
it = iter(bytearray(b'XY'))
assert next(it) == 88
assert next(it) == 89
