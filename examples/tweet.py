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
from math import pi
import threading
import twitter
import time
import math
import sys

class LaserThread(threading.Thread):
	def __init__(self):
		threading.Thread.__init__(self)
		self.die = False
		self.tweets = None

	def shade(self, coord, color):
		x, y = coord
		if abs(x) > 1:
			return (x, y), color
		#y *= (1+0.2*math.cos(x))
		if abs(x) > 0.8:
			c = 1 - (abs(x) - 0.8) / 0.2
			if c > 1:
				c = 1
			elif c < 0:
				c = 0
			color = color & 0xff
			color = ol.C_GREY(int(color * c))
		return (x, y), color

	def run(self):
		if ol.init(10) < 0:
			return
		params = ol.RenderParams()
		params.render_flags = ol.RENDER_NOREORDER | ol.RENDER_GRAYSCALE
		params.on_speed = 2/120.0
		params.off_speed = 2/30.0
		params.flatness = 0.000001
		ol.setRenderParams(params)
		ol.setPixelShader(self.shade)
		time = 0
		frames = 0
		cur_tweets = self.tweets
		xpos = 0
		idx = 0
		startpos = 1.3
		while not self.die:
			ol.loadIdentity3()
			ol.loadIdentity()

			if cur_tweets is None and self.tweets is not None:
				cur_tweets = self.tweets
				idx = 0
				xpos = startpos

			w = 0
			#print cur_tweets
			if cur_tweets is not None:
				font = ol.getDefaultFont()
				w = ol.getStringWidth(font, 0.4, cur_tweets[idx])
				col = ol.C_WHITE
				#print "Render %f %s 0x%x"%(xpos, cur_tweets[idx], col)
				ol.drawString(font, (xpos,0.1), 0.4, col, cur_tweets[idx])
	
			#print "render"
			ftime = ol.renderFrame(60)
			#print "done"
			xpos -= 0.6*ftime
			if xpos < (-w-1) and cur_tweets is not None:
				xpos = startpos
				idx += 1
				idx %= len(cur_tweets)
				if self.tweets != cur_tweets:
					idx = 0
					cur_tweets = self.tweets
					print "Reset and update"
				print "Finished, new idx: %d"%idx
			frames += 1
			time += ftime
		ol.shutdown()

cmap = {
	u'Á': 'A',
	u'É': 'E',
	u'Í': 'I',
	u'Ó': 'O',
	u'Ú': 'U',
	u'Ü': 'U',
}

search = sys.argv[1]

olt = LaserThread()
print "Starting thread"
olt.start()
print "Thread running"

try:
	api = twitter.Twitter(domain="search.twitter.com")
	since_id = None

	tweets = []

	while True:
		try:
			s = api.search(q="#EE20")["results"]
		except Exception, e:
			olt.tweets = ["Twitter Error: NO FUNCIONA INTERNEEEEE!!!"]
			time.sleep(1)
			since_id = -1
			tweets = []
			print e, "Fetch error"
			continue

		if len(s) == 0:
			time.sleep(10)
			continue
		print "Fetched %d tweets"%len(s)
		since_id = s[0]["id"]
		tweets = s + tweets
		tweets = tweets[:10]
		strings = []
		print "New tweetset:"
		for t in tweets:
			itext = u"@%s: %s"%(t["from_user_name"], t["text"])
			itext = itext.replace("&gt;", ">").replace("&lt;", "<").replace("&quot;", '"').replace("\n", " ")
			text = u""
			for c in itext:
				if c in cmap:
					c = cmap[c]
				text += c
			strings.append(text.encode('iso-8859-1', 'replace'))
			print "-->", itext
		olt.tweets = strings
		time.sleep(10)
except:
	olt.die = True
	olt.join()
	raise
