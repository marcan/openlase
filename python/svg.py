#!/usr/bin/python3
# -*- coding: utf-8 -*-

import pylase as ol
import math
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
    def to_bezier4(self):
        return PathBezier4(self.start, self.start, self.end, self.end)
    def reverse(self):
        return PathLine(self.end, self.start, self.on)
    def scp(self):
        return self.end
    def ecp(self):
        return self.start
    def showinfo(self, tr=''):
        print(tr+'Line( %s %s )'%(pc(self.start),pc(self.end)))

class PathBezier3(object):
    def __init__(self, start, cp, end):
        self.start = start
        self.end = end
        self.cp = cp
    def copy(self):
        return PathBezier3(self.start, self.cp, self.end)
    def transform(self, func):
        self.start = func(self.start)
        self.cp = func(self.cp)
        self.end = func(self.end)
    def to_bezier4(self):
        sx,sy = self.start
        cx,cy = self.cp
        ex,ey = self.end

        c1x = (1.0/3) * cx + (2.0/3) * sx
        c1y = (1.0/3) * cy + (2.0/3) * sy
        c2x = (1.0/3) * cx + (2.0/3) * ex
        c2y = (1.0/3) * cy + (2.0/3) * ey
        return PathBezier4(self.start, (c1x,c1y), (c2x,c2y), self.end)
    def reverse(self):
        return PathBezier3(self.end, self.cp, self.start)
    def scp(self):
        return self.cp
    def ecp(self):
        return self.cp
    def showinfo(self, tr=''):
        print(tr+'Bezier( %s %s %s )'%(pc(self.start),pc(self.cp),pc(self.end)))

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
    def to_bezier4(self):
        return self
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
    def cut(self, t):
        s = 1 - t
        x0,y0 = self.start
        x1,y1 = self.cp1
        x2,y2 = self.cp2
        x3,y3 = self.end
        mcx = x1 * s + x2 * t
        mcy = y1 * s + y2 * t
        ax1 = x0 * s + x1 * t
        ay1 = y0 * s + y1 * t
        ax2 = ax1 * s +  mcx * t
        ay2 = ay1 * s + mcy * t
        bx2 = x2 * s + x3 * t
        by2 = y2 * s + y3 * t
        bx1 = bx2 * t + mcx * s
        by1 = by2 * t + mcy * s
        xm = ax2 * s + bx1 * t
        ym = ay2 * s + by1 * t
        curve1 = PathBezier4((x0,y0),(ax1,ay1),(ax2,ay2),(xm,ym))
        curve2 = PathBezier4((xm,ym),(bx1,by1),(bx2,by2),(x3,y3))
        return curve1, curve2

    def showinfo(self, tr=''):
        print(tr+'Bezier( %s %s %s %s )'%(pc(self.start),pc(self.cp1),pc(self.cp2),pc(self.end)))

