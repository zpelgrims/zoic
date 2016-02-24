import sys
import excons
from excons.tools import arnold

env = excons.MakeBaseEnv()

oiio_inc, oiio_lib = excons.GetDirs("oiio", noexc=False)

zoic = {"name": "zoic",
        "type": "dynamicmodule",
        "ext": arnold.PluginExt(),
        "srcs": ["src/zoic.cpp"],
        "incdirs": [oiio_inc],
        "libdirs": [oiio_lib],
        "libs": ["OpenImageIO"],
        "install": {"": ["bin/zoic.mtd"]},
        "custom": [arnold.Require]}

excons.DeclareTargets(env, [zoic])
