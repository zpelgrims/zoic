// ZOIC - Extended Arnold camera shader with options for:
// Refracting through lens elements read from real life lens data (physically accurate lens distortion and optical vignetting)
// Image based bokeh shapes
// Emperical optical vignetting using the thin-lens equation.


// Special thanks to Marc-Antoine Desjardins for the help on the image sampling
// Special thanks to Benedikt Bitterli for the information on emperical optical vignetting
// Special thanks to Tom Minor for the help with C++ (it was needed!)
// Special thanks to Gaetan Guidet for the C++ cleanup on Github.


// (C) Zeno Pelgrims, www.zenopelgrims.com



// TODO

// Get initial sampling coordinates right
// get focus right (.. then it works..)
// assign variable to height to shoot rays on
// convert coordinate system in functions where i do the lineline intersection (x is z and z is x)
// Make sure all units are the same (eg kolb is in mm whilst thin lens is in cm..)
// Add colours to output ("\x1b[1;36m ..... \e[0m")
// precompute as much as possible (sphere centers, ...)

/* WHAT THE HELL, WHY IN 3's??? At least that means i can lower the rendertime by a factor of 3 too.
 * OR IS THIS AN OUTPUT ERROR? I DONT THINK SO

00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: -7.416297 11.924154 143.458160, 0.092027 -0.007381 0.995729
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: -7.416273 11.924155 143.458160, 0.092026 -0.007381 0.995729
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: -7.416299 11.924183 143.458145, 0.092027 -0.007381 0.995729
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: 6.245546 -23.701040 139.793015, 0.079063 0.024819 0.996561
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: 6.245576 -23.701036 139.793015, 0.079062 0.024819 0.996561
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: 6.245547 -23.701010 139.793030, 0.079063 0.024818 0.996561
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: 8.200994 6.395137 144.235397, 0.077386 -0.001935 0.997000
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: 8.201020 6.395138 144.235382, 0.077385 -0.001936 0.996999
00:00:00   142MB         |   [ZOIC] hitpoint && raydirection: 8.200998 6.395163 144.235382, 0.077386 -0.001936 0.996999
00:00:00   143MB         |   [ZOIC] hitpoint && raydirection: -35.381058 2.851584 89.898521, -5.595987 -5.595987 81.472694
00:00:00   143MB         |   [ZOIC] hitpoint && raydirection: -35.381004 2.851586 89.898499, -5.595987 -5.595987 81.472694
00:00:00   143MB         |   [ZOIC] hitpoint && raydirection: -35.381058 2.851634 89.898514, -5.595987 -5.595987 81.472694

*/
#include <ai.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <numeric>

// tmp debug stuff
#include <pngwriter.h>
pngwriter png(7200, 1000, 0.3,"/Volumes/ZENO_2016/projects/zoic/tests/images/lens_rays.png");

int counter = 0;
bool draw = true;
float scale = 5.0;
float translateX = 300.0;
float translateY = 500.0;


#ifdef NO_OIIO
// AiTextureLoad function introduced in arnold 4.2.9.0 was modified in 4.2.10.0
// Figure out the right one to call at compile time
#  if AI_VERSION_ARCH_NUM > 4
#   define AITEXTURELOAD_PROTO2
#  else
#    if AI_VERSION_ARCH_NUM == 4
#      if AI_VERSION_MAJOR_NUM > 2
#        define AITEXTURELOAD_PROTO2
#      else
#        if AI_VERSION_MAJOR_NUM == 2
#          if AI_VERSION_MINOR_NUM >= 10
#            define AITEXTURELOAD_PROTO2
#          endif
#          if AI_VERSION_MINOR_NUM == 9
#            define AITEXTURELOAD_PROTO1
#          endif
#        endif
#      endif
#    endif
#  endif
#  ifdef AITEXTURELOAD_PROTO2
inline bool LoadTexture(const AtString path, void *pixelData){
    return AiTextureLoad(path, true, 0, pixelData);
}
#  else
#    ifdef AITEXTURELOAD_PROTO1
inline bool LoadTexture(const AtString path, void *pixelData){
    return AiTextureLoad(path, true,  pixelData);
}
#    else
inline bool LoadTexture(const AtString path, void *pixelData){
    AiMsgError("Current arnold version doesn't have texture loading API");
    return false;
}
#    endif
#  endif
#else
#  include <OpenImageIO/imageio.h>
#endif

#ifdef _DEBUG
#  define DEBUG_ONLY(block) block
#else
#  define DEBUG_ONLY(block)
#endif

// Arnold thingy
AI_CAMERA_NODE_EXPORT_METHODS(zoicMethods)

#define _sensorWidth  (params[0].FLT)
#define _sensorHeight  (params[1].FLT)
#define _focalLength  (params[2].FLT)
#define _fStop  (params[3].FLT)
#define _focalDistance  (params[4].FLT)
#define _useImage  (params[5].BOOL)
#define _bokehPath (params[6].STR)
#define _kolb (params[7].BOOL)
#define _lensDataPath (params[8].STR)
#define _kolbSamplingMethod (params[9].BOOL)
#define _useDof  (params[10].BOOL)
#define _opticalVignettingDistance  (params[11].FLT)
#define _opticalVignettingRadius  (params[12].FLT)
#define _highlightWidth  (params[13].FLT)
#define _highlightStrength  (params[14].FLT)
#define _exposureControl (params[15].FLT)



struct arrayCompare{
    const float *values;
    inline arrayCompare(const float *_values) :values(_values) {}
    inline bool operator()(int _lhs, int _rhs) const{
        return values[_lhs] > values[_rhs];
    }
};

class imageData{
private:
    int x, y;
    int nchannels;
    float *pixelData;
    float *cdfRow;
    float *cdfColumn;
    int *rowIndices;
    int *columnIndices;

public:
    imageData()
        : x(0), y(0), nchannels(0)
        , pixelData(0), cdfRow(0), cdfColumn(0)
        , rowIndices(0), columnIndices(0) {
    }

    ~imageData(){
        invalidate();
    }

    bool isValid() const{
        return (x * y * nchannels > 0 && nchannels >= 3);
    }

    void invalidate(){
        if (pixelData){
            AiAddMemUsage(-x * y * nchannels * sizeof(float), "zoic");
            AiFree(pixelData);
            pixelData = 0;
        }
        if (cdfRow){
            AiAddMemUsage(-y * sizeof(float), "zoic");
            AiFree(cdfRow);
            cdfRow = 0;
        }
        if (cdfColumn){
            AiAddMemUsage(-x * y * sizeof(float), "zoic");
            AiFree(cdfColumn);
            cdfColumn = 0;
        }
        if (rowIndices){
            AiAddMemUsage(-y * sizeof(int), "zoic");
            AiFree(rowIndices);
            rowIndices = 0;
        }
        if (columnIndices){
            AiAddMemUsage(-x * y * sizeof(int), "zoic");
            AiFree(columnIndices);
            columnIndices = 0;
        }
        x = y = nchannels = 0;
    }

