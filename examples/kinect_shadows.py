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

import pylase as ol
import threading
import time
import numpy as np
import freenect
import sys

class LaserThread(threading.Thread):
	def __init__(self):
		threading.Thread.__init__(self)
		self.die = False
		self.trace = None

	def run(self):
		while self.trace is None and not self.die:
			time.sleep(0.1)

		if self.die:
			return

		if ol.init(3) < 0:
			return
		print "OL Initialized"
		params = ol.RenderParams()
		params.render_flags = ol.RENDER_GRAYSCALE
		params.on_speed = 2/120.0
		params.off_speed = 2/30.0
		params.flatness = 0.000001
		params.max_framelen = 48000 / 25
		params.min_length = 30
		params.snap = 0.04
		ol.setRenderParams(params)
		ol.loadIdentity()
		ol.scale((2, -2))
		ol.translate((-0.5, -0.5))
		ol.scale((1/640.0, 1/640.0))
		ol.translate((0, (640-480)/2))

		while not self.die:
			#ol.rect((100, 100), (640-100, 480-100), ol.C_WHITE)
			objects = self.trace
			for o in objects:
				ol.begin(ol.POINTS)
				for point in o[::2]:
					ol.vertex(point, ol.C_WHITE)
				ol.end()
			ftime = ol.renderFrame(60)
		ol.shutdown()

olt = LaserThread()
print "Starting thread"
olt.start()
print "Thread running"

try:
	tracer = ol.Tracer(640, 480)
	tracer.mode = ol.TRACE_CANNY
	tracer.threshold = 30
	tracer.threshold2 = 10
	tracer.sigma = 1.2

	try:
		floor = np.load("floor.npy")
		print "floor loaded"
	except:
		floor = None

	while True:
		frame, timestamp = freenect.sync_get_depth()
		frame = frame.astype(np.float)
		mask = frame > 1070
		depth = 1.0 / (frame * -0.0030711016 + 3.3309495161)
		depth = np.ma.filled(np.ma.array(depth, mask=mask), 100)
		if floor is not None:
			depth = np.minimum(floor, depth)

		image = np.clip((depth * 250 / 6),0,255).astype(np.uint8)

		data = image.tostring()
		objects = tracer.trace(data)
		#print objects

		olt.trace = objects

finally:
	olt.die = True
	olt.join()

