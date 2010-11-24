#!/usr/bin/python
# -*- coding: utf-8 -*-

import os, sys, math
import xml.sax, xml.sax.handler
import re

def pc(c):
	x,y = c
	if isinstance(x,int) and isinstance(y,int):
		return "(%d,%d)"%(x,y)
	else:
		return "(%.4f, %.4f)"%(x,y)

class PathLine(object):
	def __init__(self, start, end, on=True):
		self.start = start
		self.end = end
		self.on = on
	def copy(self):
		return PathLine(self.start, self.end)
	def transform(self, func):
		self.start = func(self.start)
		self.end = func(self.end)
	def reverse(self):
		return PathLine(self.end, self.start, self.on)
	def scp(self):
		return self.end
	def ecp(self):
		return self.start
	def showinfo(self, tr=''):
		print tr+'Line( %s %s )'%(pc(self.start),pc(self.end))

class PathBezier4(object):
	def __init__(self, start, cp1, cp2, end):
		self.start = start
		self.end = end
		self.cp1 = cp1
		self.cp2 = cp2
	def copy(self):
		return PathBezier4(self.start, self.cp1, self.cp2, self.end)
	def transform(self, func):
		self.start = func(self.start)
		self.cp1 = func(self.cp1)
		self.cp2 = func(self.cp2)
		self.end = func(self.end)

	def reverse(self):
		return PathBezier4(self.end, self.cp2, self.cp1, self.start)
	def scp(self):
		if self.cp1 != self.start:
			return self.cp1
		elif self.cp2 != self.start:
			return self.cp2
		else:
			return self.end
	def ecp(self):
		if self.cp2 != self.end:
			return self.cp2
		elif self.cp1 != self.end:
			return self.cp1
		else:
			return self.start
	def showinfo(self, tr=''):
		print tr+'Bezier( %s %s %s %s )'%(pc(self.start),pc(self.cp1),pc(self.cp2),pc(self.end))

class PathBezier3(PathBezier4):
	def __init__(self, start, cp, end):
		sx,sy = start
		cx,cy = cp
		ex,ey = end

		c1x = (1.0/3) * cx + (2.0/3) * sx
		c1y = (1.0/3) * cy + (2.0/3) * sy
		c2x = (1.0/3) * cx + (2.0/3) * ex
		c2y = (1.0/3) * cy + (2.0/3) * ey
		PathBezier4.__init__(start, (c1x,c1y), (c2x,c2y), end)

class LaserPath(object):
	def __init__(self):
		self.segments = []
	def add(self, seg):
		self.segments.append(seg)
	def transform(self, func):
		for i in self.segments:
			i.transform(func)
	def startpos(self):
		return self.segments[0].start
	def endpos(self):
		return self.segments[-1].end
	def reverse(self):
		lp = LaserPath()
		lp.segments = [x.reverse() for x in self.segments[::-1]]
		return lp
	def showinfo(self, tr=''):
		print tr+'LaserPath:'
		for i in self.segments:
			i.showinfo(tr+' ')

class LaserFrame(object):
	def __init__(self):
		self.objects = []
	def add(self, obj):
		self.objects.append(obj)
	def transform(self, func):
		for i in self.objects:
			i.transform(func)
	def showinfo(self, tr=''):
		print tr+'LaserFrame:'
		for i in self.objects:
			i.showinfo(tr+' ')