    bool read(const char *bokeh_kernel_filename){

        invalidate();

        AtInt64 nbytes = 0;

#ifdef NO_OIIO

        AiMsgInfo("[ZOIC] Reading image using Arnold API: %s", bokeh_kernel_filename);

        AtString path(bokeh_kernel_filename);

        unsigned int iw, ih, nc;
        if (!AiTextureGetResolution(path, &iw, &ih) ||
            !AiTextureGetNumChannels(path, &nc)){
            return false;
        }

        x = int(iw);
        y = int(ih);
        nchannels = int(nc);

        nbytes = x * y * nchannels * sizeof(float);
        AiAddMemUsage(nbytes, "zoic");
        pixelData = (float*) AiMalloc(nbytes);

        if (!LoadTexture(path, pixelData)){
            invalidate();
            return false;
        }

#else

        AiMsgInfo("[ZOIC] Reading image using OpenImageIO: %s", bokeh_kernel_filename);

        //Search for an ImageIO plugin that is capable of reading the file ("foo.jpg"), first by
        //trying to deduce the correct plugin from the file extension, but if that fails, by opening
        //every ImageIO plugin it can find until one will open the file without error. When it finds
        //the right plugin, it creates a subclass instance of ImageInput that reads the right kind of
        //file format, and tries to fully open the file.
        OpenImageIO::ImageInput *in = OpenImageIO::ImageInput::open (bokeh_kernel_filename);
        if (! in){
            return false;
        }

        const OpenImageIO::ImageSpec &spec = in->spec();

        x = spec.width;
        y = spec.height;
        nchannels = spec.nchannels;

        nbytes = x * y * nchannels * sizeof(float);
        AiAddMemUsage(nbytes, "zoic");
        pixelData = (float*) AiMalloc(nbytes);

        in->read_image(OpenImageIO::TypeDesc::FLOAT, pixelData);
        in->close();
        delete in;

#endif

        AiMsgInfo("[ZOIC] Image Width: %d", x);
        AiMsgInfo("[ZOIC] Image Height: %d", y);
        AiMsgInfo("[ZOIC] Image Channels: %d", nchannels);
        AiMsgInfo("[ZOIC] Total amount of pixels to process: %d", x * y);

        DEBUG_ONLY({
            // print out raw pixel data
            int npixels = x * y;
            for (int i = 0, j = 0; i < npixels; i++){
                std::cout << "[";
                for (int k = 0; k < nchannels; k++, j++){
                    std::cout << pixelData[j];
                    if (k + 1 < nchannels){
                        std::cout << ", ";
                    }
                }
                std::cout << "]";
                if (i + 1 < npixels){
                    std::cout << ", ";
                }
            }
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })

        bokehProbability();

        return true;
    }

    // Importance sampling
    void bokehProbability(){
        if (!isValid()){
            return;
        }

        // initialize arrays
        AtInt64 nbytes = x * y * sizeof(float);
        AtInt64 totalTempBytes = 0;

        AiAddMemUsage(nbytes, "zoic");
        float *pixelValues = (float*) AiMalloc(nbytes);
        totalTempBytes += nbytes;

        AiAddMemUsage(nbytes, "zoic");
        float *normalizedPixelValues = (float*) AiMalloc(nbytes);
        totalTempBytes += nbytes;

        int npixels = x * y;
        int o1 = (nchannels >= 2 ? 1 : 0);
        int o2 = (nchannels >= 3 ? 2 : o1);
        float totalValue = 0.0f;

        // for every pixel, stuff going wrong here
        for (int i=0, j=0; i < npixels; ++i, j+=nchannels){
            // store pixel value in array
            // calculate luminance [Y = 0.3 R + 0.59 G + 0.11 B]
            pixelValues[i] = pixelData[j] * 0.3f + pixelData[j+o1] * 0.59f + pixelData[j+o2] * 0.11f;

            totalValue += pixelValues[i];

            DEBUG_ONLY(std::cout << "Pixel Luminance: " << i << " -> " << pixelValues[i] << std::endl);
        }

        DEBUG_ONLY({
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "DEBUG: Total Pixel Value: " << totalValue << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })

        // normalize pixel values so sum = 1
        float invTotalValue = 1.0f / totalValue;
        float totalNormalizedValue = 0.0f;

        for(int i=0; i < npixels; ++i){
            normalizedPixelValues[i] = pixelValues[i] * invTotalValue;

            totalNormalizedValue += normalizedPixelValues[i];

            DEBUG_ONLY(std::cout << "Normalized Pixel Value: " << i << ": " << normalizedPixelValues[i] << std::endl);
        }

        DEBUG_ONLY({
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "DEBUG: Total Normalized Pixel Value: " << totalNormalizedValue << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })

        // calculate sum for each row
        nbytes = y * sizeof(float);
        AiAddMemUsage(nbytes, "zoic");
        float *summedRowValues = (float*) AiMalloc(nbytes);
        totalTempBytes += nbytes;

        for(int i=0, k=0; i < y; ++i){

            summedRowValues[i] = 0.0f;

            for(int j=0; j < x; ++j, ++k){

                summedRowValues[i] += normalizedPixelValues[k];
            }

           DEBUG_ONLY(std::cout << "Summed Values row [" << i << "]: " << summedRowValues[i] << std::endl);
        }


        DEBUG_ONLY({
            // calculate sum of all row values, just to debug
            float totalNormalizedRowValue = 0.0f;
            for(int i=0; i < y; ++i){
                totalNormalizedRowValue += summedRowValues[i];
            }
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "Debug: Summed Row Value: " << totalNormalizedRowValue << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })


        // make array of indices
        nbytes = y * sizeof(int);
        AiAddMemUsage(nbytes, "zoic");
        rowIndices = (int*) AiMalloc(nbytes);

        for(int i = 0; i < y; ++i){
            rowIndices[i] = i;
        }

        std::sort(rowIndices, rowIndices + y, arrayCompare(summedRowValues));


        DEBUG_ONLY({
            // print values
            for(int i = 0; i < y; ++i){
                std::cout << "PDF row [" <<  rowIndices[i] << "]: " << summedRowValues[rowIndices[i]] << std::endl;
            }
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })


        // For every row, add the sum of all previous row (cumulative distribution function)
        nbytes = y * sizeof(float);
        AiAddMemUsage(nbytes, "zoic");
        cdfRow = (float*) AiMalloc(nbytes);

        float prevVal = 0.0f;

        for (int i = 0; i < y; ++i){
            cdfRow[i] = prevVal + summedRowValues[rowIndices[i]];
            prevVal = cdfRow[i];

            DEBUG_ONLY(std::cout << "CDF row [" << rowIndices[i] << "]: " << cdfRow[i] << std::endl);
        }

        DEBUG_ONLY({
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })

        nbytes = npixels * sizeof(float);
        AiAddMemUsage(nbytes, "zoic");
        float *normalizedValuesPerRow = (float*) AiMalloc(nbytes);
        totalTempBytes += nbytes;

        // divide pixel values of each pixel by the sum of the pixel values of that row (Normalize)
        for (int r = 0, i = 0; r < y; ++r){
            for (int c = 0; c < x; ++c, ++i){
                // avoid division by 0
                if ((normalizedPixelValues[i] != 0) && (summedRowValues[r] != 0)){
                    normalizedValuesPerRow[i] = normalizedPixelValues[i] / summedRowValues[r];
                }
                else{
                    normalizedValuesPerRow[i] = 0;
                }

                DEBUG_ONLY(std::cout << "Normalized Pixel value per row: " << i << ": " << normalizedValuesPerRow[i] << std::endl);
            }
        }

        DEBUG_ONLY({
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })

        // sort column values from highest to lowest per row (probability density function)
        nbytes = npixels * sizeof(int);
        AiAddMemUsage(nbytes, "zoic");
        columnIndices = (int*) AiMalloc(nbytes);

        for(int i = 0; i < npixels; i++){
            columnIndices[i] = i;
        }

        for (int i = 0; i < npixels; i+=x){
            std::sort(columnIndices + i, columnIndices + i + x, arrayCompare(normalizedValuesPerRow));
        }

        DEBUG_ONLY({
            // print values
            for(int i = 0; i < npixels; ++i){
                std::cout << "PDF column [" << columnIndices[i] << "]: " << normalizedValuesPerRow[columnIndices[i]] << std::endl;
            }
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })

        // For every column per row, add the sum of all previous columns (cumulative distribution function)
        nbytes = npixels * sizeof(float);
        AiAddMemUsage(nbytes, "zoic");
        cdfColumn = (float*) AiMalloc(nbytes);

        for (int r = 0, i = 0; r < y; ++r){
            prevVal = 0.0f;

            for (int c = 0; c < x; ++c, ++i){
                cdfColumn[i] = prevVal + normalizedValuesPerRow[columnIndices[i]];
                prevVal = cdfColumn[i];

                DEBUG_ONLY(std::cout << "CDF column [" <<  columnIndices[i] << "]: " << cdfColumn[i] << std::endl);
            }
        }

        DEBUG_ONLY(std::cout << "----------------------------------------------" << std::endl);

        // Release and untrack memory
        AiAddMemUsage(-totalTempBytes, "zoic");

        AiFree(pixelValues);
        AiFree(normalizedPixelValues);
        AiFree(summedRowValues);
        AiFree(normalizedValuesPerRow);
    }

