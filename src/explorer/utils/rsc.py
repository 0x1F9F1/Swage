import struct

def read_rsc8(file):
    magic, flags, virt_flags, phys_flags = struct.unpack('<IIII', file.read(0x10))

    assert magic == 0x38435352

    resource_id = flags & 0xFF
    compresor_id = ((flags >> 8) + 1) & 0x1F
    is_signature_protected = (flags >> 15) & 0x1
    encryption_key_id = (flags >> 16) - 1

    resource_id2 = (phys_flags & 0xF) | (virt_flags & 0xF) << 4

    assert is_signature_protected == 0
    assert encryption_key_id == 0xFF
    assert compresor_id == 0
    assert resource_id == resource_id2

    virt_size = virt_flags & 0xFFFFFFF0
    phys_size = phys_flags & 0xFFFFFFF0

    virt_offset = 0
    phys_offset = virt_offset + virt_size

    rsc_chunks = [
        ResourceChunk(virt_offset, 0x50000000, virt_size),
        ResourceChunk(phys_offset, 0x60000000, phys_size),
    ]

    rsc_data = file.read(virt_size + phys_size)

    return resource_id, ResourceReader(rsc_data, rsc_chunks)


def rsc7_get_chunks(paddr, vaddr, flags, chunk_size):
    chunk_limits = [0x1,0x3,0xF,0x3F,0x7F,0x1,0x1,0x1,0x1]
    chunk_shifts = [  4,  5,  7,  11,  17, 24, 25, 26, 27]

    chunk_size <<= (flags & 0xF) + 4
    chunks = []
    offset = 0

    for i in range(9):
        chunk_count = (flags >> chunk_shifts[i]) & chunk_limits[i]

        for j in range(chunk_count):
            chunks.append(ResourceChunk(paddr + offset, vaddr + offset, chunk_size))
            offset += chunk_size

        chunk_size >>= 1

    return chunks, offset

def read_rsc7(file):
    magic, flags, virt_flags, phys_flags = struct.unpack('<IIII', file.read(0x10))

    assert magic == 0x37435352
    resource_id = flags & 0xFF

    virt_offset = 0
    virt_chunks, virt_size = rsc7_get_chunks(virt_offset, 0x50000000, virt_flags, 0x2000)

    phys_offset = virt_offset + virt_size
    phys_chunks, phys_size = rsc7_get_chunks(phys_offset, 0x60000000, phys_flags, 0x2000)

    rsc_chunks = virt_chunks + phys_chunks
    rsc_data = file.read(virt_size + phys_size)

    return resource_id, ResourceReader(rsc_data, rsc_chunks)

class ResourceChunk:
    def __init__(self, offset, address, size):
        self.offset = offset
        self.address = address
        self.size = size

class ResourceReader:
    def __init__(self, data, chunks):
        self.chunks = chunks
        self.data = data

    def get_offset(self, address, size):
        for chunk in self.chunks:
            if (address >= chunk.address) and (address + size <= chunk.address + chunk.size):
                return chunk.offset + (address - chunk.address)

        return None

    def read(self, address, size):
        offset = self.get_offset(address, size)

        if offset is None:
            return None

        return self.data[offset:offset+size]

    def unpack(self, address, fmt):
        return struct.unpack(fmt, self.read(address, struct.calcsize(fmt)))

    def read_atarray(self, address):
        return self.unpack(address, '<QHH')

    def read_pgdict(self, address):
        _vtbl, _prsc, _pnext, _nrefs = self.unpack(address, '<QQQI')

        p_keys,     keys_count,   keys_capacity = self.read_atarray(address + 0x20)
        p_values, values_count, values_capacity = self.read_atarray(address + 0x30)

        assert keys_count == values_count

        keys = self.unpack(p_keys, '<{}I'.format(keys_count))
        values = self.unpack(p_values, '<{}Q'.format(values_count))

        return list(zip(keys, values))

    def read_atmap_uint_ptr(self, address):
        p_buckets, bucket_count, _, value_count = self.unpack(address, '<QIII')

        results = []

        buckets = self.unpack(p_buckets, '<{}Q'.format(bucket_count))

        for current in buckets:
            while current != 0:
                key_u32, value_ptr, current = self.unpack(current, '<I4xQQ')
                results.append((key_u32, value_ptr))

        return results

    def read_cstr(self, address):
        result = bytearray()

        for i in range(1024):
            v = self.read(address + i, 1)

            if v[0] == 0:
                break

            result += v

        return result.decode('utf8')
