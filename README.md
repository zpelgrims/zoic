### READ THE DOCUMENTATION [HERE](http://zenopelgrims.com/zoic/) 
<br>
<div align="center">
  <img src="http://zenopelgrims.com/wp-content/uploads/2016/01/raytraced_50mm_1.4f.png"><br><br>
</div>

Extended Arnold camera shader with options for:
	
	Raytraced lens model
	Emperical Optical Vignetting
	Image based bokeh shapes

<br>
ZOIC 2.0 provides two different lens models, a new raytraced model which reads in lens description files often found in optics literature and lens patents, and the classical thin-lens approximation with options for emperical optical vignetting. Both models serve their own purposes, although in general the new raytraced model should be preferred at all times where photorealism is required. The thin lens approximation will always be slightly faster.

This realistic lens model reads in lens descriptions found in lens patents and books on optics. This data is used to trace the camera rays through that virtual lens. The model is based on a paper by Kolb et al [1995] and comes with some advantages over the thin-lens model, which by the way, is quite often a criminal approximation to how real lenses work:

	Physically plausible optical vignetting
	Physically plausible lens distortion
	Physically plausible bokeh shapes due to the lens geometry
	Non-planar focal field due to lens curvature
	Focus breathing – adjusting focus results in slightly shifted the focal length due to the way lens moves in respect to the sensor
	Correct image formation for wide angle lenses

Essentially, this should bring you one step closer to creating pretty, believable photographic images.

<br>
<div align="center">
  <img src="http://zenopelgrims.com/wp-content/uploads/2016/01/lensDrawing_focalLength_100-1.png"><br>
</div>

An example of how the rays are traced through the lens elements.
<br>


## INSTALLATION

Set the following environment variables, replacing "$PATH_TO_ZOIC" with the actual path on your machine. 

### Linux/OSX

```
export ARNOLD_PLUGIN_PATH=$ARNOLD_PLUGIN_PATH:$PATH_TO_ZOIC/shaders
export MTOA_TEMPLATES_PATH=$MTOA_TEMPLATES_PATH:$PATH_TO_ZOIC/maya/ae
export MAYA_SCRIPT_PATH=$MAYA_SCRIPT_PATH:$PATH_TO_ZOIC/maya/scripts
```

### Windows

```
set ARNOLD_PLUGIN_PATH=%ARNOLD_PLUGIN_PATH%;$PATH_TO_ZOIC/bin
set MTOA_TEMPLATES_PATH=%MTOA_TEMPLATES_PATH%;$PATH_TO_ZOIC/ae
set MAYA_SCRIPT_PATH=%MAYA_SCRIPT_PATH%;$PATH_TO_ZOIC/maya/scripts
```

It’s also possible to copy the files into your MtoA install, but I personally prefer the first option. Just copy the files like this:

- Files in the 'shaders' folder go to [$MTOA_LOCATION]/shaders
- Files in the 'maya/ae' folder go to [$MTOA_LOCATION]/scripts/mtoa/ui/ae 


## SPECIAL THANKS

	Marc-Antoine Desjardins for the help on the image sampling
	Benedikt Bitterli for the information on emperical optical vignetting
	Tom Minor for getting me started with C++
	Brian Scherbinski for the initial Windows compile
	Gaetan Guidet for the C++ cleanup and improvements