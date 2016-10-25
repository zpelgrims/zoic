from PIL import Image, ImageDraw, ImageFont
import math
import re


"""

IDEAL FORMAT

LENSES{lensCenter,radius,angle}
FOCUSDISTANCE{focusDistance}
IMAGEDISTANCE{imageDistance}
RAYS{x1,y1,x2,y2}

"""




# GLOBAL VARS
SIZE = (2500, 500)
AAS = 1
LINEWIDTH = 4
TRANSLATEX = 100 * AAS
TRANSLATEY = (SIZE[1] * AAS) / 2
PADDING = 10 * AAS
SCALE = 45
# font = ImageFont.truetype('C:\Windows\Fonts\Consola.ttf', 16 * AAS)
font = ImageFont.truetype('/Users/zpelgrims/Library/Fonts/Inconsolata.otf', 16 * AAS)
WHITE = (255, 255, 255)
RED = (int(0.9 * 255), int(0.4 * 255), int(0.5 * 255))
BLUE = (int(0.039 * 255), int(0.175 * 255), int(0.246 * 255))
GREEN = (int(0.5 * 255), int(0.7 * 255), int(0.25 * 255))
ORANGE = (int(0.8 * 255), int(0.4 * 255), int(0.15 * 255))


# DATA VARS
FOCUSDISTANCE = 50 * AAS

dataFile = open('/Volumes/ZENO_2016/projects/zoic/src/draw.zoic', 'r')
LENSES = dataFile.readline()

LENSESSTRING = re.sub('[LENSES{}]', '', LENSES)
LENSESSTRING = LENSESSTRING.rstrip()

LENSESLIST = [float(n) for n in LENSESSTRING.split()]
#for i in range (0, len(LENSESLIST)):
#    LENSESLIST[i] = LENSESLIST[i] * SCALE
print LENSESLIST


FOCUSDISTANCESTRING = dataFile.readline()
FOCUSDISTANCESTRING = re.sub('[FOCUSDISTANCE{}]', '', FOCUSDISTANCESTRING)
FOCUSDISTANCELIST = [float(n) for n in FOCUSDISTANCESTRING.split()]
#for i in range (0, len(FOCUSDISTANCELIST)):
#    FOCUSDISTANCELIST[i] = FOCUSDISTANCELIST[i] * SCALE
print FOCUSDISTANCELIST

IMAGEDISTANCESTRING = dataFile.readline()
IMAGEDISTANCESTRING = re.sub('[IMAGEDISTANCE{}]', '', IMAGEDISTANCESTRING)
IMAGEDISTANCELIST = [float(n) for n in IMAGEDISTANCESTRING.split()]
#for i in range (0, len(IMAGEDISTANCELIST)):
#    IMAGEDISTANCELIST[i] = IMAGEDISTANCELIST[i] * SCALE
print IMAGEDISTANCELIST

RAYSSTRING = dataFile.readline()
RAYSSTRING = re.sub('[RAYS{}]', '', RAYSSTRING)
RAYSLIST = [float(n) for n in RAYSSTRING.split()]
#for i in range (0, len(RAYSLIST)):
#    RAYSLIST[i] = RAYSLIST[i] * SCALE
print RAYSLIST

dataFile.close()



def arc(draw, bbox, start, end, fill, width=LINEWIDTH, segments=200):
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
    l = (rx+ry) * da / 2.0

    for i in range(segments):

        # angle centre
        a = start + (i+0.5) * da

        # x,y centre
        x = cx + math.cos(a) * rx
        y = cy + math.sin(a) * ry

        # derivatives
        dx = -math.sin(a) * rx / (rx+ry)
        dy = math.cos(a) * ry / (rx+ry)

        draw.line([(x-dx*l,y-dy*l), (x+dx*l, y+dy*l)], fill=fill, width=width)


img = Image.new('RGB', (SIZE[0] * AAS, SIZE[1] * AAS), BLUE)
d = ImageDraw.Draw(img)

# ORIGIN LINES
d.line([-9999 + TRANSLATEX, 0 + TRANSLATEY, 9999 + TRANSLATEX, 0 + TRANSLATEY],  WHITE, LINEWIDTH)
d.line([0 + TRANSLATEX, 9999 + TRANSLATEY, 0 + TRANSLATEX, -9999 + TRANSLATEY],  WHITE, LINEWIDTH)
d.text([0 + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING], "(0, 0)", WHITE, font)

# FOCUS DISTANCE
d.line([FOCUSDISTANCE * SCALE + TRANSLATEX, 9999 + TRANSLATEY, FOCUSDISTANCE * SCALE + TRANSLATEX, -9999 + TRANSLATEY],  ORANGE, LINEWIDTH)
d.text([FOCUSDISTANCE * SCALE + TRANSLATEX + PADDING, 0 + TRANSLATEY + PADDING], "(" + str(FOCUSDISTANCE) + ", 0)", ORANGE, font)

for count in range (0, len(LENSESLIST) / 3):
    #print LENSESLIST[count * 3]
    #print LENSESLIST[(count * 3) + 1]
    #print LENSESLIST[(count * 3) + 2]
    arc(d, [ (LENSESLIST[count * 3] - LENSESLIST[(count * 3) + 1]) * SCALE + TRANSLATEX, (- LENSESLIST[(count * 3) + 1]) * SCALE + TRANSLATEY, (LENSESLIST[count*3] + LENSESLIST[(count * 3) + 1]) * SCALE + TRANSLATEX, (LENSESLIST[(count * 3) + 1]) * SCALE + TRANSLATEY], - LENSESLIST[(count * 3) + 2], LENSESLIST[(count * 3) + 2], RED)


for count in range (0, len(RAYSLIST) / 4):
    #print RAYSLIST[count * 4]
    #print RAYSLIST[(count * 4) + 1]
    #print RAYSLIST[(count * 4) + 2]
    #print RAYSLIST[(count * 4) + 3]
    d.line([RAYSLIST[count * 4] * SCALE + TRANSLATEX, RAYSLIST[(count * 4) + 1] * SCALE + TRANSLATEY, RAYSLIST[(count * 4) + 2] * SCALE * TRANSLATEX,  RAYSLIST[(count * 4) + 3] * SCALE * TRANSLATEY], WHITE, 1)

#sphereCenter = (300, 0)
#sphereRadius = 800/2 * AAS # sphereRadius * AAS
#arc(d, [sphereCenter[0] - sphereRadius + TRANSLATEX, sphereCenter[1] - sphereRadius + TRANSLATEY, sphereCenter[0] + sphereRadius + TRANSLATEX, sphereCenter[1] + sphereRadius + TRANSLATEY], -50, 50, RED)

img.thumbnail(SIZE, Image.ANTIALIAS)

# img.save('C:\ilionData\Users\zeno.pelgrims\Documents\pillow_test\lensDrawing.png','png')
img.save('/Volumes/ZENO_2016/projects/zoic/tests/images/lensDrawing.png','png')