class SVGPath(object):
	def __init__(self, data=None):
		self.subpaths = []
		if data:
			self.parse(data)

	def poptok(self):
		if not self.tokens:
			raise ValueError("Expected token but got end of path data")
		return self.tokens.pop(0)
	def peektok(self):
		if not self.tokens:
			raise ValueError("Expected token but got end of path data")
		return self.tokens[0]
	def popnum(self):
		tok = self.tokens.pop(0)
		try:
			return float(tok)
		except ValueError:
			raise ValueError("Invalid SVG path numerical token: %s"%tok)
	def isnum(self):
		if not self.tokens:
			return False # no more tokens is considered a non-number
		try:
			float(self.peektok())
		except ValueError:
			return False
		return True
	def popcoord(self,rel=False, cur=None):
		x = self.popnum()
		if self.peektok() == ',':
			self.poptok()
		y = self.popnum()
		if rel:
			x += cur[0]
			y += cur[1]
		return x,y
	def reflect(self, center, point):
		cx,cy = center
		px,py = point
		return (2*cx-px, 2*cy-py)

	def parse(self, data):
		ds = re.split(r"[ \r\n\t]*([-+]?\d+\.\d+[eE][+-]?\d+|[-+]?\d+\.\d+|[-+]?\.\d+[eE][+-]?\d+|[-+]?\.\d+|[-+]?\d+\.?[eE][+-]?\d+|[-+]?\d+\.?|[MmZzLlHhVvCcSsQqTtAa])[, \r\n\t]*", data)
		tokens = ds[1::2]
		if any(ds[::2]) or not all(ds[1::2]):
			raise ValueError("Invalid SVG path expression: %r"%data)
		self.tokens = tokens

		cur = (0,0)
		curcpc = (0,0)
		curcpq = (0,0)
		sp_start = (0,0)
		in_path = False
		cmd = None
		subpath = LaserPath()
		while tokens:
			if self.isnum() and cmd in "MmLlHhVvCcSsQqTtAa":
				pass
			elif self.peektok() in "MmZzLlHhVvCcSsQqTtAa":
				cmd = tokens.pop(0)
			else:
				raise ValueError("Invalid SVG path token %s"%tok)

			rel = cmd.upper() != cmd
			ucmd = cmd.upper()

			if not in_path and ucmd != 'M' :
				raise ValueErorr("SVG path segment must begin with 'm' command, not '%s'"%cmd)

			if ucmd == 'M':
				sp_start = self.popcoord(rel, cur)
				if subpath.segments:
					self.subpaths.append(subpath)
				subpath = LaserPath()
				cmd = 'Ll'[rel]
				cur = curcpc = curcpq = sp_start
				in_path = True
			elif ucmd == 'Z':
				if sp_start != cur:
					subpath.add(PathLine(cur, sp_start))
				cur = curcpc = curcpq = sp_start
				in_path = False
			elif ucmd == 'L':
				end = self.popcoord(rel, cur)
				subpath.add(PathLine(cur, end))
				cur = curcpc = curcpq = end
			elif ucmd == 'H':
				ex = self.popnum()
				if rel:
					ex += cur[0]
				end = ex,cur[1]
				subpath.add(PathLine(cur, end))
				cur = curcpc = curcpq = end
			elif ucmd == 'V':
				ey = self.popnum()
				if rel:
					ey += cur[1]
				end = cur[0],ey
				subpath.add(PathLine(cur, end))
				cur = curcpc = curcpq = end
			elif ucmd in 'CS':
				if ucmd == 'S':
					c1 = self.reflect(cur,curcpc)
				else:
					c1 = self.popcoord(rel, cur)
				c2 = curcpc = self.popcoord(rel, cur)
				end = self.popcoord(rel, cur)
				subpath.add(PathBezier4(cur, c1, c2, end))
				cur = curcpq = end
			elif ucmd in 'QT':
				if ucmd == 'T':
					cp = curcpq = self.reflect(cur,curcpq)
				else:
					cp = curcpq = self.popcoord(rel, cur)
				end = self.popcoord(rel, cur)
				subpath.add(PathBezier3(cur, cp, end))
				cur = curcpc = end
			elif ucmd in 'Aa':
				raise ValueError("Arcs not implemented, biatch")

		if subpath.segments:
			self.subpaths.append(subpath)

class SVGReader(xml.sax.handler.ContentHandler):
	def doctype(self, name, pubid, system):
		print name,pubid,system
	def startDocument(self):
		self.frame = LaserFrame()
		self.rects = []
		self.matrix_stack = [(1,0,0,1,0,0)]
	def endDocument(self):
		pass
	def startElement(self, name, attrs):
		if name == "svg":
			self.width = float(attrs['width'].replace("px",""))
			self.height = float(attrs['height'].replace("px",""))
		elif name == "path":
			if 'transform' in attrs.keys():
				self.transform(attrs['transform'])
			self.addPath(attrs['d'])
			if 'transform' in attrs.keys():
				self.popmatrix()
		elif name == "rect":
			self.rects.append((attrs['id'],float(attrs['x']),float(attrs['y']),float(attrs['width']),float(attrs['height'])))
		elif name == 'g':
			if 'transform' in attrs.keys():
				self.transform(attrs['transform'])
			else:
				self.pushmatrix((1,0,0,1,0,0))
	def endElement(self, name):
		if name == 'g':
			self.popmatrix()
	def mmul(self, m1, m2):
		a1,b1,c1,d1,e1,f1 = m1
		a2,b2,c2,d2,e2,f2 = m2
		a3 = a1 * a2 + c1 * b2
		b3 = b1 * a2 + d1 * b2
		c3 = a1 * c2 + c1 * d2
		d3 = b1 * c2 + d1 * d2
		e3 = a1 * e2 + c1 * f2 + e1
		f3 = b1 * e2 + d1 * f2 + f1
		return (a3,b3,c3,d3,e3,f3)
	def pushmatrix(self, m):
		new_mat = self.mmul(self.matrix_stack[-1], m)
		self.matrix_stack.append(new_mat)
	def popmatrix(self):
		self.matrix_stack.pop()
	def tc(self,coord):
		vw = vh = max(self.width, self.height) / 2.0
		x,y = coord
		x -= self.width / 2.0
		y -= self.height / 2.0
		x = x / vw
		y = y / vh
		return (x,y)
	def ts(self,coord):
		a,b,c,d,e,f = self.matrix_stack[-1]
		x,y = coord
		nx = a*x + c*y + e
		ny = b*x + d*y + f
		return (nx,ny)
	def transform(self, data):
		ds = re.split(r"[ \r\n\t]*([a-z]+\([^)]+\)|,)[ \r\n\t]*", data)
		tokens = ds[1::2]
		if any(ds[::2]) or not all(ds[1::2]):
			raise ValueError("Invalid SVG transform expression: %r"%data)
		if not all([x == ',' for x in tokens[1::2]]):
			raise ValueError("Invalid SVG transform expression: %r"%data)
		transforms = tokens[::2]

		mat = (1,0,0,1,0,0)
		for t in transforms:
			name,rest = t.split("(")
			if rest[-1] != ")":
				raise ValueError("Invalid SVG transform expression: %r"%data)
			args = map(float,rest[:-1].split(","))
			if name == 'matrix':
				mat = self.mmul(mat, args)
			elif name == 'translate':
				tx,ty = args
				mat = self.mmul(mat, (1,0,0,1,tx,ty))
			elif name == 'scale':
				sx,sy = args
				mat = self.mmul(mat, (sx,0,0,sy,0,0))
			elif name == 'rotate':
				a = args[0] / 180.0 * math.pi
				if len(args) == 3:
					cx,cy = args[1:]
					mat = self.mmul(mat, (1,0,0,1,cx,cy))
				cos = math.cos(a)
				sin = math.sin(a)
				mat = self.mmul(mat, (cos,sin,-sin,cos,0,0))
				if len(args) == 3:
					mat = self.mmul(mat, (1,0,0,1,-cx,-cy))
			elif name == 'skewX':
				a = args[0] / 180.0 * math.pi
				mat = self.mmul(mat, (1,0,math.tan(a),1,0,0))
			elif name == 'skewY':
				a = args[0] / 180.0 * math.pi
				mat = self.mmul(mat, (1,math.tan(a),0,1,0,0))
		self.pushmatrix(mat)
	def addPath(self, data):
		p = SVGPath(data)
		for path in p.subpaths:
			path.transform(self.ts)
			self.frame.add(path)

