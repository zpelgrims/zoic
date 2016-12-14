from PIL import Image, ImageDraw
import math
import re
 
print "####### INITIALIZING PYTHON IMAGE DRAWING #######"


SIZE = (2048, 2048)
AAS = 4
LINEWIDTH = 10
TRANSLATEX = (SIZE[0] * AAS) / 2
TRANSLATEY = (SIZE[1] * AAS) / 2

SCALE = 70

WHITE = (255, 255, 255)
RED = (int(0.9 * 255), int(0.4 * 255), int(0.5 * 255))
BLUE = (36, 71, 91)
GREEN = (int(0.5 * 255), int(0.7 * 255), int(0.25 * 255))
ORANGE = (int(0.8 * 255), int(0.4 * 255), int(0.15 * 255))
GREY = (150, 150, 150)
DARKGREY = (120, 120, 120)





# ----------------------------------- READ IN DATA -----------------------------------------
"""dataFile = open('C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/testApertures.zoic', 'r')"""
dataFile = open('/Volumes/ZENO_2016/projects/zoic/src/testApertures.zoic', 'r')
GTPOINTSLIST = []
SSPOINTSLIST = []

i = 0
while i < 99:
	POINTS = dataFile.readline()
	if POINTS.startswith('G'):
		GTPOINTSSTRING = POINTS[3:]
		GTPOINTSLIST.append([float(n) for n in GTPOINTSSTRING.split()])
	else:
		SSPOINTSSTRING = POINTS[3:]
		SSPOINTSLIST.append([float(n) for n in SSPOINTSSTRING.split()])
	i = i + 1



# ----------------------------- BG DRAWING -------------------------------------
img_bg = Image.new('RGBA', (SIZE[0] * AAS, SIZE[1] * AAS), BLUE)
d_bg = ImageDraw.Draw(img_bg)

i = 0
posx = -12
posy = -12

while i < 49:
	d_bg.ellipse([((-1.0 + posx) * AAS * SCALE + TRANSLATEX), ((-1.0 + posy) * AAS * SCALE + TRANSLATEY), ((1.0 + posx) * AAS * SCALE + TRANSLATEX), ((1.0 + posy) * AAS * SCALE + TRANSLATEY)], DARKGREY, DARKGREY)
	posy = posy + 4
	i += 1

	if i % 7 == 0:
		posx = posx + 4
		posy = -12



# ----------------------------- GROUND TRUTH DRAWING -------------------------------------
img_gt = Image.new('RGBA', (SIZE[0] * AAS, SIZE[1] * AAS))
img_gt_bg = img_bg.copy()
d_gt = ImageDraw.Draw(img_gt)

i = 0
posx = -12
posy = -12

while i < len(GTPOINTSLIST):

	print "GT DRAW PERCENTAGE: ", int(float(i) / float(len(SSPOINTSLIST)) * 100.0)

	j = 0
	
	while j < len(GTPOINTSLIST[i]):
		d_gt.ellipse([((GTPOINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) - 5, ((GTPOINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) - 5, ((GTPOINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) + 5, ((GTPOINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) + 5], ORANGE, ORANGE)
		j+=2

	posy = posy + 4
	i += 1

	if i % 7 == 0:
		posx = posx + 4
		posy = -12


img_gt_bg.paste(img_gt, (0, 0), img_gt)


print "PYTHON: ---- Anti-aliasing image"
img_gt_bg.thumbnail(SIZE, Image.ANTIALIAS)

print "PYTHON: Saving image"
"""img_gt_bg.save('C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/sampling_GT.png','png')"""
img_gt_bg.save('/Volumes/ZENO_2016/projects/zoic/tests/images/sampling_GT.png','png')






# ----------------------------- SMART SAMPLING DRAWING -------------------------------------
img_ss = Image.new('RGBA', (SIZE[0] * AAS, SIZE[1] * AAS))
d_ss = ImageDraw.Draw(img_ss)
img_ss_bg = img_bg.copy()


i = 0
posx = -12
posy = -12

while i < len(SSPOINTSLIST):

	print "SS DRAW PERCENTAGE: ", int(float(i) / float(len(SSPOINTSLIST)) * 100.0)

	j = 0
	
	while j < len(SSPOINTSLIST[i]):
		#points
		d_ss.ellipse([((SSPOINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) - 5, ((SSPOINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) - 5, ((SSPOINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) + 5, ((SSPOINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) + 5], WHITE, WHITE)
		#centroids
		# d_ss.ellipse([((POINTSLIST[i][j+2] + posx) * AAS * SCALE + TRANSLATEX) - 20, ((POINTSLIST[i][j+3] + posy) * AAS * SCALE + TRANSLATEY) - 20, ((POINTSLIST[i][j+2] + posx) * AAS * SCALE + TRANSLATEX) + 20, ((POINTSLIST[i][j+3] + posy) * AAS * SCALE + TRANSLATEY) + 20], WHITE,WHITE)
		# vertices
		#d_ss.ellipse([((POINTSLIST[i][j+4] + posx) * AAS * SCALE + TRANSLATEX) - 10, ((POINTSLIST[i][j+5] + posy) * AAS * SCALE + TRANSLATEY) - 10, ((POINTSLIST[i][j+4] + posx) * AAS * SCALE + TRANSLATEX) + 10, ((POINTSLIST[i][j+5] + posy) * AAS * SCALE + TRANSLATEY) + 10], WHITE,WHITE)
		#d_ss.ellipse([((POINTSLIST[i][j+6] + posx) * AAS * SCALE + TRANSLATEX) - 10, ((POINTSLIST[i][j+7] + posy) * AAS * SCALE + TRANSLATEY) - 10, ((POINTSLIST[i][j+6] + posx) * AAS * SCALE + TRANSLATEX) + 10, ((POINTSLIST[i][j+7] + posy) * AAS * SCALE + TRANSLATEY) + 10], WHITE,WHITE)

		#d_ss.rectangle([((POINTSLIST[i][j+8] + posx) * AAS * SCALE + TRANSLATEX), ((POINTSLIST[i][j+9] + posy) * AAS * SCALE + TRANSLATEY), ((POINTSLIST[i][j+10] + posx) * AAS * SCALE + TRANSLATEX), ((POINTSLIST[i][j+11] + posy) * AAS * SCALE + TRANSLATEY)], 0, WHITE)
		#j += 12

		j+=2

	posy = posy + 4
	i += 1

	if i % 7 == 0:
		posx = posx + 4
		posy = -12



img_ss_bg.paste(img_ss, (0, 0), img_ss)

print "PYTHON: ---- Anti-aliasing image"
img_ss_bg.thumbnail(SIZE, Image.ANTIALIAS)

print "PYTHON: Saving image"
"""img_ss_bg.save('C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/sampling_SS.png','png')"""
img_ss_bg.save('/Volumes/ZENO_2016/projects/zoic/tests/images/sampling_SS.png','png')





# ----------------------------- COMBINED SAMPLING DRAWING -------------------------------------
img_combined_bg_mask = Image.new("L", (SIZE[0] * AAS, SIZE[1] * AAS), 120)
img_combined_bg_mask.thumbnail(SIZE, Image.ANTIALIAS)

img_combined_bg = Image.composite(img_ss_bg, img_gt_bg, img_combined_bg_mask)

print "PYTHON: ---- Anti-aliasing image"
img_combined_bg.thumbnail(SIZE, Image.ANTIALIAS)

print "PYTHON: Saving image"
"""img_combined_bg.save('C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/sampling_COMPARISON.png','png')"""
img_combined_bg.save('/Volumes/ZENO_2016/projects/zoic/tests/images/sampling_COMPARISON.png','png')