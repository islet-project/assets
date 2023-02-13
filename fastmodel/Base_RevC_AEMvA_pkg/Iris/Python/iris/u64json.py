"""
U64Json encoder/decoder.

This module provides a similar API to the Python json module to encode/decode U64Json data.
"""

import array
import functools
import numbers
import struct
import sys

from . import error

U64_MAX = (2**64) - 1
S64_MAX = (2**63) - 1
S64_MIN = -(2**63)

# This value is defined to be invalid.
INVALID = 0xc9ffffffffffffff

# This module needs a container to efficiently hold 64-bit integers.
# array.array() is ideal but it does not support 64-bit arrays on all platforms/version.
# Before Python 3.3 array.array does not support the typecode 'Q' (unsigned long long)
# but it does have 'L' (unsigned long) which is 64-bit on some platforms.

def _has_typecode_Q():
    try:
        array.array('Q')
    except ValueError:
        # No typecode 'Q'
        return False
    else:
        return True

def _typecode_L_is_64bit():
    return array.array('L').itemsize == 8

if _has_typecode_Q():
    _U64Array = functools.partial(array.array, 'Q')
elif _typecode_L_is_64bit():
    _U64Array = functools.partial(array.array, 'L')
else:
    # Fallback to using a wrapper around bytearray. This should work on any system with Python 2.7+ but
    # will be a little less efficient.
    class _U64Array(object):
        def __init__(self, initial_data=None):
            if initial_data is not None and len(initial_data) > 0:
                self.data = bytearray(struct.pack('Q' * len(initial_data), *initial_data))
            else:
                self.data = bytearray()

            self.itemsize = 8

        def _get_u8_indices(self, key):
            """Interpret a __get/setitem__ key"""

            if isinstance(key, numbers.Integral):
                start = key * self.itemsize
                stop = (key + 1) * self.itemsize
                u64_size = 1
                value_is_array = False
            elif isinstance(key, slice):
                start, stop, step = key.indices(len(self))
                u64_size = stop - start
                start *= self.itemsize
                stop *= self.itemsize
                value_is_array = True
                if step != 1:
                    raise KeyError('_U64Array does not support strides other than 1')
            else:
                raise KeyError('Invalid key of type {}'.format(type(key).__name__))

            return start, stop, u64_size, value_is_array

        def __getitem__(self, key):
            start, stop, u64_size, value_is_array = self._get_u8_indices(key)
            val_buffer = self.data[start : stop]

            u64_data = struct.unpack('Q' * u64_size, val_buffer)
            if value_is_array:
                return list(u64_data)
            else:
                return u64_data[0]

        def __setitem__(self, key, u64_data):
            start, stop, u64_size, value_is_array = self._get_u8_indices(key)
            if value_is_array:
                u8_data = bytearray(struct.pack('Q' * u64_size, *u64_data))
            else:
                u8_data = bytearray(struct.pack('Q' * u64_size, u64_data))
            self.data[start : stop] = u8_data

        def __len__(self):
            """Return the length of this structure in 64-bit elements."""
            assert(len(self.data) % self.itemsize == 0)
            return len(self.data) // self.itemsize

        def append(self, item):
            self.data += bytearray(struct.pack('Q', item))

        def extend(self, u64_data):
            self.data += bytearray(struct.pack('Q' * len(u64_data), *u64_data))

        def tolist(self):
            return list(struct.unpack('Q'* len(self), self.data))

        def tobytes(self):
            return str(self.data)

        def frombytes(self, data):
            if len(data) % self.itemsize != 0:
                raise ValueError('bytes length not a multiple of item size')
            self.data += bytearray(data)

        def fromstring(self, data):
            self.frombytes(data)


if sys.version_info < (3, 0):
    def _int_to_chr(i):
        """Convert an integer into a byte character."""
        return chr(i)

    # Convert a character to an integer.
    # Indexing a bytes (str) object returns a str so ord will convert it to an integer.
    # b'x'[0] -> str
    _byte_val = ord