    // Sample image
   void bokehSample(float randomNumberRow, float randomNumberColumn, float *dx, float *dy){

        if (!isValid()){
            AiMsgWarning("[ZOIC] Invalid bokeh image data.");
            *dx = 0.0f;
            *dy = 0.0f;
           return;
        }

        // print random number between 0 and 1
        DEBUG_ONLY(std::cout << "RANDOM NUMBER ROW: " << randomNumberRow << std::endl);

        // find upper bound of random number in the array
        float *pUpperBound = std::upper_bound(cdfRow, cdfRow + y, randomNumberRow);
        int r = 0;

        if (pUpperBound >= cdfRow + y){
            //AiMsgWarning("[zoic] %f larger than last biggest cdfRow[%d] = %f", randomNumberRow, y-1, cdfRow[y-1]);
            r = y - 1;

        } else{
            DEBUG_ONLY(std::cout << "UPPER BOUND: " << *pUpperBound << std::endl);
            r = int(pUpperBound - cdfRow);
        }


        // find actual pixel row
        int actualPixelRow = rowIndices[r];

        // recalculate pixel row so that the center pixel is (0,0) - might run into problems with images of dimensions like 2x2, 4x4, 6x6, etc
        int recalulatedPixelRow = actualPixelRow - ((x - 1) / 2);

       DEBUG_ONLY({
            // print values
            std::cout << "INDEX IN CDF ROW: " << r << std::endl;
            std::cout << "ACTUAL PIXEL ROW: " << actualPixelRow << std::endl;
            std::cout << "RECALCULATED PIXEL ROW: " << recalulatedPixelRow << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            // print random number between 0 and 1
            std::cout << "RANDOM NUMBER COLUMN: " << randomNumberColumn << std::endl;
        })

        int startPixel = actualPixelRow * x;

        DEBUG_ONLY(std::cout << "START PIXEL: " << startPixel << std::endl);


        // find upper bound of random number in the array
        float *pUpperBoundColumn = std::upper_bound(cdfColumn + startPixel, cdfColumn + startPixel + x, randomNumberColumn);
        int c = 0;

        if (pUpperBoundColumn >= cdfColumn + startPixel + x){
            //AiMsgWarning("[zoic] %f larger than last biggest cdfColumn[%d][%d] = %f", randomNumberColumn, r, x-1, cdfColumn[startPixel+x-1]);
            c = startPixel + x - 1;

        } else{
            DEBUG_ONLY(std::cout << "UPPER BOUND: " << *pUpperBoundColumn << std::endl);
            c = int(pUpperBoundColumn - cdfColumn);
        }

        // find actual pixel column
        int actualPixelColumn = columnIndices[c];
        int relativePixelColumn = actualPixelColumn - startPixel;
        int recalulatedPixelColumn = relativePixelColumn - ((y - 1) / 2);

        DEBUG_ONLY({
            // print values
            std::cout << "INDEX IN CDF COLUMN: " << c << std::endl;
            std::cout << "ACTUAL PIXEL COLUMN: " << actualPixelColumn << std::endl;
            std::cout << "RELATIVE PIXEL COLUMN (starting from 0): " << relativePixelColumn << std::endl;
            std::cout << "RECALCULATED PIXEL COLUMN: " << recalulatedPixelColumn << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        })

        // to get the right image orientation, flip the x and y coordinates and then multiply the y values by -1 to flip the pixels vertically
        float flippedRow = float(recalulatedPixelColumn);
        float flippedColumn = recalulatedPixelRow * -1.0f;

        // send values back
        *dx = (float)flippedRow / (float)x * 2.0;
        *dy = (float)flippedColumn / (float)y * 2.0;
    }
};

struct cameraData{
    float fov;
    float tan_fov;
    float apertureRadius;
    imageData image;

    cameraData()
        : fov(0.0f), tan_fov(0.0f), apertureRadius(0.0f){
    }

    ~cameraData(){
    }
};

struct Lensdata{
    std::vector<double> lensRadiusCurvature;
    std::vector<double> lensThickness;
    std::vector<double> lensIOR;
    std::vector<double> lensAperture;
    double userApertureRadius;
    int apertureElement;
    int vignettedRays, succesRays;
    int totalInternalReflection;
    double apertureDistance;
    float xres, yres;
    double optimalAperture;
    double focalLengthRatio;
    float filmDiagonal;
    double lensShift;
} ld;


struct DrawData{
    std::vector<double> coordinates;
    std::vector<double> points;
} dd;


class vec3 {
    public:
        double x;
        double y;
        double z;
};


double vec3dot(vec3 vector1, vec3 vector2){
    return (vector1.x * vector2.x + vector1.y * vector2.y + vector1.z * vector2.z);
}


vec3 vec3subtr(vec3 vector1, vec3 vector2){
    vec3 subtraction;
    subtraction.x = vector1.x - vector2.x;
    subtraction.y = vector1.y - vector2.y;
    subtraction.z = vector1.z - vector2.z;
    return subtraction;
}


vec3 vec3add(vec3 vector1, vec3 vector2){
    vec3 add;
    add.x = vector1.x + vector2.x;
    add.y = vector1.y + vector2.y;
    add.z = vector1.z + vector2.z;
    return add;
}


void vec3normalize(vec3 *vector1){
    double length = sqrt((vector1->x * vector1->x) + (vector1->y * vector1->y) + (vector1->z * vector1->z));
    vector1->x /= length;
    vector1->y /= length;
    vector1->z /= length;
}


