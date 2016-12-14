from PIL import Image, ImageDraw, ImageFont
import math
import re
 
print "####### INITIALIZING PYTHON IMAGE DRAWING #######"
# GLOBAL VARS
SIZE = (4500, 750)
AAS = 4
LINEWIDTH = 10
TRANSLATEX = 2800 * AAS
TRANSLATEY = (SIZE[1] * AAS) / 2
PADDING = 10 * AAS
SCALE = 85
# font = ImageFont.truetype('C:\Windows\Fonts\Consola.ttf', 16 * AAS)
font = ImageFont.truetype('/Users/zpelgrims/Library/Fonts/Inconsolata.otf', 16 * AAS)
WHITE = (255, 255, 255)
RED = (int(0.9 * 255), int(0.4 * 255), int(0.5 * 255))
BLUE = (36, 71, 91)
GREEN = (int(0.5 * 255), int(0.7 * 255), int(0.25 * 255))
ORANGE = (int(0.8 * 255), int(0.4 * 255), int(0.15 * 255))
GREY = (150, 150, 150)
DARKGREY = (120, 120, 120)
 
 
# DATA VARS
print "PYTHON: Reading data file"
dataFile = open('/Volumes/ZENO_2016/projects/zoic/src/draw.zoic', 'r')
# dataFile = open('C:\ilionData\Users\zeno.pelgrims\Documents\zoic_compile/draw.zoic', 'r')
 
LENSMODEL = dataFile.readline()
LENSMODELSTRING = LENSMODEL[10:-2]
print "PYTHON: Lens model: ", LENSMODELSTRING
 
 
if LENSMODELSTRING == "KOLB":
 
                LENSES = dataFile.readline()
                LENSESSTRING = re.sub('[LENSES{}]', '', LENSES)
                LENSESSTRING = LENSESSTRING.rstrip()
                LENSESLIST = [float(n) for n in LENSESSTRING.split()]
 
                IORSTRING = dataFile.readline()
                IORSTRING = re.sub('[IOR{}]', '', IORSTRING)
                IORLIST = [float(n) for n in IORSTRING.split()]
 
                APERTUREELEMENTSTRING = dataFile.readline()
                APERTUREELEMENTSTRING = re.sub('[APERTUREELEMENT{}]', '', APERTUREELEMENTSTRING)
                APERTUREELEMENTLIST = [int(n) for n in APERTUREELEMENTSTRING.split()]
 
                APERTUREDISTANCESTRING = dataFile.readline()
                APERTUREDISTANCESTRING = re.sub('[APERTUREDISTANCE{}]', '', APERTUREDISTANCESTRING)
                APERTUREDISTANCELIST = [float(n) for n in APERTUREDISTANCESTRING.split()]
 
                APERTURESTRING = dataFile.readline()
                APERTURESTRING = re.sub('[APERTURE{}]', '', APERTURESTRING)
                APERTURELIST = [float(n) for n in APERTURESTRING.split()]
 
                APERTUREMAXSTRING = dataFile.readline()
                APERTUREMAXSTRING = re.sub('[APERTUREMAX{}]', '', APERTUREMAXSTRING)
                APERTUREMAXLIST = [float(n) for n in APERTUREMAXSTRING.split()]
                FOCUSDISTANCESTRING = dataFile.readline()
                FOCUSDISTANCESTRING = re.sub('[FOCUSDISTANCE{}]', '', FOCUSDISTANCESTRING)
                FOCUSDISTANCELIST = [float(n) for n in FOCUSDISTANCESTRING.split()]
                IMAGEDISTANCESTRING = dataFile.readline()
                IMAGEDISTANCESTRING = re.sub('[IMAGEDISTANCE{}]', '', IMAGEDISTANCESTRING)
                IMAGEDISTANCELIST = [float(n) for n in IMAGEDISTANCESTRING.split()]
 
                SENSORHEIGHTSTRING = dataFile.readline()
                SENSORHEIGHTSTRING = re.sub('[SENSORHEIGHT{}]', '', SENSORHEIGHTSTRING)
                SENSORHEIGHTLIST = [float(n) for n in SENSORHEIGHTSTRING.split()]
 