class SVGSubpath(object):
    def __init__(self, name=None, color=0xffffff):
        self.name = name
        self.color = color
        self.segments = []
        self.lines_only = True
    def copy(self):
        s = SVGSubpath()
        s.segments = [i.copy() for i in self.segments]
        s.name = self.name
        s.lines_only = self.lines_only
        s.color = self.color
        return s
    def add(self, seg):
        self.segments.append(seg)
        if not isinstance(seg, PathLine):
            self.lines_only = False
    def transform(self, func):
        for i in self.segments:
            i.transform(func)
    def startpos(self):
        return self.segments[0].start
    def endpos(self):
        return self.segments[-1].end
    def reverse(self):
        s = SVGSubpath()
        s.segments = [x.reverse() for x in self.segments[::-1]]
        s.name = self.name
        s.lines_only = self.lines_only
        return s
    def draw(self):
        if self.lines_only:
            ol.begin(ol.LINESTRIP)
            x, y =  self.segments[0].start
            ol.vertex3((x, y, 0), self.color)
            for i in self.segments:
                ol.vertex3((i.end[0], i.end[1], 0), self.color)
            ol.end()
        else:
            ol.begin(ol.BEZIERSTRIP)
            x, y = self.segments[0].start
            ol.vertex3((x, y, 0), self.color)
            for i in self.segments:
                i = i.to_bezier4()
                ol.vertex3((i.cp1[0], i.cp1[1], 0), self.color)
                ol.vertex3((i.cp2[0], i.cp2[1], 0), self.color)
                ol.vertex3((i.end[0], i.end[1], 0), self.color)
            ol.end()
    def evaluate(self, at):
        if at >= 1.0:
            return self.endpos()
        elif at <= 0.0:
            return self.startpos()
        at *= len(self.segments)
        mid = self.segments[int(at)].to_bezier4()
        return mid.cut(at - int(at))[0].end
    def cut(self, at):
        if at >= 1.0:
            return self.copy(), SVGSubpath()
        elif at <= 0.0:
            return SVGSubpath(), self.copy()
        at *= len(self.segments)
        a = self.copy()
        b = self.copy()
        mid = int(at)
        a.segments = a.segments[:mid + 1]
        b.segments = b.segments[mid:]
        mid_curve = a.segments[-1].to_bezier4()
        a.segments[-1], b.segments[0] = mid_curve.cut(at - int(at))
        return a, b

    # only considers control points, not curves.
    def bbox(self):
        xmin, ymin = self.segments[0].start
        xmax, ymax = xmin, ymin
        for i in self.segments:
            x, y = i.end
            xmin = min(xmin, x)
            ymin = min(ymin, y)
            xmax = max(xmax, x)
            ymax = max(ymax, y)
        return xmin, ymin, xmax, ymax

    def showinfo(self, tr=''):
        print(tr+'SVGSubpath(%s): color=%06x, lo=%r' % (
            self.name, self.color, self.lines_only))
        for i in self.segments:
            i.showinfo(tr+' ')