// PBRT v2 source code  - Concentric disk sampling (Sampling the disk in a more uniform way than with random sampling)
inline void ConcentricSampleDisk(float u1, float u2, float *dx, float *dy) {
    float radius; // radius
    float theta; // angle

    // Map uniform random numbers to $[-1,1]^2$
    float sx = 2.0f * u1 - 1.0f;
    float sy = 2.0f * u2 - 1.0f;

    // Map square to $(r,\theta)$
    // Handle degeneracy at the origin
    if (sx == 0.0f && sy == 0.0f){
       *dx=0.0f;
        *dy = 0.0f;
   }
    if (sx >= -sy) {
        if (sx > sy) {
            // Handle first region of disk
            radius = sx;
            if (sy > 0.0f) theta = sy/radius;
            else          theta = 8.0f + sy/radius;
        }
        else {
            // Handle second region of disk
            radius = sy;
            theta = 2.0f - sx/radius;
        }
    }
    else {
        if (sx <= sy) {
            // Handle third region of disk
            radius = -sx;
            theta = 4.0f - sy/radius;
        }
        else {
            // Handle fourth region of disk
            radius = -sy;
            theta = 6.0f + sx/radius;
        }
    }

    theta *= AI_PI / 4.0f;
    *dx = radius * std::cos(theta);
    *dy = radius * std::sin(theta);
}


// READ IN TABULAR LENS DATA
void readTabularLensData(std::string lensDataFileName, Lensdata *ld){

    // reset vectors
    ld->lensAperture.clear();
    ld->lensIOR.clear();
    ld->lensRadiusCurvature.clear();
    ld->lensAperture.clear();

    std::ifstream lensDataFile(lensDataFileName);
    std::string line;
    std::string token;
    std::stringstream iss;
    int lensDataCounter = 1;


    AiMsgInfo("[ZOIC] ##############################################");
    AiMsgInfo("[ZOIC] ############# READING LENS DATA ##############");
    AiMsgInfo("[ZOIC] ##############################################");
    AiMsgInfo("[ZOIC] If you're reading this, welcome to the nerd club :-D");

    while (getline(lensDataFile, line))
    {
        if (line.length() == 0 || line[0] == '#'){
            AiMsgInfo("[ZOIC] Comment or empty line, skipping line");
        }
        else {
            iss << line;

            // put values (converting from string to float) into the vectors
            while (getline(iss, token, '\t') ){
                if (token == " "){
                   AiMsgError("[ZOIC] Please make sure your .dat file only contains TAB spacings.");
                }

                if (lensDataCounter == 1){
                    ld->lensRadiusCurvature.push_back(std::stod(token));
                }

                if (lensDataCounter == 2){
                    ld->lensThickness.push_back(std::stod(token));
                }

                if (lensDataCounter == 3){
                    ld->lensIOR.push_back(std::stod(token));
                }

                if (lensDataCounter == 4){
                    ld->lensAperture.push_back(std::stod(token));
                    lensDataCounter = 0;
                }
                lensDataCounter += 1;
            }

            iss.clear();
        }
    }

    AiMsgInfo("[ZOIC] ##############################################");
    AiMsgInfo("[ZOIC] # ROC \t Thickness \t IOR \t Aperture #");
    AiMsgInfo("[ZOIC] ##############################################");

    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        AiMsgInfo("[ZOIC] %f    %f    %f    %f", ld->lensRadiusCurvature[i], ld->lensThickness[i], ld->lensIOR[i], ld->lensAperture[i]);
    }

    AiMsgInfo("[ZOIC] ##############################################");
    AiMsgInfo("[ZOIC] ########### END READING LENS DATA ############");
    AiMsgInfo("[ZOIC] ##############################################");

    // reverse the datasets in the vector, since we will start with the rear-most lens element
    std::reverse(ld->lensRadiusCurvature.begin(),ld->lensRadiusCurvature.end());
    std::reverse(ld->lensThickness.begin(),ld->lensThickness.end());
    std::reverse(ld->lensIOR.begin(),ld->lensIOR.end());
    std::reverse(ld->lensAperture.begin(),ld->lensAperture.end());

}


void cleanupLensData(Lensdata *ld){
    for (int i = 0; i < ld->lensRadiusCurvature.size(); i++){
       // check if there is a 0.0 lensRadiusCurvature, which is the aperture.
      if (ld->lensRadiusCurvature[i] == 0.0){
           ld->apertureElement = i;
           AiMsgInfo("[ZOIC] Aperture is lens element number = [%d]", ld->apertureElement);

           AiMsgInfo("[ZOIC] Adjusted lensRadiusCurvature[%d] [%f] to [99999.0]", i, ld->lensRadiusCurvature[i]);
           ld->lensRadiusCurvature[i] = 99999.0;
       }

       if (ld->lensIOR[i] == 0.0){
           AiMsgInfo("[ZOIC] Changed lensIOR[%d] [%f] to [1.0]", i, ld->lensIOR[i]);
           ld->lensIOR[i] = 1.0;
       }

    }
}


// RAY SPHERE INTERSECTIONS
AtVector raySphereIntersection(AtVector ray_direction, AtVector ray_origin, AtVector sphere_center, double sphere_radius, bool reverse, bool tracingRealRays){

    AtVector norm_ray_direction = AiV3Normalize(ray_direction);
    AtVector L = sphere_center - ray_origin;

    // project the directionvector onto the distancevector
    double tca = AiV3Dot(L, norm_ray_direction);

    double radius2 = sphere_radius * sphere_radius;
    double d2 = AiV3Dot(L, L) - tca * tca;

    // if the distance from the ray to the spherecenter is larger than its radius, don't worry about it
    // this is just some arbitrary value that I will use to check for errors
    if (tracingRealRays == true && d2 > radius2){return {0.0, 0.0, 0.0};}

    double thc = sqrt(radius2 - d2);

    if(!reverse){
        return ray_origin + norm_ray_direction * (tca + thc * std::copysign(1.0, sphere_radius));
    }
    else{
        return ray_origin + norm_ray_direction * (tca - thc * std::copysign(1.0, sphere_radius));
    }
}


// COMPUTE NORMAL HITPOINT
AtVector intersectionNormal(AtVector hit_point, AtVector sphere_center, double sphere_radius){
    return AiV3Normalize(sphere_center - hit_point) * std::copysign(1.0, sphere_radius);
}


// TRANSMISSION VECTOR
AtVector calculateTransmissionVector(double ior1, double ior2, AtVector incidentVector, AtVector normalVector, bool tracingRealRays){

    // VECTORS NEED TO BE NORMALIZED BEFORE USE!
    incidentVector = AiV3Normalize(incidentVector);
    normalVector = AiV3Normalize(normalVector);

    double eta = ior1 / ior2;
    double c1 = - AiV3Dot(incidentVector, normalVector);
    double cs2 = eta * eta * (1.0 - c1 * c1);

    // total internal reflection, can only occur when ior1 > ior2
    if( tracingRealRays && ior1 > ior2 && cs2 > 1.0){
        ld.totalInternalReflection += 1;
        // this is just some arbitrary value that I will use to check for errors
        return {0.0, 0.0, 0.0};
    }

    return (incidentVector * eta) + (normalVector * ((eta * c1) - sqrt(std::abs(1.0 - cs2))));
}



