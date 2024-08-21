import struct
import random
import enum
import socket

CURRENT_REQUEST_VERSION = 1
MAX_REQUEST_DATA_SIZE = 32
MAX_PACKET_SIZE = 48

class RequestType(enum.IntEnum):
    ReadMemory = 1,
    WriteMemory = 2

CITRA_PORT = 45987

class Citra:
    def __init__(self, address="127.0.0.1", port=CITRA_PORT):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.address = address

    def is_connected(self):
        return self.socket is not None

    def _generate_header(self, request_type, data_size):
        request_id = random.getrandbits(32)
        return (struct.pack("IIII", CURRENT_REQUEST_VERSION, request_id, request_type, data_size), request_id)

    def _read_and_validate_header(self, raw_reply, expected_id, expected_type):
        reply_version, reply_id, reply_type, reply_data_size = struct.unpack("IIII", raw_reply[:4*4])
        if (CURRENT_REQUEST_VERSION == reply_version and
            expected_id == reply_id and
            expected_type == reply_type and
            reply_data_size == len(raw_reply[4*4:])):
            return raw_reply[4*4:]
        return None

    def read_memory(self, read_address, read_size):
        """
        >>> c.read_memory(0x100000, 4)
        b'\\x07\\x00\\x00\\xeb'
        """
        result = bytes()
        while read_size > 0:
            temp_read_size = min(read_size, MAX_REQUEST_DATA_SIZE)
            request_data = struct.pack("II", read_address, temp_read_size)
            request, request_id = self._generate_header(RequestType.ReadMemory, len(request_data))
            request += request_data
            self.socket.sendto(request, (self.address, CITRA_PORT))

            raw_reply = self.socket.recv(MAX_PACKET_SIZE)
            reply_data = self._read_and_validate_header(raw_reply, request_id, RequestType.ReadMemory)

            if reply_data:
                result += reply_data
                read_size -= len(reply_data)
                read_address += len(reply_data)
            else:
                return None

        return result

    def write_memory(self, write_address, write_contents):
        """
        >>> c.write_memory(0x100000, b"\\xff\\xff\\xff\\xff")
        True
        >>> c.read_memory(0x100000, 4)
        b'\\xff\\xff\\xff\\xff'
        >>> c.write_memory(0x100000, b"\\x07\\x00\\x00\\xeb")
        True
        >>> c.read_memory(0x100000, 4)
        b'\\x07\\x00\\x00\\xeb'
        """
        write_size = len(write_contents)
        while write_size > 0:
            temp_write_size = min(write_size, MAX_REQUEST_DATA_SIZE - 8)
            request_data = struct.pack("II", write_address, temp_write_size)
            request_data += write_contents[:temp_write_size]
            request, request_id = self._generate_header(RequestType.WriteMemory, len(request_data))
            request += request_data
            self.socket.sendto(request, (self.address, CITRA_PORT))

            raw_reply = self.socket.recv(MAX_PACKET_SIZE)
            reply_data = self._read_and_validate_header(raw_reply, request_id, RequestType.WriteMemory)

            if None != reply_data:
                write_address += temp_write_size
                write_size -= temp_write_size
                write_contents = write_contents[temp_write_size:]
            else:
                return False
        return True

if "__main__" == __name__:
    import doctest
    doctest.testmod(extraglobs={'c': Citra()})
