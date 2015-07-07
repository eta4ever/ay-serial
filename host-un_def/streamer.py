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

parser.add_argument(    '-l', '--loop',
                        action='store_true',
                        help='looped playback')

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

    start_frame = 0
    sleep = 1 / ym['frame_freq']
    while True:
        for frame in range(start_frame, ym['frames']):
            fd = ymfile.get_frame(ym['data'], frame, ym['interleaved'])
            conn.write(fd)
            time.sleep(sleep)
        if args.loop:
            start_frame = ym['loop_frame']
        else:
            conn.write(silence)
            break

else:

    frame = 0
    conn.flushInput()
    while True:
        wait_request(conn)
        buff = bytearray()
        for i in range(16):
            if frame >= ym['frames'] and not args.loop:
                fdata = silence
            else:
                if frame >= ym['frames']:
                    frame = ym['loop_frame']
                fdata = ymfile.get_frame(ym['data'], frame, ym['interleaved'])
            buff.extend(fdata)
            frame += 1
        conn.write(buff)
        if frame >= ym['frames'] and not args.loop:
            buffer_silence = silence * 16
            wait_request(conn)
            conn.write(buffer_silence)
            wait_request(conn)
            conn.write(buffer_silence)
            break

conn.close()
