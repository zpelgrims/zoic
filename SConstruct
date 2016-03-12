import sys
import glob
import excons
from excons.tools import arnold

excons.SetArgument("no-arch", 1)

env = excons.MakeBaseEnv()

if sys.platform != "win32":
    env.Append(CPPFLAGS=" -Wno-unused-parameter")


# Arnold 4.2.9.0 provides api AiTextureLoad to read texture data
# Arnold 4.2.10.0 adds a new parameter to the function above

defs = []
incdirs = []
libdirs = []
libs = []

arniver = arnold.Version()
if arniver[0] < 4 or (arniver[0] == 4 and (arniver[1] < 2 or (arniver[1] == 2 and arniver[3] < 9))):
    oiio_inc, oiio_lib = excons.GetDirs("oiio", noexc=False)
    incdirs.append(oiio_inc)
    libdirs.append(oiio_lib)
    libs.append("OpenImageIO")
else:
    oiio_inc, oiio_lib = excons.GetDirs("oiio", noexc=True, silent=True)
    if oiio_inc or oiio_lib:
        incdirs.append(oiio_inc)
        libdirs.append(oiio_lib)
        libs.append("OpenImageIO")
    else:
        defs.append("NO_OIIO")

zoic = {"name": "zoic",
        "type": "dynamicmodule",
        "prefix": "bin",
        "ext": arnold.PluginExt(),
        "srcs": ["src/zoic.cpp"],
        "defs": defs,
        "incdirs": incdirs,
        "libdirs": libdirs,
        "libs": libs,
        "install": {"bin": ["src/zoic.mtd"],
                    "scripts": glob.glob("maya/scripts/*.mel"),
                    "ae": ["maya/ae/aiZoicTemplate.py"]},
        "custom": [arnold.Require]}

excons.DeclareTargets(env, [zoic])