// LINE LINE INTERSECTIONS
// watch out here, x is the z coordinate here and z is x.. (still need to convert)
AtVector2 lineLineIntersection(AtVector line1_origin, AtVector line1_direction, AtVector line2_origin, AtVector line2_direction){
    // Get A,B,C of first line - points : ps1 to pe1
    double A1 = line1_direction.y - line1_origin.y;
    double B1 = line1_origin.x - line1_direction.x;
    double C1 = A1 * line1_origin.x + B1 * line1_origin.y;

    // Get A,B,C of second line - points : ps2 to pe2
    double A2 = line2_direction.y - line2_origin.y;
    double B2 = line2_origin.x - line2_direction.x;
    double C2 = A2 * line2_origin.x + B2 * line2_origin.y;

    // Get delta and check if the lines are parallel
    double delta = A1 * B2 - A2 * B1;

    // now return the Vector2 intersection point
    AtVector2 intersectionPoint;
    intersectionPoint.x = (B2 * C1 - B1 * C2) / delta;
    intersectionPoint.y = (A1 * C2 - A2 * C1) / delta;

    return intersectionPoint;
}


// CALCULATE IMAGE DISTANCE
// watch out here, x is the z coordinate here and z is x.. (still need to convert)
double calculateImageDistance(double objectDistance, Lensdata *ld, DrawData *dd, bool draw){

    // watch out here, x is the z coordinate here and z is x.. (still need to convert)
    double imageDistance;
    AtVector ray_origin_focus;
    ray_origin_focus.x = objectDistance;
    ray_origin_focus.y = 0.0;
    ray_origin_focus.z = 0.0;


    //dd->coordinates.push_back(objectDistance);
    //dd->coordinates.push_back(9999.0);
    //dd->coordinates.push_back(objectDistance);
    //dd->coordinates.push_back(-9999.0);

    png.line((objectDistance * scale) + translateX, (9999.0 * scale) + translateY, (objectDistance * scale) + translateX, (-9999.0 * scale) + translateY, 0.64, 1.0, 0.0);




    // 20.0 needs to be changed to a number as small as possible whilst still getting no numerical errors. (eg 0.001)
    AtVector ray_direction_focus;
    ray_direction_focus.x = - objectDistance;
    ray_direction_focus.y = 26.0;
    ray_direction_focus.z = 0.0;

    double summedThickness_focus = 0.0;

    // go through every lens element
    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        if(i==0){
            for(int k = 0; k < ld->lensRadiusCurvature.size(); k++){
                summedThickness_focus += ld->lensThickness[k];
            }
        }

        // (condition) ? true : false;
        i == 0 ? summedThickness_focus = summedThickness_focus : summedThickness_focus -= ld->lensThickness[ld->lensRadiusCurvature.size() - i];

        if(ld->lensRadiusCurvature[i] == 0.0){
            ld->lensRadiusCurvature[i] = 99999.0;
        }

        AtVector sphere_center;
        sphere_center.x = summedThickness_focus - ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i];
        sphere_center.y = 0.0;
        sphere_center.z = 0.0;

        AtVector hit_point = raySphereIntersection(ray_direction_focus, ray_origin_focus, sphere_center, ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i], true, false);

        AtVector hit_point_normal = intersectionNormal(hit_point, sphere_center, - ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i]);

        if(draw){
            png.line((hit_point.x * scale) + translateX, (hit_point.y * scale) + translateY, (ray_origin_focus.x * scale) + translateX, (ray_origin_focus.y * scale) + translateY, 0.64, 1.0, 0.0);
        }


        if(i==0){
            ray_direction_focus = calculateTransmissionVector(1.0, ld->lensIOR[ld->lensRadiusCurvature.size() - 1 - i], ray_direction_focus, hit_point_normal, false);
        } else {
            ray_direction_focus = calculateTransmissionVector(ld->lensIOR[ld->lensRadiusCurvature.size() - i], ld->lensIOR[ld->lensRadiusCurvature.size() - i - 1], ray_direction_focus, hit_point_normal, false);
        }

        // set hitpoint to be the new origin
        ray_origin_focus = hit_point;

        // shoot off rays after last refraction
        if(i == ld->lensRadiusCurvature.size() - 1){
            ray_direction_focus = calculateTransmissionVector(ld->lensIOR[ld->lensRadiusCurvature.size() - 1 - i], 1.0, ray_direction_focus, hit_point_normal, false);

            // find intersection point
            AtVector axialStart = {-99999.0, 0.0, 0.0};
            AtVector axialEnd = {99999.0, 0.0, 0.0};
            AtVector lineDirection = {ray_origin_focus.x + ray_direction_focus.x, ray_origin_focus.y + ray_direction_focus.y, 0.0};
            imageDistance = lineLineIntersection(axialStart, axialEnd, ray_origin_focus, lineDirection).x;
            if(draw){
                png.line((hit_point.x * scale) + translateX, (hit_point.y * scale) + translateY, (ray_direction_focus.x * 10000.0 * scale) + translateX, (ray_direction_focus.y * 10000.0 * scale) + translateY, 0.64, 1.0, 0.0);
            }
        }

    }

    AiMsgInfo("[ZOIC] Object distance = [%f]", objectDistance);
    AiMsgInfo("[ZOIC] Image distance = [%f]", imageDistance);


    dd->coordinates.push_back(imageDistance);
    dd->coordinates.push_back(-9999.0);
    dd->coordinates.push_back(imageDistance);
    dd->coordinates.push_back(9999.0);


    return imageDistance;
}



void traceThroughLensElements(AtVector *ray_origin, AtVector *ray_direction, float *weight, Lensdata *ld, DrawData *dd, bool draw){

    AtVector hit_point;
    AtVector hit_point_normal;
    AtVector sphere_center;
    double summedThickness;
    AtVector tmpRayDirection;

    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        if(i == 0){
            summedThickness = ld->lensThickness[0];
        } else {
            summedThickness += ld->lensThickness[i];
        }

        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = summedThickness - ld->lensRadiusCurvature[i];

        hit_point = raySphereIntersection(*ray_direction, *ray_origin, sphere_center, ld->lensRadiusCurvature[i], false, true);

        // ray won´t hit the lens
        if (hit_point.x == 0.0 && hit_point.y == 0.0 && hit_point.z == 0.0){
            *weight = 0.0f;
            break;
        }

        double hitPointHypotenuse = sqrt(hit_point.x * hit_point.x + hit_point.y * hit_point.y);
        if(hitPointHypotenuse > (ld->lensAperture[i]/2.0)){
            ld->vignettedRays += 1;
            *weight = 0.0f;
           break;
        }

        if(i == ld->apertureElement && hitPointHypotenuse > ld->userApertureRadius){
            ld->vignettedRays += 1;
            *weight = 0.0f;
            break;
        }

        hit_point_normal = intersectionNormal(hit_point, sphere_center, ld->lensRadiusCurvature[i]);


        if (draw){
            dd->points.push_back(hit_point.z);
            dd->points.push_back(hit_point.y);

            dd->coordinates.push_back(hit_point.z);
            dd->coordinates.push_back(hit_point.y);
            dd->coordinates.push_back(ray_origin->z);
            dd->coordinates.push_back(ray_origin->y);
        }


        // set hitpoint to be the new origin
        *ray_origin = hit_point;


       // if not last lens element
       if(i != ld->lensRadiusCurvature.size() - 1){
            tmpRayDirection = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], *ray_direction, hit_point_normal, true);
        } else { // last lens element
            // assuming the material outside the lens is air
            tmpRayDirection = calculateTransmissionVector(ld->lensIOR[i], 1.0, *ray_direction, hit_point_normal, true);


            if (draw){
                dd->coordinates.push_back(hit_point.z);
                dd->coordinates.push_back(hit_point.y);
                dd->coordinates.push_back(tmpRayDirection.z * 10000.0);
                dd->coordinates.push_back(tmpRayDirection.y * 10000.0);
            }

       }


        // check for total internal reflection case
        if (tmpRayDirection.x == 0.0 && tmpRayDirection.y == 0.0 && tmpRayDirection.z == 0.0){
            *weight = 0.0f;
            break;
        }

        *ray_direction = tmpRayDirection;

    }

    // count succesful rays
    ld->succesRays += 1;
    // test
    //AiMsgInfo("[ZOIC] RAY: [%d]: hitpoint && raydirection: %f %f %f, %f %f %f", counter, hit_point.x, hit_point.y, hit_point.z, tmpRayDirection.x, tmpRayDirection.y, tmpRayDirection.z);
}




