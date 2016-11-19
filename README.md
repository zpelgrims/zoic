# zoic

Extended Arnold camera shader with options for:
	
	Raytraced lens model
	Optical Vignetting
	Image based bokeh shapes
	

## Install

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


## Credits

Special thanks to Marc-Antoine Desjardins for the help with the image sampling. I owe this guy quite a few beers by now.

Special thanks to Benedikt Bitterli for the information on optical vignetting.

Special thanks to Tom Minor for the major help with C++ (it was needed!)

Special thanks to Brian Scherbinski for the windows compile.

Special thanks to Gaetan Guidet for the C++ cleanup and improvements.