svg = sys.argv[1]
cfile = sys.argv[2]
if len(sys.argv) > 3:
	font_name = sys.argv[3]
else:
	font_name = "font"

handler = SVGReader()
parser = xml.sax.make_parser()
parser.setContentHandler(handler)
parser.setFeature(xml.sax.handler.feature_external_ges, False)
parser.parse(sys.argv[1])

frame = handler.frame

cdefs = []

max_h = 0

output = ""

output += "#include \"text.h\"\n"

overlap = 0

for id, x, y, w, h in handler.rects:
	if id == "overlap":
		overlap = w
		continue
	if id[:5] != "char_":
		continue
	cv = id[5:]
	if len(cv) == 1:
		chrval = ord(id[5:])
	else:
		chrval = int(cv,0)

	if h > max_h:
		max_h = h

	#print >>sys.stderr, "Got rect %s"%id
	char_objs = []
	for obj in frame.objects:
		sx,sy = obj.segments[0].start
		if sx >= x and sy >= y and sx <= (x+w) and sy <= (y+h):
			char_objs.append(obj)
	#print >>sys.stderr,  " - %d objects"%len(char_objs)
	
	def transform(pt):
		px,py = pt
		px -= x
		py -= y
		return (px,py)
	
	if len(char_objs) != 0:
		output += "static const FontPoint fc_%02x[] = {\n"%chrval

		for i,obj in enumerate(char_objs):
			obj.transform(transform)
			last_obj = i == len(char_objs)-1
			output += "\t{0, %8.4f, %8.4f},\n"%obj.startpos()
			
			for j,seg in enumerate(obj.segments):
				last_seg = j == len(obj.segments)-1
				if last_obj and last_seg:
					ctl = 2
				elif last_seg:
					ctl = 1
				else:
					ctl = 0
				if isinstance(seg, PathBezier4):
					output += "\t{0, %8.4f, %8.4f},\n"%seg.cp1
					output += "\t{0, %8.4f, %8.4f},\n"%seg.cp2
					output += "\t{%d, %8.4f, %8.4f},\n"%(ctl, seg.end[0], seg.end[1])
				elif isinstance(seg, PathLine):
					output += "\t{0, %8.4f, %8.4f},\n"%seg.start
					output += "\t{0, %8.4f, %8.4f},\n"%seg.end
					output += "\t{%d, %8.4f, %8.4f},\n"%(ctl, seg.end[0], seg.end[1])
		
		output += "};\n"
	
		cdefs.append((chrval, w, "fc_%02x"%chrval))
	else:
		cdefs.append((chrval, w, "NULL"))

output += "\nstatic const FontChar font_chars[256] = {\n"

for chrval, width, sym in cdefs:
	output += "\t['%s'] = {%8.4f, %s},\n"%(chr(chrval),width,sym)

output += "};\n\n"

output += "const Font %s = {%f, %f, font_chars};\n\n"%(font_name,max_h,overlap)

fd = open(cfile,"w")
fd.write(output)
fd.close()