void drawLenses(Lensdata *ld, DrawData *dd){

    AtVector hit_point;
    AtVector sphere_center;
    float summedThickness;
    AtVector rayOrigin = {0.0, 0.0, 0.0};
    AtVector rayDirection;

    std::vector<float> lensFrontSurfaceX;
    std::vector<float> lensFrontSurfaceY;
    std::vector<float> lensBackSurfaceX;
    std::vector<float> lensBackSurfaceY;


    int samples = 1000;

    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){

        float heightVariation = ld->lensAperture[i] / float(samples);

        if(i == 0){
            summedThickness = ld->lensThickness[0];
        } else {
            summedThickness += ld->lensThickness[i];
        }

        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = summedThickness - ld->lensRadiusCurvature[i];

        for(int j = 0; j < samples; j++){
            rayDirection.x = 0.0;
            rayDirection.y = (ld->lensAperture[i] / 2.0) - heightVariation * float(j);
            rayDirection.z = summedThickness;

            hit_point = raySphereIntersection(rayDirection, rayOrigin, sphere_center, ld->lensRadiusCurvature[i], false, false);

            png.filledcircle((hit_point.z * scale) + translateX, (hit_point.y * scale) + translateY, 1.0, 1.0, 1.0, 1.0);

        }
    }
}




// CALCULATE FOCAL LENGTH
// watch out here, x is the z coordinate here and z is x.. (still need to convert)
double traceThroughLensElementsForFocalLength(Lensdata *ld){

    double tracedFocalLength;
    double focalPointDistance;
    double principlePlaneDistance;
    double summedThickness_fp = 0.0;

    //this might not do in all cases, maybe find better way
    float rayOriginHeight = ld->lensAperture[0] * 0.25;

    AtVector ray_origin_fp = {0.0, rayOriginHeight, 0.0};
    AtVector ray_direction_fp = {99999.0, 0.0, 0.0};

    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        i == 0 ? summedThickness_fp = ld->lensThickness[0] : summedThickness_fp += ld->lensThickness[i];

        AtVector sphere_center;
        sphere_center.x = summedThickness_fp - ld->lensRadiusCurvature[i];
        sphere_center.y = 0.0;
        sphere_center.z = 0.0;

        AtVector hit_point = raySphereIntersection(ray_direction_fp, ray_origin_fp, sphere_center, ld->lensRadiusCurvature[i], false, false);

        AtVector hit_point_normal = intersectionNormal(hit_point, sphere_center, ld->lensRadiusCurvature[i]);


        if(i != ld->lensRadiusCurvature.size() - 1){
            ray_direction_fp = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], ray_direction_fp, hit_point_normal, true);
        } else { // last element in array
            ray_direction_fp = calculateTransmissionVector(ld->lensIOR[i], 1.0, ray_direction_fp, hit_point_normal, true);

            AtVector pp_line1start; //original parallel ray start
            pp_line1start.x = 0.0;
            pp_line1start.y = rayOriginHeight;
            pp_line1start.z = 0.0;

            AtVector pp_line1end; //original parallel ray end
            pp_line1end.x = 99999.0;
            pp_line1end.y = rayOriginHeight;
            pp_line1end.z = 0.0;

            AtVector pp_line2end; //direction ray end
            pp_line2end.x = ray_origin_fp.x + (ray_direction_fp.x * 1000.0);
            pp_line2end.y = ray_origin_fp.y + (ray_direction_fp.y * 1000.0);
            pp_line2end.z = 0.0;

            principlePlaneDistance = lineLineIntersection(pp_line1start, pp_line1end, ray_origin_fp, pp_line2end).x;
            AiMsgInfo("[ZOIC] Principle Plane distance = [%f]", principlePlaneDistance);

            // find intersection point
            AtVector axialStart = {0.0, 0.0, 0.0};
            AtVector axialEnd = {99999.0, 0.0, 0.0};
            AtVector lineDirection;
            lineDirection.x = ray_origin_fp.x + ray_direction_fp.x * 1000.0;
            lineDirection.y = ray_origin_fp.y + ray_direction_fp.y * 1000.0;
            lineDirection.z = 0.0;

            focalPointDistance = lineLineIntersection(axialStart, axialEnd, ray_origin_fp, lineDirection).x;
            AiMsgInfo("[ZOIC] Focal point distance = [%f]", focalPointDistance);
        }

        // set hitpoint to be the new origin
        ray_origin_fp = hit_point;
    }

    tracedFocalLength = focalPointDistance - principlePlaneDistance;
    AiMsgInfo("[ZOIC] Raytraced Focal Length = [%f]", tracedFocalLength);

    return tracedFocalLength;
}



bool traceThroughLensElementsForApertureSize(AtVector ray_origin, AtVector ray_direction, Lensdata *ld){

    AtVector hit_point;
    AtVector hit_point_normal;
    AtVector sphere_center;
    double summedThickness;

    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        i == 0 ? summedThickness = ld->lensThickness[0] : summedThickness += ld->lensThickness[i];

        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = summedThickness - ld->lensRadiusCurvature[i];

        hit_point = raySphereIntersection(ray_direction, ray_origin, sphere_center, ld->lensRadiusCurvature[i], false, true);

        // set hitpoint to be the new origin
        ray_origin = hit_point;

        double hitPointHypotenuse = sqrt(hit_point.x * hit_point.x + hit_point.y * hit_point.y);
        if(i == ld->apertureElement && hitPointHypotenuse > ld->userApertureRadius){
            return 0;
        }

        hit_point_normal = intersectionNormal(hit_point, sphere_center, ld->lensRadiusCurvature[i]);

        ray_direction = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], ray_direction, hit_point_normal, true);

    }
    return 1;
}



void adjustFocalLength(Lensdata *ld){
    for(unsigned int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        ld->lensRadiusCurvature[i] *= ld->focalLengthRatio;
        ld->lensThickness[i] *= ld->focalLengthRatio;
        ld->lensAperture[i] *= ld->focalLengthRatio;
    }
}



