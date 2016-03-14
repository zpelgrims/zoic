import os
import sys
import glob
import excons
from excons.tools import arnold

excons.SetArgument("no-arch", 1)

env = excons.MakeBaseEnv()

version = "1.1.2"

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

def Package(env, target, source):
    global version
    
    outdir = excons.OutputBaseDirectory()
    shaderbin = str(target[0])
    shadermtd = os.path.splitext(shaderbin)[0] + ".mtd"
    
    # Create RELEASE.txt file
    arniver = arnold.Version(asString=True)
    with open(outdir+"/RELEASE.txt", "w") as outf:
        with open("RELEASE.txt", "r") as inf:
            for l in inf.readlines():
                outf.write(l.replace("<version>", version).replace("<arnold_version>", arniver))
    
    # Make package tarball
    packagesuffix = ""
    packagetype = ARGUMENTS.get("package-type", None)
    
    if sys.platform == "win32":
        packagesuffix = "win"
        
        if packagetype is None:
            packagetype = "zip"
    
    else:
        if sys.platform == "darwin":
            packagesuffix = "osx"
        else:
            packagesuffix = "linux"
        
        if packagetype is None:
            packagetype = "tgz"
    
    outpath = outdir + "/zoic-%s_%s.%s" % (version, packagesuffix, packagetype)
    
    if packagetype == "zip":
        import zipfile
        
        def r_write(f, src, dst):
            if os.path.isdir(src):
                for item in glob.glob(src+"/*"):
                    r_write(f, item, dst+"/"+os.path.basename(item))
            else:
                f.write(src, dst)
        
        with zipfile.ZipFile(outpath, "w") as f:
            f.write(shaderbin, "shaders/" + os.path.basename(shaderbin))
            f.write(shadermtd, "shaders/" + os.path.basename(shadermtd))
            r_write(f, outdir + "/maya", "maya")
            r_write(f, outdir + "/c4d", "c4d")
            f.write(outdir + "/RELEASE.txt", "RELEASE.txt")
    
    elif packagetype == "tgz":
        import tarfile
        
        with tarfile.open(outpath, "w:gz") as f:
            f.add(shaderbin, "shaders/" + os.path.basename(shaderbin))
            f.add(shadermtd, "shaders/" + os.path.basename(shadermtd))
            f.add(outdir + "/maya", "maya")
            f.add(outdir + "/c4d", "c4d")
            f.add(outdir + "/RELEASE.txt", "RELEASE.txt")
    
    else:
        print("Unsupported package type '%s'" % packagetype)
    
    return None

zoic = {"name": "zoic",
        "type": "dynamicmodule",
        "prefix": "shaders",
        "ext": arnold.PluginExt(),
        "srcs": ["src/zoic.cpp"],
        "defs": defs,
        "incdirs": incdirs,
        "libdirs": libdirs,
        "libs": libs,
        "install": {"shaders": ["src/zoic.mtd"],
                    "maya": glob.glob("maya/*"),
                    "c4d": glob.glob("c4d/*")},
        "post": [] if int(ARGUMENTS.get("package", "0")) == 0 else [Package],
        "custom": [arnold.Require]}

excons.DeclareTargets(env, [zoic])