else:
    # In Python str replaces unicode and unicode is removed.
    unicode = str

    def _int_to_chr(i):
        """Convert an integer into a byte character."""
        return bytes((i,))

    # Indexing a bytes object already returns an integer in Python3 so _byte_val() is a NOP.
    # b'x'[0] -> int
    _byte_val = lambda x: x

def _tobytes(arr):
    """Convert an array object into a bytes string."""
    if hasattr(arr, 'tobytes'):
        return arr.tobytes()
    elif hasattr(arr, 'tostring'):
        return arr.tostring()
    else:
        raise TypeError('{} object does not have tobytes() or tostring() methods'.format(type(arr).__name__))

def _frombytes(arr, data):
    """Append a bytes string to an array."""
    if hasattr(arr, 'frombytes'):
        return arr.frombytes(data)
    elif hasattr(arr, 'fromstring'):
        return arr.fromstring(data)
    else:
        raise TypeError('{} object does not have frombytes() or fromstring() methods'.format(type(arr).__name__))

def _is_numberu64(value):
    """Return True if value can be represented as a NumberU64."""
    return (isinstance(value, numbers.Integral)
            and not isinstance(value, bool) # bool is considered Integral so must be filtered out.
            and 0 <= value <= U64_MAX)

def _is_valid_request(obj):
    """Return True iff obj is a valid JSON-RPC 2.0 request or notification."""
    if not isinstance(obj, dict):
        return False

    # Check that the mandatory fields are present
    if 'jsonrpc' not in obj or 'method' not in obj:
        return False

    # Now check that all the fields present valid.
    for key, value in obj.items():
        if key == 'jsonrpc':
            if value != '2.0':
                return False
        elif key == 'method':
            if not isinstance(value, (str, bytes)):
                return False
        elif key == 'params':
            if not isinstance(value, dict):
                return False
            if not _is_numberu64(value.get('instId', 0)):
                return False
        elif key == 'id':
            if not _is_numberu64(value):
                return False
        else:
            # No other members are allowed so this is not a valid request.
            return False

    return True

def _is_valid_response(obj):
    """Return True iff obj is a valid JSON-RPC 2.0 response."""
    if not isinstance(obj, dict):
        return False

    # Check that the mandatory fields are present
    if 'jsonrpc' not in obj or 'id' not in obj or ('result' not in obj and 'error' not in obj):
        return False

    # Make sure we have exactly one of 'result' and 'error' fields, not both.
    if 'result' in obj and 'error' in obj:
        return False

    # Now check that all the fields present valid.
    for key, value in obj.items():
        if key == 'jsonrpc':
            if value != '2.0':
                return False
        elif key == 'id':
            if not _is_numberu64(value):
                return False
        elif key == 'result':
            pass # result can be any type
        elif key == 'error':
            if not isinstance(obj, dict):
                return False

            if 'code' not in value or 'message' not in value:
                return False

            for key, value in value.items():
                if key == 'code':
                    if not isinstance(value, numbers.Integral):
                        return False
                elif key == 'message':
                    if not isinstance(value, (str, bytes)):
                        return False
                elif key == 'data':
                    pass # data can be any type
                else:
                    # No other members are allowed so this is not a valid error object.
                    return False
        else:
            # No other members are allowed so this is not a valid response.
            return False

    return True

def _is_valid_message(obj):
    """Return True iff obj is a valid JSON-RPC 2.0 request, notification, or response."""
    return _is_valid_request(obj) or _is_valid_response(obj)


