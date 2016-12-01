from PIL import Image, ImageDraw, ImageFont
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


dataFile = open('C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/testApertures.zoic', 'r')
"""dataFile = open('/Volumes/ZENO_2016/projects/zoic/src/testApertures.zoic', 'r')"""
POINTSLIST = []


i = 0
while i < 49:
	POINTS = dataFile.readline()
	POINTSLIST.append([float(n) for n in POINTS.split()])
	i = i + 1


# IMAGE WITH ACTUAL RAYS
img = Image.new('RGB', (SIZE[0] * AAS, SIZE[1] * AAS), BLUE)
d = ImageDraw.Draw(img)


i = 0
posx = -12
posy = -12


while i < len(POINTSLIST):

	print "DRAW PERCENTAGE: ", int(float(i) / float(len(POINTSLIST)) * 100.0)

	j = 0

	d.ellipse([((-1.5 + posx) * AAS * SCALE + TRANSLATEX), ((-1.5 + posy) * AAS * SCALE + TRANSLATEY), ((1.5 + posx) * AAS * SCALE + TRANSLATEX), ((1.5 + posy) * AAS * SCALE + TRANSLATEY)], DARKGREY, DARKGREY)

	
	while j < len(POINTSLIST[i]):
		#points
		d.ellipse([((POINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) - 5, ((POINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) - 5, ((POINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) + 5, ((POINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) + 5], ORANGE,ORANGE)
		#centroids
		# d.ellipse([((POINTSLIST[i][j+2] + posx) * AAS * SCALE + TRANSLATEX) - 20, ((POINTSLIST[i][j+3] + posy) * AAS * SCALE + TRANSLATEY) - 20, ((POINTSLIST[i][j+2] + posx) * AAS * SCALE + TRANSLATEX) + 20, ((POINTSLIST[i][j+3] + posy) * AAS * SCALE + TRANSLATEY) + 20], WHITE,WHITE)
		# vertices
		#d.ellipse([((POINTSLIST[i][j+4] + posx) * AAS * SCALE + TRANSLATEX) - 10, ((POINTSLIST[i][j+5] + posy) * AAS * SCALE + TRANSLATEY) - 10, ((POINTSLIST[i][j+4] + posx) * AAS * SCALE + TRANSLATEX) + 10, ((POINTSLIST[i][j+5] + posy) * AAS * SCALE + TRANSLATEY) + 10], WHITE,WHITE)
		#d.ellipse([((POINTSLIST[i][j+6] + posx) * AAS * SCALE + TRANSLATEX) - 10, ((POINTSLIST[i][j+7] + posy) * AAS * SCALE + TRANSLATEY) - 10, ((POINTSLIST[i][j+6] + posx) * AAS * SCALE + TRANSLATEX) + 10, ((POINTSLIST[i][j+7] + posy) * AAS * SCALE + TRANSLATEY) + 10], WHITE,WHITE)

		#d.rectangle([((POINTSLIST[i][j+8] + posx) * AAS * SCALE + TRANSLATEX), ((POINTSLIST[i][j+9] + posy) * AAS * SCALE + TRANSLATEY), ((POINTSLIST[i][j+10] + posx) * AAS * SCALE + TRANSLATEX), ((POINTSLIST[i][j+11] + posy) * AAS * SCALE + TRANSLATEY)], 0, WHITE)
		#j += 12

		j+=2


	""" ground truth drawing
	while j < len(POINTSLIST[i]):
		#points
		d.ellipse([((POINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) - 5, ((POINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) - 5, ((POINTSLIST[i][j] + posx) * AAS * SCALE + TRANSLATEX) + 5, ((POINTSLIST[i][j+1] + posy) * AAS * SCALE + TRANSLATEY) + 5], ORANGE,ORANGE)
		j += 2
	"""



	posy = posy + 4
	i += 1

	if i % 7 == 0:
		posx = posx + 4
		posy = -12


print "PYTHON: ---- Anti-aliasing image"
img.thumbnail(SIZE, Image.ANTIALIAS)

print "PYTHON: Saving image"
img.save('C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/sampling_new.png','png')
"""img.save('/Volumes/ZENO_2016/projects/zoic/tests/images/sampling.png','png')"""