class SVGGroup(object):
    def __init__(self, name=None):
        self.name = name
        self.objects = []
        self.omap = {}
    def add(self, obj):
        self.objects.append(obj)
        if obj.name:
            self.omap[obj.name] = obj
    def copy(self):
        g = SVGGroup(self.name)
        for i in self.objects:
            g.add(i.copy())
        return g
    def flatten(self):
        g = SVGGroup(self.name)
        for i in self.objects:
            if isinstance(i, SVGGroup):
                for j in i.flatten():
                    g.add(j)
            else:
                g.add(i)
        return g
    def __getitem__(self, i):
        if isinstance(i, str):
            return self.omap[i]
        else:
            return self.objects[i]
    def __getattr__(self, i):
        return self.omap[i]
    def __iter__(self):
        return iter(self.objects)
    def cut(self, at):
        if at >= 1.0:
            return self.copy(), SVGGroup(self.name)
        elif at <= 0.0:
            return SVGGroup(self.name), self.copy()
        at *= len(self.objects)
        a = SVGGroup(self.name)
        b = SVGGroup(self.name)
        mid = int(at)
        for i in self.objects[:mid]:
            a.add(i)
        left, right = self.objects[mid].cut(at - int(at))
        a.add(left)
        b.add(right)
        for i in self.objects[mid + 1:]:
            b.add(i)
        return a, b
    def transform(self, func):
        for i in self.objects:
            i.transform(func)
    def draw(self):
        for i in self.objects:
            i.draw()
    def bbox(self):
        xmin = ymin = float("inf")
        xmax = ymax = float("-inf")
        for i in self.objects:
            x0, y0, x1, y1 = i.bbox()
            xmin = min(xmin, x0)
            ymin = min(ymin, y0)
            xmax = max(xmax, x1)
            ymax = max(ymax, y1)
        return xmin, ymin, xmax, ymax
    def showinfo(self, tr=''):
        print(tr+'SVGGroup(%s):' % self.name)
        for i, j in enumerate(self.objects):
            #print tr+'[%d]' % i
            j.showinfo(tr+' ')

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

    def angle(self, u, v):
        # calculate the angle between two vectors
        ux, uy = u
        vx, vy = v
        dot = ux*vx + uy*vy
        ul = math.sqrt(ux**2 + uy**2)
        vl = math.sqrt(vx**2 + vy**2)
        a = math.acos(dot/(ul*vl))
        return math.copysign(a, ux*vy - uy*vx)
    def arc_eval(self, cx, cy, rx, ry, phi, w):
        # evaluate a point on an arc
        x = rx * math.cos(w)
        y = ry * math.sin(w)
        x, y = x*math.cos(phi) - y*math.sin(phi), math.sin(phi)*x + math.cos(phi)*y
        x += cx
        y += cy
        return (x,y)
    def arc_deriv(self, rx, ry, phi, w):
        # evaluate the derivative of an arc
        x = -rx * math.sin(w)
        y = ry * math.cos(w)
        x, y = x*math.cos(phi) - y*math.sin(phi), math.sin(phi)*x + math.cos(phi)*y
        return (x,y)
    def arc_to_beziers(self, cx, cy, rx, ry, phi, w1, dw):
        # convert an SVG arc to 1-4 bezier segments
        segcnt = min(4,int(abs(dw) / (math.pi/2) - 0.00000001) + 1)
        beziers = []
        for i in range(segcnt):
            sw1 = w1 + dw / segcnt * i
            sdw = dw / segcnt
            p0 = self.arc_eval(cx, cy, rx, ry, phi, sw1)
            p3 = self.arc_eval(cx, cy, rx, ry, phi, sw1+sdw)
            a = math.sin(sdw)*(math.sqrt(4+3*math.tan(sdw/2)**2)-1)/3
            d1 = self.arc_deriv(rx, ry, phi, sw1)
            d2 = self.arc_deriv(rx, ry, phi, sw1+sdw)
            p1 = p0[0] + a*d1[0], p0[1] + a*d1[1]
            p2 = p3[0] - a*d2[0], p3[1] - a*d2[1]
            beziers.append(PathBezier4(p0, p1, p2, p3))
        return beziers
    def svg_arc_to_beziers(self, start, end, rx, ry, phi, fa, fs):
        # first convert endpoint format to center-and-radii format
        rx, ry = abs(rx), abs(ry)
        phi = phi % (2*math.pi)

        x1, y1 = start
        x2, y2 = end
        x1p = (x1 - x2) / 2
        y1p = (y1 - y2) / 2
        psin = math.sin(phi)
        pcos = math.cos(phi)
        x1p, y1p = pcos*x1p + psin*y1p, -psin*x1p + pcos*y1p
        foo = x1p**2 / rx**2 + y1p**2 / ry**2
        sr = ((rx**2 * ry**2 - rx**2 * y1p**2 - ry**2 * x1p**2) /
                    (rx**2 * y1p**2 + ry**2 * x1p**2))
        if foo > 1 or sr < 0:
            #print "fixup!",foo,sr
            rx = math.sqrt(foo)*rx
            ry = math.sqrt(foo)*ry
            sr = 0

        srt = math.sqrt(sr)
        if fa == fs:
            srt = -srt
        cxp, cyp = srt * rx * y1p / ry, -srt * ry * x1p / rx
        cx, cy = pcos*cxp + -psin*cyp, psin*cxp + pcos*cyp
        cx, cy = cx + (x1+x2)/2, cy + (y1+y2)/2

        va = ((x1p - cxp)/rx, (y1p - cyp)/ry)
        vb = ((-x1p - cxp)/rx, (-y1p - cyp)/ry)
        w1 = self.angle((1,0), va)
        dw = self.angle(va, vb) % (2*math.pi)
        if not fs:
            dw -= 2*math.pi

        # then do the actual approximation
        return self.arc_to_beziers(cx, cy, rx, ry, phi, w1, dw)

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
        subpath = SVGSubpath()
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
                subpath = SVGSubpath()
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
            elif ucmd == 'A':
                rx, ry = self.popcoord()
                phi = self.popnum() / 180.0 * math.pi
                fa = self.popnum() != 0
                fs = self.popnum() != 0
                end = self.popcoord(rel, cur)

                if cur == end:
                    cur = curcpc = curcpq = end
                    continue
                if rx == 0 or ry == 0:
                    subpath.add(PathLine(cur, end))
                    cur = curcpc = curcpq = end
                    continue

                for i in self.svg_arc_to_beziers(cur, end, rx, ry, phi, fa, fs):
                    subpath.add(i)
                cur = curcpc = curcpq = end

        if subpath.segments:
            self.subpaths.append(subpath)

