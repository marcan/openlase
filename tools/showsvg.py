#!/usr/bin/python2
import sys
from math import pi, cos

import pylase as ol
import svg

image = svg.load_svg(sys.argv[1])

params = ol.RenderParams()
params.render_flags = 0
params.on_speed = 2/110.0
params.off_speed = 2/70.0
params.flatness = 0.0001
params.corner_dwell = 4
params.start_dwell = 3
params.end_dwell = 3
params.curve_dwell = 0
params.curve_angle = cos(30.0 * (pi / 180.0))
params.snap = 0.0001

ol.init()
ol.setRenderParams(params)

while True:
	ol.loadIdentity()
	ol.translate((-1,-1))
	ol.scale((1.99,1.99))
	xmin, ymin, xmax, ymax = image.bbox()
	w = xmax - xmin
	h = ymax - ymin
	ol.scale((1.0/max(w,h), 1.0/max(w,h)))
	ol.translate((-xmin, -ymin))
	image.draw()
	ol.renderFrame(60)