node_parameters {
    AiParameterFLT("sensorWidth", 3.6f); // 35mm film
    AiParameterFLT("sensorHeight", 2.4f); // 35 mm film
    AiParameterFLT("focalLength", 100.0f); // distance between sensor and lens
    AiParameterFLT("fStop", 1.8f);
    AiParameterFLT("focalDistance", 120.0f); // distance from lens to focal point
    AiParameterBOOL("useImage", false);
    AiParameterStr("bokehPath", ""); //bokeh shape image location
    AiParameterBOOL("kolb", true);
    AiParameterStr("lensDataPath", ""); // lens data file location
    AiParameterBOOL("kolbSamplingMethod", false);
    AiParameterBOOL("useDof", true);
    AiParameterFLT("opticalVignettingDistance", 0.0f); //distance of the opticalVignetting virtual aperture
    AiParameterFLT("opticalVignettingRadius", 0.0f); // 1.0 - .. range float, to multiply with the actual aperture radius
    AiParameterFLT("highlightWidth", 0.2f);
    AiParameterFLT("highlightStrength", 10.0f);
    AiParameterFLT("exposureControl", 0.0f);
}


node_initialize {
    cameraData *camera = new cameraData();
    AiCameraInitialize(node, (void*)camera);

    // draw axials
    png.line((-9999 * scale) + translateX, (0.0 * scale) + translateY, (9999.0 * scale) + translateX, (0.0 * scale) + translateY, 1.0, 0.64, 0.64);
    png.line(translateX, (9999.0 * scale) + translateY, translateX, (-9999.0 * scale) + translateY, 1.0, 0.64, 0.64);




}

node_update {
    AiCameraUpdate(node, false);
    cameraData *camera = (cameraData*) AiCameraGetLocalData(node);

    if(!_kolb){
        // calculate field of view (theta = 2arctan*(sensorSize/focalLength))
        camera->fov = 2.0f * atan((_sensorWidth / (2.0f * (_focalLength / 10.0f)))); // in radians
        camera->tan_fov = tanf(camera->fov/ 2.0f);

        // calculate aperture radius (apertureRadius = focalLength / 2*fStop)
        camera->apertureRadius = (_focalLength / 10.0f) / (2.0f * _fStop);
    }

    camera->image.invalidate();

    // make probability functions of the bokeh image
    if (_useImage == true){
       if (!camera->image.read(_bokehPath)){
            AiMsgError("[ZOIC] Couldn't open bokeh image!");
            AiRenderAbort();
       }
    }


    if (_kolb){
        // Update shaderData variables
        AtNode* options = AiUniverseGetOptions();
        ld.xres = static_cast<float>(AiNodeGetInt(options,"xres"));
        ld.yres = static_cast<float>(AiNodeGetInt(options,"yres"));
        AiMsgInfo("[ZOIC] Image resolution = [%f, %f]", ld.xres, ld.yres);

        ld.lensRadiusCurvature.empty();
        ld.lensThickness.empty();
        ld.lensIOR.empty();
        ld.lensAperture.empty();


        // not sure if this is the right way to do it.. probably more to it than this!
        // these values seem to produce the same image as the other camera which is correct.. hey ho
        //ld.filmDiagonal = sqrt(_sensorWidth *_sensorWidth + _sensorHeight * _sensorHeight);
        ld.filmDiagonal = 24.0; //should be 44

       // check if file is supplied
       // string is const char* so have to do it the oldskool way
       if ((_lensDataPath != NULL) && (_lensDataPath[0] == '\0')){
           AiMsgError("[ZOIC] Lens Data Path is empty");
           exit (EXIT_FAILURE);
       } else {
           AiMsgInfo("[ZOIC] Lens Data Path = [%s]", _lensDataPath);
           readTabularLensData(_lensDataPath, &ld);
       }


        // bail out if something is incorrect with the vectors
        if (ld.lensRadiusCurvature.size() == 0 ||
            ld.lensRadiusCurvature.size() != ld.lensAperture.size() ||
            ld.lensThickness.size() != ld.lensIOR.size()){
            AiMsgError("[ZOIC] Failed to read lens data file.");
            AiMsgError("[ZOIC] ... Is it the path correct?");
            AiMsgError("[ZOIC] ... Does it have 4 tabbed columns?");
            exit (EXIT_FAILURE);
        }

        // look for invalid numbers that would mess it all up bro
        cleanupLensData(&ld);

        // calculate focal length by tracing a parallel ray through the lens system
        float kolbFocalLength = traceThroughLensElementsForFocalLength(&ld);

        ld.userApertureRadius = kolbFocalLength / (2.0 * _fStop);
        AiMsgInfo("[ZOIC] userApertureRadius = [%f]", ld.userApertureRadius);

        // clamp aperture if fstop is wider than max aperture given by lens description
        if (ld.userApertureRadius > ld.lensAperture[ld.apertureElement]){
            AiMsgWarning("[ZOIC] Given FSTOP wider than maximum aperture radius provided by lens data.");
            AiMsgWarning("[ZOIC] Clamping aperture radius from [%f] to [%f]", ld.userApertureRadius, ld.lensAperture[ld.apertureElement]);
            ld.userApertureRadius = ld.lensAperture[ld.apertureElement];
        }


        // find by how much all lens elements should be scaled
        ld.focalLengthRatio = _focalLength / kolbFocalLength;

        // scale lens elements
        adjustFocalLength(&ld);

        traceThroughLensElementsForFocalLength(&ld);

        // shift first lens element (and all others consequently) so that
        // the image distance at a certain object distance falls on the film plane
        ld.lensShift = calculateImageDistance(_focalDistance * 10.0, &ld, &dd, false);
        //ld.lensThickness[0] -= ld.lensShift;

        calculateImageDistance(_focalDistance * 10.0, &ld, &dd, true);

        // find how far the aperture is from the film plane
        ld.apertureDistance = 0.0;
        for(int i = 0; i < ld.lensRadiusCurvature.size(); i++){
            ld.apertureDistance += ld.lensThickness[i];
            if(i == ld.apertureElement){
                AiMsgInfo("[ZOIC] Aperture distance after lens shift = [%f]", ld.apertureDistance);
                break;
            }
        }

        // search for ideal max height to shoot rays to on first lens element, by tracing test rays and seeing which one fails
        // maybe this varies based on where on the filmplane we are shooting the ray from? In this case this wouldn´t work..
        // and I don't think it does..
        if(_kolbSamplingMethod == 1){
            int sampleCount = 1024;
            AtVector sampleOrigin = {0.0, 0.0, 0.0};
            for (int i = 0; i < sampleCount; i++){
                float heightVariation = ld.lensAperture[0] / float(sampleCount);
                AtVector sampleDirection = {0.0, heightVariation * float(i), float(ld.lensThickness[0])};

                if (!traceThroughLensElementsForApertureSize(sampleOrigin, sampleDirection, &ld)){
                    AiMsgInfo("[ZOIC] Positive failure at sample [%d] out of [%d]", i, sampleCount);
                    ld.optimalAperture = sampleDirection.y - heightVariation;
                    AiMsgInfo("[ZOIC] Optimal max height to shoot rays to on first lens element = [%f]", ld.optimalAperture);
                    break;
                }
            }
        }
    }
}

