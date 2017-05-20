#!/usr/bin/python3
# -*- coding: utf-8 -*-
#         OpenLase - a realtime laser graphics toolkit
#
# Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 or version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

import sys, os, json, http.cookiejar, urllib.request, urllib.parse, urllib.error, urllib.request, urllib.error, urllib.parse, zipfile, io
import pylase as ol
from PIL import Image

def fetch_ugo(illust_id):
    cj = http.cookiejar.CookieJar()
    try:
        print('Logging in...')
        user, password = open(os.environ['HOME'] + '/.pixiv_credentials').read().strip().split(':', 1)
        opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))
        fd = opener.open('https://www.secure.pixiv.net/login.php')
        fd.read()
        fd.close()
        data = urllib.parse.urlencode({
            'mode': 'login',
            'pixiv_id': user,
            'pass': password,
            'skip': '1'
        })
        post = urllib.request.Request('https://www.secure.pixiv.net/login.php', data)
        fd = opener.open(post)
        fd.read()
        fd.close()
    except:
        print('Login failed or credentials not available, using logged-out mode')
    print('Fetching ugoira...')
    url = 'http://www.pixiv.net/member_illust.php?mode=medium&illust_id=%d' % illust_id
    meta = None
    fd = opener.open(url)
    for line in fd:
        if 'ugokuIllustData' in line:
            meta = json.loads(line.split('ugokuIllustData', 1)[1]
                              .strip()[1:].split(';', 1)[0])
            break
    fd.close()
    if meta is None:
        print('Not ugoira or fetch failed!')
        sys.exit(1)
    src_url = meta['src']
    get = urllib.request.Request(src_url, None, {'Referer': url})
    fd = opener.open(get)
    zip_data = fd.read()
    fd.close()
    return meta, zip_data

def trace_ugo(meta, zip_data):
    zipfd = io.StringIO(zip_data)
    zf = zipfile.ZipFile(zipfd, 'r')

    tracer = None
    frames = []

    for frame_meta in meta['frames']:
        file_data = zf.open(frame_meta['file']).read()
        fd = io.StringIO(file_data)
        im = Image.open(fd)
        im = im.convert('I')
        if tracer is None:
            width, height = im.size
            tracer = ol.Tracer(width, height)
            tracer.mode = ol.TRACE_CANNY
            tracer.threshold = 30
            tracer.threshold2 = 10
            tracer.sigma = 1.2
        s = im.tostring('raw', 'I')
        if len(s) == (width * height * 4):
             s = s[::4] #XXX workaround PIL bug
        objects = tracer.trace(s)
        frames.append(objects)
    return (width, height), frames

def play_ugo(size, frames, meta):
    frame_pts = []
    total_time = 0
    for frame_meta in meta['frames']:
        frame_pts.append(total_time)
        total_time += frame_meta['delay'] / 1000.0

    if ol.init(3) < 0:
        return

    width, height = size

    params = ol.RenderParams()
    params.render_flags = ol.RENDER_GRAYSCALE
    params.on_speed = 2/60.0
    params.off_speed = 2/30.0
    params.flatness = 0.000001
    params.max_framelen = 48000 / 25
    params.min_length = 30
    params.snap = 0.04
    ol.setRenderParams(params)
    ol.loadIdentity()
    ol.scale((2, -2))
    ol.translate((-0.5, -0.5))
    mw = float(max(width, height))
    print(width, height, mw)
    ol.scale((1/mw, 1/mw))
    ol.translate(((mw-width)/2, (mw-height)/2))

    frame = 0
    time = 0

    DECIMATE = 2

    while True:
        while time > total_time:
            time -= total_time
            frame = 0

        while (frame+1) < len(frames) and frame_pts[frame+1] < time:
            frame += 1

        print("t=%.02f frame=%d" % (time, frame))

        objects = frames[frame]
        points = 0
        for o in objects:
            ol.begin(ol.POINTS)
            for point in o[::DECIMATE]:
                ol.vertex(point, ol.C_WHITE)
                points += 1
            ol.end()
        print("%d objects, %d points" % (len(objects), points))
        time += ol.renderFrame(60)

    ol.shutdown()


if __name__ == '__main__':
    meta, zip_data = fetch_ugo(int(sys.argv[1]))
    print('Converting ugoira...')
    size, frames = trace_ugo(meta, zip_data)
    print('Playing...')
    play_ugo(size, frames, meta)

