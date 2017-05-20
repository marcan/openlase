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

import pylase as ol

import sys

if ol.init(10) < 0:
	sys.exit(1)
params = ol.RenderParams()
params.render_flags = ol.RENDER_NOREORDER | ol.RENDER_GRAYSCALE
params.on_speed = 2/120.0
params.off_speed = 2/30.0
params.flatness = 0.000001
ol.setRenderParams(params)

lines = sys.argv[1:]

SIZE = 0.4

while True:
	lc = len(lines)

	font = ol.getDefaultFont()
	yoff = (lc/2.0) * 0.4

	for i,line in enumerate(lines):
		w = ol.getStringWidth(font, 0.4, line)
		ol.drawString(font, (-w/2,yoff-i*0.4), 0.4, ol.C_WHITE, line)

	ftime = ol.renderFrame(60)
