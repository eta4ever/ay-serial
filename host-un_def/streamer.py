#!/usr/bin/env python
# -*- coding: utf-8 -*-
# py3
# un.def, 2015

import sys
import time
import math
import argparse
import serial
import ymfile

parser = argparse.ArgumentParser(description='AY-serial streamer')
parser.add_argument(    '-p', '--port',
                        required=True,
                        metavar='port',
                        help='serial port device (required)')
parser.add_argument(    '-b', '--baudrate',
                        type=int,
                        default=38400,
                        metavar='bd',
                        help='serial port baud rate (default: 38400)')
parser.add_argument(    '-u', '--unbuf',
                        action='store_true',
                        help='unbuffered mode')
parser.add_argument(    '-v', '--verbose',
                        action='store_true',
                        help='show all file info (as dict)')
parser.add_argument(    'file',
                        help='.ym file')
args = parser.parse_args()

silence = bytearray(16)

def wait_request(conn):
    while True:
        request = conn.read()
        if request == b'\x08':
            return


with open(args.file, 'rb') as f:
    try:
        ym = ymfile.load(f)
    except ymfile.YMFileException as e:
        print("Error:", e)
        sys.exit(1)

if args.verbose:
    for k in sorted(ym):
        if k != 'data':
            print("{0}: {1}".format(k, ym[k]))
else:
    print("Song ......", ym['song_name'])
    print("Author ....", ym['author'])
    print("Comment ...", ym['comment'])
    mins, secs = ym['length'] // 60, ym['length'] % 60
    print("Length .... {0:02d}:{1:02d}".format(mins, secs))

conn = serial.Serial(args.port, args.baudrate)

if args.unbuf:
    sleep = 1 / ym['frame_freq']
    for frame in range(ym['frames']):
        fd = ymfile.get_frame(ym['data'], frame, ym['interleaved'])
        conn.write(fd)
        time.sleep(sleep)
    conn.write(silence)
else:
    frame = 0
    frames = math.ceil(ym['frames'] / 16) * 16 + 32
    conn.flushInput()
    while frame < frames:
        wait_request(conn)
        buff = bytearray()
        for i in range(16):
            if frame >= ym['frames']:
                fdata = silence
            else:
                fdata = ymfile.get_frame(ym['data'], frame, ym['interleaved'])
            buff.extend(fdata)
            frame += 1
        conn.write(buff)

conn.close()