class SVGPolyline(SVGPath):
    def __init__(self, data=None, close=False):
        self.subpaths = []
        if data:
            self.parse(data, close)
    def parse(self, data, close=False):
        ds = re.split(r"[ \r\n\t]*([-+]?\d+\.\d+[eE][+-]?\d+|[-+]?\d+\.\d+|[-+]?\.\d+[eE][+-]?\d+|[-+]?\.\d+|[-+]?\d+\.?[eE][+-]?\d+|[-+]?\d+\.?)[, \r\n\t]*", data)
        tokens = ds[1::2]
        if any(ds[::2]) or not all(ds[1::2]):
            raise ValueError("Invalid SVG path expression: %r"%data)

        self.tokens = tokens
        cur = None
        first = None
        subpath = SVGSubpath()
        while tokens:
            pt = self.popcoord()
            if first is None:
                first = pt
            if cur is not None:
                subpath.add(PathLine(cur, pt))
            cur = pt
        if close:
            subpath.add(PathLine(cur, first))
        self.subpaths.append(subpath)

class SVGReader(xml.sax.handler.ContentHandler):
    def doctype(self, name, pubid, system):
        pass
    def startDocument(self):
        self.matrix_stack = [(1,0,0,1,0,0)]
        self.style_stack = []
        self.defsdepth = 0
        self.pen = 0xffffff
        self.group_stack = [SVGGroup('root')]
        self.cur = self.root = self.group_stack[-1] 
    def endDocument(self):
        self.root.transform(self.tc)
    def startElement(self, name, attrs):
        if "inkscape:label" in attrs:
            id = attrs["inkscape:label"]
        elif "id" in attrs:
            id = attrs["id"]
        else:
            id = None
        if name == "svg":
            self.dx = self.dy = 0
            if 'viewBox' in list(attrs.keys()):
                self.dx, self.dy, self.width, self.height = list(map(float, attrs['viewBox'].split()))
            else:
                ws = attrs['width']
                hs = attrs['height']
                for r in ('px','pt','mm','in','cm','%'):
                    hs = hs.replace(r,"")
                    ws = ws.replace(r,"")
                self.width = float(ws)
                self.height = float(hs)
        elif name == "path":
            if 'transform' in list(attrs.keys()):
                self.transform(attrs['transform'])
            if self.defsdepth == 0 and self.isvisible(attrs):
                self.addPath(id, attrs['d'])
            if 'transform' in list(attrs.keys()):
                self.popmatrix()
        elif name in ("polyline","polygon"):
            if 'transform' in list(attrs.keys()):
                self.transform(attrs['transform'])
            if self.defsdepth == 0 and self.isvisible(attrs):
                self.addPolyline(id, attrs['points'], name == "polygon")
            if 'transform' in list(attrs.keys()):
                self.popmatrix()
        elif name == "line":
            if 'transform' in list(attrs.keys()):
                self.transform(attrs['transform'])
            if self.defsdepth == 0 and self.isvisible(attrs):
                x1, y1, x2, y2 = [float(attrs[x]) for x in ('x1','y1','x2','y2')]
                self.addLine(id, x1, y1, x2, y2)
            if 'transform' in list(attrs.keys()):
                self.popmatrix()
        elif name == "rect":
            if 'transform' in list(attrs.keys()):
                self.transform(attrs['transform'])
            if self.defsdepth == 0 and self.isvisible(attrs):
                x1, y1, w, h = [float(attrs[x]) for x in ('x','y','width','height')]
                self.addRect(id, x1, y1, x1+w, y1+h)
            if 'transform' in list(attrs.keys()):
                self.popmatrix()
        elif name == "circle":
            if 'transform' in list(attrs.keys()):
                self.transform(attrs['transform'])
            if self.defsdepth == 0 and self.isvisible(attrs):
                cx, cy, r = [float(attrs[x]) for x in ('cx','cy','r')]
                self.addCircle(id, cx, cy, r)
            if 'transform' in list(attrs.keys()):
                self.popmatrix()
        elif name == "ellipse":
            if 'transform' in list(attrs.keys()):
                self.transform(attrs['transform'])
            if self.defsdepth == 0 and self.isvisible(attrs):
                cx, cy, rx, ry = [float(attrs[x]) for x in ('cx','cy','rx','ry')]
                self.addEllipse(id, cx, cy, rx, ry)
            if 'transform' in list(attrs.keys()):
                self.popmatrix()
        elif name == 'g':
            if 'transform' in list(attrs.keys()):
                self.transform(attrs['transform'])
            else:
                self.pushmatrix((1,0,0,1,0,0))
            if 'style' in list(attrs.keys()):
                self.style_stack.append(attrs['style'])
            else:
                self.style_stack.append("")
            self.group_stack.append(SVGGroup(id))
            self.cur.add(self.group_stack[-1])
            self.cur = self.group_stack[-1]
        elif name in ('defs','clipPath'):
            self.defsdepth += 1
    def endElement(self, name):
        if name == 'g':
            self.popmatrix()
            self.style_stack.pop()
            self.group_stack.pop()
            self.cur = self.group_stack[-1]
        elif name in ('defs','clipPath'):
            self.defsdepth -= 1
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
        x -= self.width / 2.0 + self.dx
        y -= self.height / 2.0 + self.dy
        x = x / vw
        y = y / vh
        return (x,-y)
    def ts(self,coord):
        a,b,c,d,e,f = self.matrix_stack[-1]
        x,y = coord
        nx = a*x + c*y + e
        ny = b*x + d*y + f
        return (nx,ny)
    def transform(self, data):
        ds = re.split(r"[ \r\n\t]*([a-z]+\([^)]+\)|,)[ \r\n\t]*", data)
        tokens = []
        for v in ds:
            if v in ', \t':
                continue
            if v == '':
                continue
            if not re.match(r"[a-z]+\([^)]+\)", v):
                raise ValueError("Invalid SVG transform expression: %r (%r)"%(data,v))
            tokens.append(v)
        transforms = tokens

        mat = (1,0,0,1,0,0)
        for t in transforms:
            name,rest = t.split("(")
            if rest[-1] != ")":
                raise ValueError("Invalid SVG transform expression: %r (%r)"%(data,rest))
            args = list(map(float,re.split(r'[, \t]+',rest[:-1])))
            if name == 'matrix':
                mat = self.mmul(mat, args)
            elif name == 'translate':
                tx,ty = args
                mat = self.mmul(mat, (1,0,0,1,tx,ty))
            elif name == 'scale':
                if len(args) == 1:
                    sx,sy = args[0],args[0]
                else:
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
    def add_subpaths(self, name, p):
        if len(p.subpaths) == 1:
            path = p.subpaths[0]
            path.transform(self.ts)
            path.name = name
            path.color = self.pen
            self.cur.add(path)
        elif len(p.subpaths) > 1:
            g = SVGGroup(name)
            for path in p.subpaths:
                path.transform(self.ts)
                path.color = self.pen
                g.add(path)
            self.cur.add(g)
    def addPath(self, name, data):
        p = SVGPath(data)
        self.add_subpaths(name, p)
    def addPolyline(self, name, data, close=False):
        p = SVGPolyline(data, close)
        self.add_subpaths(name, p)
    def addLine(self, name, x1, y1, x2, y2):
        path = SVGSubpath(name, color=self.pen)
        path.add(PathLine((x1,y1), (x2,y2)))
        path.transform(self.ts)
        self.cur.add(path)
    def addRect(self, name, x1, y1, x2, y2):
        path = SVGSubpath(name, color=self.pen)
        path.add(PathLine((x1,y1), (x2,y1)))
        path.add(PathLine((x2,y1), (x2,y2)))
        path.add(PathLine((x2,y2), (x1,y2)))
        path.add(PathLine((x1,y2), (x1,y1)))
        path.transform(self.ts)
        self.cur.add(path)
    def addCircle(self, name, cx, cy, r):
        cp = 0.55228475 * r
        path = SVGSubpath(name, color=self.pen)
        path.add(PathBezier4((cx,cy-r), (cx+cp,cy-r), (cx+r,cy-cp), (cx+r,cy)))
        path.add(PathBezier4((cx+r,cy), (cx+r,cy+cp), (cx+cp,cy+r), (cx,cy+r)))
        path.add(PathBezier4((cx,cy+r), (cx-cp,cy+r), (cx-r,cy+cp), (cx-r,cy)))
        path.add(PathBezier4((cx-r,cy), (cx-r,cy-cp), (cx-cp,cy-r), (cx,cy-r)))
        path.transform(self.ts)
        self.cur.add(path)
    def addEllipse(self, name, cx, cy, rx, ry):
        cpx = 0.55228475 * rx
        cpy = 0.55228475 * ry
        path = SVGSubpath(name, color=self.pen)
        path.add(PathBezier4((cx,cy-ry), (cx+cpx,cy-ry), (cx+rx,cy-cpy), (cx+rx,cy)))
        path.add(PathBezier4((cx+rx,cy), (cx+rx,cy+cpy), (cx+cpx,cy+ry), (cx,cy+ry)))
        path.add(PathBezier4((cx,cy+ry), (cx-cpx,cy+ry), (cx-rx,cy+cpy), (cx-rx,cy)))
        path.add(PathBezier4((cx-rx,cy), (cx-rx,cy-cpy), (cx-cpx,cy-ry), (cx,cy-ry)))
        path.transform(self.ts)
        self.cur.add(path)
    def apply_gamma(self, c):
        GAMMA = 2.2
        r, g, b = c >> 16, (c >> 8) & 0xff, c & 0xff
        r = int(((r / 255.0) ** GAMMA) * 255)
        g = int(((g / 255.0) ** GAMMA) * 255)
        b = int(((b / 255.0) ** GAMMA) * 255)
        return (r<<16) | (g<<8) | b
    def isvisible(self, attrs):
        # skip elements with no stroke or fill
        # hacky but gets rid of some gunk
        style = ' '.join(self.style_stack)
        if 'style' in list(attrs.keys()):
            style += " %s"%attrs['style']
        stroke = re.findall(r'stroke:\s*#([0-9a-fA-F]{6})\b', style)
        if stroke:
            color = int(stroke[-1], 16)
            self.pen = self.apply_gamma(color)
        if 'fill' in list(attrs.keys()) and attrs['fill'] != "none":
            return True
        style = re.sub(r'fill:\s*none\s*(;?)','', style)
        style = re.sub(r'stroke:\s*none\s*(;?)','', style)
        if 'stroke' not in style and re.match(r'fill:\s*none\s',style):
            return False
        if re.match(r'display:\s*none', style):
            return False
        return True

def load_svg(path):
    handler = SVGReader()
    parser = xml.sax.make_parser()
    parser.setContentHandler(handler)
    parser.setFeature(xml.sax.handler.feature_external_ges, False)
    parser.parse(path)
    return handler.root
