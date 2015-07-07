#!/usr/bin/env python
# -*- coding: utf-8 -*-
# py3
# un.def, 2015

try:
    import lhafile
except ImportError:
    lhafile = None
else:
    import io

supported_formats = ('YM5!', 'YM6!')

header = (
    ('file_id', 'str', 4),
    ('check_string', 'str', 8),
    ('frames', 'int', 4),
    ('attributes', 'int', 4),
    ('digidrums', 'int', 2),
    ('clock', 'int', 4),
    ('frame_freq', 'int', 2),
    ('loop_frame', 'int', 4),
    # Size, in bytes, of futur additionnal data.
    # You have to skip these bytes. (always 0 for the moment)
    (None, 'skip', 2),
    (None, 'digidrum', None),
    ('song_name', 'str', None),
    ('author', 'str', None),
    ('comment', 'str', None)
)

# Let's see the DWORD "song attribute":
# (bn represent the bit n of the DWORD)
# b0: Set if Interleaved data block.
# b1: Set if the digi-drum samples are signed data.
# b2: Set if the digidrum is already in ST 4 bits format.
# b3-b31: Not used yet, MUST BE 0.

class YMFileException(Exception):
    pass

def read_str(fo, length=None, encoding='ascii'):
    # bool(length) == False => null-terminated string
    if length:
        return fo.read(length).decode(encoding)
    buff = bytearray()
    while True:
        byte = fo.read(1)
        if not byte or byte == b'\x00':
            break
        buff.extend(byte)
    return buff.decode(encoding)

def read_int(fo, length, byteorder='big'):
    return int.from_bytes(fo.read(length), byteorder)

def load(fo):
    is_lha = fo.read(5).endswith(b'-lh')
    fo.seek(0)
    if is_lha:
        if not lhafile:
            raise YMFileException("Compressed file; install 'lhafile'")
        else:
            lha = lhafile.LhaFile(fo)
            lha_filename = lha.namelist()[0]
            fo = io.BytesIO(lha.read(lha_filename))
    ym = {}
    for field, data_type, length in header:
        if data_type == 'skip':
            fo.seek(length, 1)
            continue
        if data_type == 'digidrum':
            if ym['digidrums']:
                for i in range(ym['digidrums']):
                    sample_size = read_int(fo, 4)
                    fo.seek(sample_size, 1)
            continue
        read = read_int if data_type == 'int' else read_str
        value = read(fo, length)
        if field == 'file_id' and value not in supported_formats:
            raise YMFileException("Unsupported format: " + value)
        ym[field] = value
    interleaved = bool(ym['attributes'] & 1)
    if interleaved:
        chunks = 16
        chunk_size = ym['frames']
    else:
        chunks = ym['frames']
        chunk_size = 16
    data = []
    for i in range(chunks):
        chunk = fo.read(chunk_size)
        if len(chunk) < chunk_size:
            raise YMFileException("Truncated file")
        data.append(chunk)
    end_marker = fo.read(4)
    if len(end_marker) < 4:
        raise YMFileException("Truncated file")
    elif end_marker != b'End!':
        raise YMFileException("Wrong end marker: " + str(end_marker))
    ym['interleaved'] = interleaved
    ym['data'] = data
    ym['compressed'] = is_lha
    if is_lha:
        ym['compressed_filename'] = lha_filename
        fo.close()
    ym['length'] = round(ym['frames'] / ym['frame_freq'])
    return ym

def get_frame(data, frame, interleaved=True):
    if interleaved:
        frame_data = bytearray()
        for reg in range(16):
            frame_data.append(data[reg][frame])
        return bytes(frame_data)
    else:
        return data[frame]
