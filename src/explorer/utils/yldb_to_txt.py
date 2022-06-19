import sys
import os

from rsc8 import read_rsc8

def convert_yldb(input_file):
    with open(input_file, 'rb') as f:
        resource_id, reader = read_rsc8(f)

    assert resource_id == 5

    yldb_dict = reader.read_atmap_uint_ptr(0x50000010)

    output_file = os.path.splitext(input_file)[0] + '.txt'

    with open(output_file, 'w', encoding='utf-8') as f:
        for a, b in yldb_dict:
            value_ptr, = reader.unpack(b + 0x10, '<Q')
            value = reader.read_cstr(value_ptr)

            f.write('0x{:08X} = {}\n'.format(a, value))

if len(sys.argv) < 2:
    print('Usage: python3 yldb_to_txt.py <ytd files>')
    sys.exit(1)

for input_file in sys.argv[1:]:
    convert_yldb(input_file)
