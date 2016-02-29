# zoic
Extended Arnold camera shader

## Requirements

- arnold
- OpenImageIO (for arnold older than 4.2.9.0)
- qmake or [SCons](http://scons.org)


## Build

### With qmake
Edit the 'src/arnoldCamera.pro' file to setup the arnold (and optionally OpenImageIO) headers and libraries path.
If your arnold version is recent enough, also add the 'NO_OIIO' define. Then
```
cd src
qmake
make
```

### With SCons
```
scons with-arnold=/path/to/arnold
```
- if it complains about missing 'excons' module, you most probably didn't initialize the submodules
```
git submodule update --init
```
- if it complains about missing OpenImageIO, it means the provided arnold version doesn't provide the function for reading a full texture in one call. Provide additional flags to scons
```
scons [other options] with-oiio=/path/to/oiio
```
That will add the provided directory's 'include' subdirectory to the header path and 'lib' subdirectory to the library path.
You may also be more explicit using
```
scons [other options] with-oiio-inc=/path/to/oiio/include with-oiio-lib=/path/to/oiio/lib
```
- on windows, you may want to adjust the visual studio compiler version using mscver= flag (9.0 by default)
```
scons [other options] mscver=12.0 
```

**The flags passed to scons command are cached in a file name 'excons.cache' so that you don't need to reset them for subsequent builds. The missing flags will be fetched from the cache and the provided ones will replace the cache content.**

## Install

Set the following environment variables, replacing "$PATH_TO_ZOIC" with the actual path on your machine. 

### Linux/OSX

```
export ARNOLD_PLUGIN_PATH=$ARNOLD_PLUGIN_PATH:$PATH_TO_ZOIC/bin
export MTOA_TEMPLATES_PATH=$MTOA_TEMPLATES_PATH:$PATH_TO_ZOIC/ae
```

### Windows

```
set ARNOLD_PLUGIN_PATH=%ARNOLD_PLUGIN_PATH%;$PATH_TO_ZOIC/bin
set MTOA_TEMPLATES_PATH=%MTOA_TEMPLATES_PATH%;$PATH_TO_ZOIC/ae
```

It’s also possible to copy the files into your MtoA install, but I personally prefer the first option. Just copy the files like this:

- Files in the 'bin' folder go to [$MTOA_LOCATION]/shaders
- Files in the 'ae' folde go to [$MTOA_LOCATION]/scripts/mtoa/ui/ae 

## Credits
Special thanks to Marc-Antoine Desjardins for the help with the image sampling. I owe this guy quite a few beers by now.

Special thanks to Benedikt Bitterli for the information on optical vignetting.

Special thanks to Tom Minor for the major help with C++ (it was needed!)

Special thanks to Brian Scherbinski for the windows compile.

Special thanks to Gaetan Guidet for the C++ cleanup and improvements.