RAYSSTRING = dataFile.readline()
RAYSSTRING = re.sub('[RAYS{}]', '', RAYSSTRING)
RAYSLIST = [float(n) for n in RAYSSTRING.split()]
 
dataFile.close()
 
 
 
 
list1 = []
list2 = []
lenscounter = 0
 
def arc(draw, bbox, start, end, fill, width = LINEWIDTH, segments = 200):
    global list1
    global list2
    global lenscounter
    global IORLIST
    global APERTUREELEMENTLIST
 
    firstLastList = []
 
    # radians
    start *= math.pi / 180
    end *= math.pi / 180
 
    # angle step
    da = (end - start) / segments
 
    # shift end points with half a segment angle
    start -= da / 2
    end -= da / 2
 
    # ellips radii
    rx = (bbox[2] - bbox[0]) / 2
    ry = (bbox[3] - bbox[1]) / 2
 
    # box centre
    cx = bbox[0] + rx
    cy = bbox[1] + ry
 
    # segment length
    l = (rx + ry) * da / 2.0
 
    for i in range(segments):
 
        # angle centre
        a = start + (i + 0.5) * da
 
        # x,y centre
        x = cx + math.cos(a) * rx
        y = cy + math.sin(a) * ry
 
        # derivatives
        dx = -math.sin(a) * rx / (rx + ry)
        dy = math.cos(a) * ry / (rx + ry)
 
        if lenscounter != APERTUREELEMENTLIST[0]:
            draw.line([(x - dx * l, y - dy * l), (x + dx * l, y + dy * l)], fill=fill, width=width)
 
 
        if lenscounter == 0:
            if i == 0:
                list1.append((x - dx * l, y - dy * l))
 
            if i == segments - 1:
                list1.append((x - dx * l, y - dy * l))
 
 
        if lenscounter % 2 == 0:
            if i == 0:
                list2.append((x - dx * l, y - dy * l))
 
            if i == segments - 1:
                list2.append((x - dx * l, y - dy * l))
 
                if IORLIST[lenscounter] != 1.0 or lenscounter == 0:
                    draw.line([list1[0], list2[0]], fill = fill, width = width)
                    draw.line([list1[1], list2[1]], fill = fill, width = width)
 
                list1 = []
 
 
        elif lenscounter % 2 != 0:
            if i == 0:
                list1.append((x - dx * l, y - dy * l))
 
            if i == segments - 1:
                list1.append((x - dx * l, y - dy * l))
 
 
                if IORLIST[lenscounter] != 1.0:
                    draw.line([list1[0], list2[0]], fill = fill, width = width)
                    draw.line([list1[1], list2[1]], fill = fill, width = width)
 
                list2 = []
 
    lenscounter = lenscounter + 1
 
 
 
 
 
 
# IMAGE WITH ACTUAL RAYS
print "PYTHON: Creating actual rays image"
img = Image.new('RGB', (SIZE[0] * AAS, SIZE[1] * AAS), BLUE)
d = ImageDraw.Draw(img)
 
# ORIGIN LINES
print "PYTHON: ---- Drawing origin lines"
d.line([-5000 + TRANSLATEX, 0 + TRANSLATEY, 15000 + TRANSLATEX, 0 + TRANSLATEY],  DARKGREY, 5)
d.line([0 + TRANSLATEX, 9999 + TRANSLATEY, 0 + TRANSLATEX, -9999 + TRANSLATEY],  DARKGREY, 5)
d.text([0 + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING + 1000], "(0, 0)", DARKGREY, font)
d.text([0 + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING + 1100], "ORIGIN", DARKGREY, font)
 
# TEMPORARY FOCUS LINE, REMOVE!
d.line([(100 * AAS * SCALE + TRANSLATEX, 9999 * AAS * SCALE + TRANSLATEY), (100 * AAS * SCALE + TRANSLATEX, - 9999 * AAS * SCALE + TRANSLATEY)], ORANGE, 1)
 

