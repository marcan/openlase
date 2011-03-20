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
from math import pi

ol.init()

time = 0
frames = 0

while True:
	ol.loadIdentity3()
	ol.loadIdentity()

	font = ol.getDefaultFont()
	s = "Hi!"
	w = ol.getStringWidth(font, 0.2, s)
	ol.drawString(font, (-w/2,0.1), 0.2, ol.C_WHITE, s)

	ol.perspective(60, 1, 1, 100)
	ol.translate3((0, 0, -3))

	for i in range(2):
		ol.scale3((0.6, 0.6, 0.6))
		ol.rotate3Z(time * pi * 0.1)
		ol.rotate3X(time * pi * 0.8)
		ol.rotate3Y(time * pi * 0.73)

		ol.begin(ol.LINESTRIP)
		ol.vertex3((-1, -1, -1), ol.C_WHITE)
		ol.vertex3(( 1, -1, -1), ol.C_WHITE)
		ol.vertex3(( 1,  1, -1), ol.C_WHITE)
		ol.vertex3((-1,  1, -1), ol.C_WHITE)
		ol.vertex3((-1, -1, -1), ol.C_WHITE)
		ol.vertex3((-1, -1,  1), ol.C_WHITE)
		ol.end()

		ol.begin(ol.LINESTRIP);
		ol.vertex3(( 1,  1,  1), ol.C_WHITE)
		ol.vertex3((-1,  1,  1), ol.C_WHITE)
		ol.vertex3((-1, -1,  1), ol.C_WHITE)
		ol.vertex3(( 1, -1,  1), ol.C_WHITE)
		ol.vertex3(( 1,  1,  1), ol.C_WHITE)
		ol.vertex3(( 1,  1, -1), ol.C_WHITE)
		ol.end()

		ol.begin(ol.LINESTRIP)
		ol.vertex3(( 1, -1, -1), ol.C_WHITE)
		ol.vertex3(( 1, -1,  1), ol.C_WHITE)
		ol.end()

		ol.begin(ol.LINESTRIP)
		ol.vertex3((-1,  1,  1), ol.C_WHITE)
		ol.vertex3((-1,  1, -1), ol.C_WHITE)
		ol.end()

	ftime = ol.renderFrame(60)
	frames += 1
	time += ftime
	print "Frame time: %f, FPS:%f"%(ftime, frames/time)

ol.shutdown()