def _encode_recursive(value, buffer):
    """Encode an value in U64Json into the supplied U64 buffer."""

    if isinstance(value, bool):
        if value:
            buffer.append(0xcf00000000000000)
        else:
            buffer.append(0xce00000000000000)

    elif isinstance(value, numbers.Integral):
        msb4 = (value & 0xffffffffffffffff) >> 60
        if 0 <= value <= U64_MAX:
            # NumberU64
            if msb4 in (0, 0xF):
                # Use 0x0/0xF encoding
                buffer.append(value)
            else:
                buffer.extend([0xc000000000000000, value])
        elif S64_MIN <= value <= S64_MAX:
            # NumberS64
            if msb4 == 0xF:
                # Use 0x1 encoding
                buffer.append(value & 0x1fffffffffffffff)
            else:
                buffer.extend([0xc100000000000000, value & 0xffffffffffffffff])
        else:
            raise ValueError('Value {} is out of range for U64Json'.format(value))

    elif value is None:
        buffer.append(0xcd00000000000000)

    elif isinstance(value, bytes): # This als handles 'str'.
        str_len = len(value)

        if str_len < 7 or (str_len <= 255 and 0x20 <= _byte_val(value[6]) < 0x7f):
            # The string meets the requirements to be encoded as a short string
            parts = [_int_to_chr(str_len), value]

            # All elements need to be 64-bit so we need to pad the string out to be a multiple
            # of 8 bytes in length.
            u64_length = (str_len + 1 + 7) // 8
            u8_length = u64_length * 8
            n_pad_bytes = u8_length - (str_len + 1)
            assert(n_pad_bytes < 8)
            if str_len < 7:
                # Very short strings must set the MSB to 0x20 to indicate that this is a string.
                parts.append(b'\0' * (n_pad_bytes - 1))
                parts.append(b'\x20')
            else:
                parts.append(b'\0' * n_pad_bytes)

            # Join the string parts together and add them to the buffer as U64 elements
            _frombytes(buffer, b''.join(parts))
        else:
            # String cannot be encoded as a short string, we must use the long string format
            buffer.append(0xcc00000000000000 + str_len)

            # Pad the string out to a multiple of 8
            n_pad_bytes = 8 - ((str_len) % 8)
            pad_bytes = b'\0' * n_pad_bytes
            _frombytes(buffer, value + pad_bytes)

    elif isinstance(value, unicode):
        _encode_recursive(unicode.encode(value), buffer)

    elif isinstance(value, float):
        fp_bitpattern = struct.unpack('Q', struct.pack('d', value))[0]
        buffer.extend([0xca00000000000000, fp_bitpattern])

    elif isinstance(value, (list, tuple, set)):
        # NOTE: sets are converted to arrays
        # There are two ways to encode an array. A general encoding that can be used for
        # any sequence of value and an optimised encoding for an array which only contains
        # NumberU64 values. We need to check the type of all the values before we can decide
        # which encoding to use.
        is_u64_array = all(_is_numberu64(e) for e in value) and len(value) > 0

        if is_u64_array:
            buffer.append(0x8000000000000000 + len(value))
            buffer.extend(value)

        else:
            # General Array format is:
            # uint64_t header = 0xaxxx xxxx xxxx xxxx; // xx... is the container length
            # uint64_t array_length
            # Value elements[array_length]

            buffer.append(INVALID) # This value needs to be replaced with the
                                   # container length but we won't know how long it
                                   # is until the rest has been encoded.
            array_head_offset = len(buffer) - 1

            # Add the array length.
            buffer.append(len(value))

            # Now encode all the elements of the array as appropriate for their type.
            for element in value:
                _encode_recursive(element, buffer)

            # Patch the container length back in.
            container_length = len(buffer) - array_head_offset
            buffer[array_head_offset] = 0xa000000000000000 + container_length

    elif isinstance(value, dict):
        buffer.append(INVALID) # This value needs to be replaced with the
                               # container length but we won't know how long it
                               # is until the rest has been encoded.
        head_offset = len(buffer) - 1

        # A dict could either be a generic object or it could be a message object
        # which uses a special optimised encoding.
        if _is_valid_message(value):
            # _is_valid_message() checks that all necessary fields are present and all
            # fields have the correct type so it is safe to assume here that value is a
            # sane message.
            is_request = 'method' in value

            id = value.get('id', None)
            if is_request:
                # Request or Notification
                if id is not None:
                    head = 0xe020000000000000
                else:
                    head = 0xe120000000000000
            else:
                # Response
                if 'error' in value:
                    error = value['error']

                head = 0xe220000000000000

            if id is None:
                buffer.append(0xffffffffffffffff)
            else:
                buffer.append(id)

            # Find the destination instId.
            if 'params' in value:
                dest = value['params'].get('instId', 0)
            elif 'method' in value:
                # A request with no params object. instId is implicitly 0 by default
                dest = 0
            else:
                # This must be a response. The destination is encoded by the upper 32 bits of
                # the id.
                assert(id is not None)
                dest = id >> 32

            buffer.append(dest)

            if is_request:
                # Request or notification
                _encode_recursive(value['method'], buffer)
                _encode_recursive(value.get('params', {}), buffer)
            else:
                # Response
                is_error = 'error' in value
                if is_error:
                    error_code = value['error']['code'] & 0xffffffffffffffff
                else:
                    error_code = 0

                buffer.append(error_code)

                if is_error:
                    _encode_recursive(value['error']['message'], buffer)
                    if 'data' in value['error']:
                        _encode_recursive(value['error']['data'], buffer)
                else:
                    _encode_recursive(value['result'], buffer)

        else:
            # Generic Object format is:
            # uint64_t header = 0xbxxx xxxx xxxx xxxx; // xx... is the container length
            # uint64_t number_of_members
            # struct { String member_name; Value value; ] members[number_of_members];
            head = 0xb000000000000000

            # Add the member count
            buffer.append(len(value))

            # Add the members in alphabetical order
            for key in sorted(value):
                if not isinstance(key, (str, bytes, unicode)):
                    raise TypeError('Object member names must be strings', key)
                _encode_recursive(key, buffer)
                _encode_recursive(value[key], buffer)

        # Patch the container length back in.
        container_length = len(buffer) - head_offset
        buffer[head_offset] = head + container_length


    else:
        raise TypeError('Object cannot be serialized to U64Json: {!r}'.format(value))


