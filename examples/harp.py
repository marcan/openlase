#!/usr/bin/python3
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

# This is the laser harp sensing portion, which has nothing to do with OpenLase.
# You need OpenCV (2.2.0) and PyALSA installed
# Pass the destination ALSA MIDI client:port pair as an argument.

# You may need to edit some constants here

THRESHOLD=200
WIDTH=320
HEIGHT=240

NOTE_OFFSET=47

VELOCITY=90

import sys, math
import cv

from pyalsa import alsaseq

seq = alsaseq.Sequencer(name="default", clientname=sys.argv[0], streams = alsaseq.SEQ_OPEN_DUPLEX, mode = alsaseq.SEQ_NONBLOCK)
port = seq.create_simple_port(
	name='out',
	type = alsaseq.SEQ_PORT_TYPE_MIDI_GENERIC | alsaseq.SEQ_PORT_TYPE_APPLICATION,
	caps = alsaseq.SEQ_PORT_CAP_READ | alsaseq.SEQ_PORT_CAP_SUBS_READ)

def note(n, state=False):
	if state:
		etype = alsaseq.SEQ_EVENT_NOTEON
	else:
		etype = alsaseq.SEQ_EVENT_NOTEOFF
	event = alsaseq.SeqEvent(type=etype)
	event.set_data({'note.channel': 0, 'note.note': n, 'note.velocity': 90})
	seq.output_event(event)
	seq.drain_output()

if len(sys.argv) > 1:
	toport = seq.parse_address(sys.argv[1])
	seq.connect_ports((seq.client_id, port), toport)

camera = cv.CreateCameraCapture(0)
cv.SetCaptureProperty(camera, cv.CV_CAP_PROP_FRAME_WIDTH, WIDTH)
cv.SetCaptureProperty(camera, cv.CV_CAP_PROP_FRAME_HEIGHT, HEIGHT)
cv.SetCaptureProperty(camera, cv.CV_CAP_PROP_BRIGHTNESS, 0)

image = cv.QueryFrame(camera)
grey = cv.CreateImage(cv.GetSize(image), cv.IPL_DEPTH_8U, 1)
grey2 = cv.CreateImage(cv.GetSize(image), cv.IPL_DEPTH_8U, 1)

def getpoints(image):
	points = []
	cv.CvtColor(image, grey, cv.CV_RGB2GRAY)
	cv.Threshold(grey, grey, THRESHOLD, 255, cv.CV_THRESH_BINARY)
	cv.Copy(grey, grey2)

	storage = cv.CreateMemStorage(0)
	contour = cv.FindContours(grey, storage, cv.CV_RETR_CCOMP, cv.CV_CHAIN_APPROX_SIMPLE)
	while contour:
		bound_rect = cv.BoundingRect(list(contour))
		contour = contour.h_next()

		pt = (bound_rect[0] + bound_rect[2]/2, bound_rect[1] + bound_rect[3]/2)
		#cv.Circle(image, pt, 2, cv.CV_RGB(255,0,0), 1)
		points.append(pt)
	return points

def near(p1, p2, d):
	x,y = (p1[0]-p2[0],p1[1]-p2[1])
	dist = math.sqrt(x**2+y**2)
	return dist < d

refpoints = getpoints(image)
refpoints.sort(key=lambda x: x[1])

oldstate = None

while True:
	image = cv.QueryFrame(camera)
	cpoints = getpoints(image)
	state = [False]*len(refpoints)
	for i, rp in enumerate(refpoints):
		for cp in cpoints:
			if near(rp,cp,3):
				break
		else:
			state[i] = True

	if oldstate is not None:
		for i,(j,k) in enumerate(zip(oldstate, state)):
			if j != k:
				note(NOTE_OFFSET+len(oldstate)-i, k)
				if k:
					print("PRESSED: %d"%i)
				else:
					print("RELEASED: %d"%i)
	oldstate = state

	cv.ShowImage("thresholded", grey2)
	if cv.WaitKey(10)&0xfff == 27:
		break