# RAYS
print "PYTHON: ---- Drawing rays"
for count in range (0, len(RAYSLIST) / 4):
    d.line([RAYSLIST[(count * 4)] * AAS * SCALE + TRANSLATEX, RAYSLIST[(count * 4) + 1] * AAS * SCALE + TRANSLATEY, RAYSLIST[(count * 4) + 2] * AAS * SCALE + TRANSLATEX,  (RAYSLIST[(count * 4) + 3] * AAS * SCALE) + TRANSLATEY], WHITE, 1)
 

if LENSMODELSTRING == "KOLB":
 
                # FOCUS DISTANCE
                print "PYTHON: ---- Drawing focus line"
                d.line([(FOCUSDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX, 9999 + TRANSLATEY, (FOCUSDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX, -9999 + TRANSLATEY],  ORANGE, LINEWIDTH)
                d.text([(FOCUSDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING + 1000], "(" + str(FOCUSDISTANCELIST[0]) + ", 0)", ORANGE, font)
                d.text([(FOCUSDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING + 1100], "FOCUSDISTANCE", ORANGE, font)
               
                # IMAGE DISTANCE
                print "PYTHON: ---- Drawing image line"
                d.line([(IMAGEDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX, 9999 + TRANSLATEY, (IMAGEDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX, -9999 + TRANSLATEY],  ORANGE, LINEWIDTH)
                d.text([(IMAGEDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING + 1000], "(" + str(IMAGEDISTANCELIST[0]) + ", 0)", ORANGE, font)
                d.text([(IMAGEDISTANCELIST[0] * AAS) * SCALE + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING + 1100], "IMAGEDISTANCE", ORANGE, font)
 
                # LENS ELEMENTS
                print "PYTHON: ---- Drawing lens elements"
                for count in range (0, len(LENSESLIST) / 3):
                    arc(d, [((LENSESLIST[count * 3] * AAS) - (LENSESLIST[(count * 3) + 1]) * AAS) * SCALE + TRANSLATEX, (- LENSESLIST[(count * 3) + 1]) * AAS * SCALE + TRANSLATEY, ((LENSESLIST[count*3] * AAS) + (LENSESLIST[(count * 3) + 1] * AAS)) * SCALE + TRANSLATEX, (LENSESLIST[(count * 3) + 1] * AAS) * SCALE + TRANSLATEY], - LENSESLIST[(count * 3) + 2], LENSESLIST[(count * 3) + 2], RED)
 
                # APERTURE
                print "PYTHON: ---- Drawing aperture"
                d.line([(APERTUREDISTANCELIST[0] * AAS * SCALE + TRANSLATEX, (APERTUREMAXLIST[0] / 2.0) * AAS * SCALE + TRANSLATEY), (APERTUREDISTANCELIST[0] * AAS * SCALE + TRANSLATEX, APERTURELIST[0] * AAS * SCALE + TRANSLATEY)], GREY, LINEWIDTH)
                d.line([(APERTUREDISTANCELIST[0] * AAS * SCALE + TRANSLATEX, - (APERTUREMAXLIST[0] / 2.0) * AAS * SCALE + TRANSLATEY), (APERTUREDISTANCELIST[0] * AAS * SCALE + TRANSLATEX, - APERTURELIST[0] * AAS * SCALE + TRANSLATEY)], GREY, LINEWIDTH)
 
                # SENSOR
                print "PYTHON: ---- Drawing sensor"
                d.line([(IMAGEDISTANCELIST[0] * AAS * SCALE + TRANSLATEX, SENSORHEIGHTLIST[0] * AAS * SCALE + TRANSLATEY), (IMAGEDISTANCELIST[0] * AAS * SCALE + TRANSLATEX, - SENSORHEIGHTLIST[0] * AAS * SCALE + TRANSLATEY)], ORANGE, LINEWIDTH * 3)
 

print "PYTHON: ---- Anti-aliasing image"
img.thumbnail(SIZE, Image.ANTIALIAS)
 
print "PYTHON: Saving image"
# img.save('C:\ilionData\Users\zeno.pelgrims\Documents\zoic_compile\lensDrawing.png','png')
img.save('/Volumes/ZENO_2016/projects/zoic/tests/images/lensDrawing.png','png')