def _encode(value):
    """Encode an value in U64Json and return a uint64 array"""
    buffer = _U64Array([])
    assert(buffer.itemsize == 8)
    _encode_recursive(value, buffer)

    if sys.byteorder == 'big':
        # U64Json data is always little-endian so it must be byteswapped on big-endian systems.
        buffer.byteswap()

    return buffer

def _u64array_to_string(buffer, start_byte_offset, length_in_bytes):
    """
    Extract bytes [start_byte_offset : start_byte_offset + length_in_bytes] from a sequence
    of 64-bit integers.
    """
    if isinstance(buffer, array.array) and buffer.itemsize == 8:
        return _tobytes(buffer)[start_byte_offset : start_byte_offset + length_in_bytes]
    elif isinstance(buffer, list):
        return struct.pack('Q' * len(buffer), *buffer)[start_byte_offset : start_byte_offset + length_in_bytes]
    else:
        raise TypeError('Cannot convert {} to a string'.format(type(buffer).__name__))

def _u64array_to_bytes(buffer, length):
    return b''.join(struct.pack('<Q', b) for b in buffer)[:length]

def _decode_recursive(buffer, offset=0):
    """Decode a uint64 array into Python values. Returns (decoded_value, elements_read)"""
    head = buffer[offset]
    msb4 = head >> 60

    if msb4 in (0x0, 0xf): # +ve Number
        return (head, 1)

    elif msb4 == 0x1: # -ve Number
        return (struct.unpack('q', struct.pack('Q', head | 0xf000000000000000))[0], 1)

    elif 0x2 <= msb4 < 0x8: # Short string
        str_len = head & 0xff
        container_len = (str_len >> 3) + 1
        s = _u64array_to_string(buffer[offset : offset + container_len], 1, str_len)
        return (s.decode('utf-8'), container_len)

    elif msb4 == 0x8: # NumberU64[]
        arr_len = head & 0x0fffffffffffffff
        offset += 1
        return (list(buffer[offset : offset + arr_len]), arr_len + 1)

    elif msb4 == 0xa: # Value[] (Generic array)
        container_len = head & 0x0fffffffffffffff
        arr_len = buffer[offset + 1]
        arr = []
        offset += 2
        for i in range(arr_len):
            val, size = _decode_recursive(buffer, offset)
            arr.append(val)
            offset += size
        return (arr, container_len)

    elif msb4 == 0xb: # Object
        container_len = head & 0x0fffffffffffffff
        member_count = buffer[offset + 1]
        obj = {}
        offset += 2
        for i in range(member_count):
            member, size = _decode_recursive(buffer, offset)
            offset += size
            value, size = _decode_recursive(buffer, offset)
            offset += size
            obj[member] = value
        return (obj, container_len)

    elif msb4 == 0xc:
        msb8 = head >> 56
        if msb8 == 0xc0: # NumberU64
            return (buffer[offset + 1], 2)
        elif msb8 == 0xc1: # NumberS64
            return (struct.unpack('q', struct.pack('Q', buffer[offset + 1]))[0], 2)
        elif msb8 == 0xca: # Floating-point number
            return (struct.unpack('d', struct.pack('Q', buffer[offset + 1]))[0], 2)
        elif msb8 == 0xcb: # ByteArray
            array_length = head & 0x00ffffffffffffff
            return (_u64array_to_bytes(buffer[offset + 1:], array_length), (array_length + 15) >> 3)
        elif msb8 == 0xcc: # Long string
            str_len = head & 0x00ffffffffffffff
            s = _u64array_to_string(buffer[offset + 1:], 0, str_len)
            return (s.decode('utf-8'), (str_len + 15 ) >> 3)
        elif msb8 == 0xcd:
            return (None, 1)
        elif msb8 == 0xce:
            return (False, 1)
        elif msb8 == 0xcf:
            return (True, 1)
        else:
            raise error.IrisError(error.E_u64json_encoding_error, 'Invalid U64Json MSB 0x{:x}'.format(msb8))

    elif msb4 == 0xe: # Message
        msb8 = head >> 56
        version = (head >> 48) & 0xff
        if version != 0x20:
            raise error.IrisError(error.E_u64json_encoding_error, 'Invalid U64Json message version')
        container_len = head & 0x0000ffffffffffff

        id = buffer[offset + 1]
        instId = buffer[offset + 2]
        offset += 3
        msg = {
            'jsonrpc': '2.0',
        }
        if msb8 in (0xe0, 0xe1): # Request or Notification
            method, size = _decode_recursive(buffer, offset)
            offset += size
            params, size = _decode_recursive(buffer, offset)
            offset += size
            msg['method'] = method
            msg['params'] = params
            if msb8 == 0xe0:
                msg['id'] = id
        elif msb8 == 0xe2: # Response
            error_code = struct.unpack('q', struct.pack('Q', buffer[offset]))[0]
            offset += 1
            msg['id'] = id
            if error_code == 0:
                # Result response
                result, size = _decode_recursive(buffer, offset)
                offset += size
                msg['result'] = result
            else:
                # Error response
                message, size = _decode_recursive(buffer, offset)
                offset += size
                msg['error'] = {
                    'code': error_code,
                    'message': message,
                }
                if offset < container_len:
                    data, size = _decode_recursive(buffer, offset)
                    offset += size
                    msg['error']['data'] = data
        else:
            raise error.IrisError(error.E_u64json_encoding_error, 'Invalid U64Json MSB 0x{:x}'.format(msb8))
        return (msg, container_len)

    raise error.IrisError(error.E_u64json_encoding_error, 'Invalid U64Json MSB 0x{:x}'.format(msb4))

def _decode(buffer):
    """Decode U64Json value from a U64 array buffer."""

    if sys.byteorder == 'big':
        # U64Json data is always little-endian so it must be byteswapped on big-endian systems.
        buffer = _U64Array(buffer) # Avoid modifying the caller's buffer by making a local copy
        assert(buffer.itemsize == 8)
        buffer.byteswap()

    value, size = _decode_recursive(buffer)
    return value

#  Public APIs. These mirror other serialization/deserialization APIs in the Python standard library like json and pickle.

def dump(obj, fp):
    """Serialize obj to U64Json and write the stream to a file-like object."""
    s = dumps(obj)
    fp.write(s)

def dumps(obj):
    """Serialize obj to U64Json and return the encoded data as a string."""
    return _tobytes(_encode(obj))

def load(fp):
    """Read U64Json encoded data from fp and deserialize it. Returns the deserialized value."""
    return loads(fp.read())

def loads(s):
    """Read U64Json encoded data from string s and deserialize it. Returns the deserialized value."""
    buffer = _U64Array()
    assert(buffer.itemsize == 8)
    _frombytes(buffer, s)
    return _decode(buffer)