node_finish {
    cameraData *camera = (cameraData*) AiCameraGetLocalData(node);

    AiMsgInfo("[ZOIC] Succesful rays = [%d]", ld.succesRays);
    AiMsgInfo("[ZOIC] Vignetted rays = [%d]", ld.vignettedRays);
    AiMsgInfo("[ZOIC] Vignetted Percentage = [%f]", (float(ld.vignettedRays) / float(ld.succesRays) * 100.0));
    AiMsgInfo("[ZOIC] Total internal reflection cases = [%d]", ld.totalInternalReflection);

    // origin line
    for(int i = 0; i < 200; i++){
        png.filledcircle(translateX, ((12.0 - ((24.0/200.0) * (float)i)) * scale) + translateY, 1.0, 1.0, 1.0, 1.0);

    }

    drawLenses(&ld, &dd);


    delete camera;
    AiCameraDestroy(node);


    AiMsgInfo("[ZOIC] Coordinate vector size = [%lu]", dd.coordinates.size());
    AiMsgInfo("[ZOIC] Points vector size = [%lu]", dd.points.size());


    png.close();
}


camera_create_ray {
    // get values
    const AtParamValue* params = AiNodeGetParams(node);
    cameraData *camera = (cameraData*) AiCameraGetLocalData(node);

    if (counter == 100000){
        draw = true;
        for (int i = 0; i < dd.coordinates.size() / 4; i++){
            png.line((dd.coordinates[i * 4] * scale) + translateX,
                    (dd.coordinates[(i * 4) + 1] * scale) + translateY,
                    (dd.coordinates[(i * 4) + 2] * scale) + translateX,
                    (dd.coordinates[(i * 4) + 3] * scale) + translateY, 0.6, 0.6, 0.6);
        }
        for (int i = 0; i < dd.points.size() / 2; i++){
            png.filledcircle((dd.points[i * 2] * scale) + translateX, (dd.points[(i * 2) + 1] * scale) + translateY, 2.0, 1.0, 1.0, 1.0);
        }
        dd.coordinates.clear();
        dd.points.clear();
        counter = 0;
        //AiRenderAbort();
    }


    // chang this to an enum, thinlens, raytraced
    if(!_kolb){

        // create point on lens
        AtPoint p;
        p.x = input->sx * camera->tan_fov;
        p.y = input->sy * camera->tan_fov;
        p.z = 1.0;


        AiMsgInfo("[ZOIC] input = [%f, %f]", input->sx, input->sy);

        // compute direction
        output->dir = AiV3Normalize(p - output->origin);

        // now looking down -Z
        output->dir.z *= -1.0;

        // DOF CALCULATIONS
        if (_useDof == true) {

            // Initialize point on lens
            float lensU = 0.0f;
            float lensV = 0.0f;

            // sample disk with proper sample distribution, lensU & lensV (positions on lens) are updated.
            if (_useImage == false){
                ConcentricSampleDisk(input->lensx, input->lensy, &lensU, &lensV);
            } else { // sample bokeh image
                camera->image.bokehSample(input->lensx, input->lensy, &lensU, &lensV);
            }

            // scale new lens coordinates by the aperture radius
            lensU *= camera->apertureRadius;
            lensV *= camera->apertureRadius;

            // update arnold ray origin
            output->origin = {lensU, lensV, 0.0};

            // Compute point on plane of focus, intersection on z axis
            float intersection = std::abs(_focalDistance / output->dir.z);
            AtPoint focusPoint = output->dir * intersection;

            // update arnold ray direction, normalize
            output->dir = AiV3Normalize(focusPoint - output->origin);

            // Optical Vignetting (CAT EYE EFFECT) - just a hack!
            if (_opticalVignettingDistance > 0.0f){
                // because the first intersection point of the aperture is already known, I can just linearly scale it by the distance to the second aperture
                AtPoint opticalVignetPoint;
                opticalVignetPoint = output->dir * _opticalVignettingDistance;

                // re-center point
                opticalVignetPoint -= output->origin;

                // find hypotenuse of x, y points.
                float pointHypotenuse = sqrt((opticalVignetPoint.x * opticalVignetPoint.x) + (opticalVignetPoint.y * opticalVignetPoint.y));

                // if intersection point on the optical vignetting virtual aperture is within the radius of the aperture from the plane origin, kill ray
                float virtualApertureTrueRadius = camera->apertureRadius * _opticalVignettingRadius;

                // set ray weight to 0, there is an optimisation inside Arnold that doesn't send rays if they will return black anyway.
                if (ABS(pointHypotenuse) > virtualApertureTrueRadius){output->weight = 0.0f;}

                // inner highlight,if point is within domain between lens radius and new inner radius (defined by the width)
                // adding weight to opposite edges to get nice rim on the highlights
                else if (ABS(pointHypotenuse) < virtualApertureTrueRadius && ABS(pointHypotenuse) > (virtualApertureTrueRadius - _highlightWidth)){
                    output->weight *= _highlightStrength * (1.0 - (virtualApertureTrueRadius - ABS(pointHypotenuse))) * sqrt(input->sx * input->sx + input->sy * input->sy);
                }
            }
        }
    }


    if(_kolb){

        // determine origin of rays
        output->origin.x = 0.0;//input->sx * (ld.filmDiagonal * 0.5);
        output->origin.y = 0.0;//input->sy * (ld.filmDiagonal * 0.5);
        output->origin.z = ld.lensShift;



        // determine in which direction to shoot the rays
        // sample disk with proper sample distribution, lensU & lensV (positions on lens) are updated.
        float lensU, lensV = 0.0;
        if (_useImage == false){
            ConcentricSampleDisk(input->lensx, input->lensy, &lensU, &lensV);
        } else {
            // sample bokeh image
            camera->image.bokehSample(input->lensx, input->lensy, &lensU, &lensV);
        }


        // pick between different sampling methods (change to enum)
        // sampling first element is "ground truth" but wastes a lot of rays
        // sampling optimal aperture is efficient, but might not make a whole image
        if (_kolbSamplingMethod == 0){ // using noisy ground truth
            output->dir.x = lensU * ld.lensAperture[0];
            output->dir.y = lensV * ld.lensAperture[0];
            output->dir.z = ld.lensThickness[0];
        } else if (_kolbSamplingMethod == 1){ // using binary aperture search
            output->dir.x = lensU * ld.optimalAperture;
            output->dir.y = lensV * ld.optimalAperture;
            output->dir.z = ld.lensThickness[0];
        }



        output->dir.x = input->sx * ld.lensAperture[0];// * (ld.filmDiagonal / 5.0);
        output->dir.y = input->sy * ld.lensAperture[0];// * (ld.filmDiagonal / 5.0);
        output->dir.z = ld.lensThickness[0];

        traceThroughLensElements(&output->origin, &output->dir, &output->weight, &ld, &dd, draw);

        // flip ray direction
        output->dir *= -1.0;

        //AiMsgInfo("[ZOIC] counter = [%d]", counter);
        draw = false;
    }

    // control to go light stops up and down
    float e2 = _exposureControl * _exposureControl;
    if (_exposureControl > 0){
        output->weight *= 1.0f + e2;
    } else if (_exposureControl < 0){
        output->weight *= 1.0f / (1.0f + e2);
    }


    counter += 1;
}


node_loader {
   if (i > 0){return false;}
   node->methods      = zoicMethods;
   node->output_type  = AI_TYPE_NONE;
   node->name         = "zoic";
   node->node_type    = AI_NODE_CAMERA;
   strcpy(node->version, AI_VERSION);
   return true;
}
