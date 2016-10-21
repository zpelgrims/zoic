// ZOIC - Extended Arnold camera shader with options for:
// Refracting through lens elements read from ground truth lens data (physically accurate lens distortion and optical vignetting)
// Image based bokeh shapes
// Emperical optical vignetting using the thin-lens equation.


// Special thanks to Marc-Antoine Desjardins for the help on the image sampling
// Special thanks to Benedikt Bitterli for the information on emperical optical vignetting
// Special thanks to Tom Minor for the help with C++ (it was needed!)
// Special thanks to Gaetan Guidet for the C++ cleanup on Github.


// (C) Zeno Pelgrims, www.zenopelgrims.com



// TODO

// looks like scale of kolb is off by a large factor! Might have to downsize the whole system by 10
// change to line plane intersection for pp and focus
// Get initial sampling coordinates right (with diagonal of sensor)
// fix origin issue (just sliiiiightly off, but makes a difference to focus)
// Make sure all units are the same (kolb is in mm whilst thin lens is in cm..)
// Add colours to output ("\x1b[1;36m ..... \e[0m")
// fix multithreading (might be the image drawing?)
// change defines to enum
// use concentric mapping + rejection for image based bokeh? Not sure if possible


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


#include <pngwriter.h>

bool draw = false;
float scale = 8.0;
float translateX = 100.0;
float translateY = 1500.0;
pngwriter png(20000, 3000, 0.3,"/Volumes/ZENO_2016/projects/zoic/tests/images/lens_rays.png");


int counter = 0;

// AiTextureLoad function introduced in arnold 4.2.9.0 was modified in 4.2.10.0
// Figure out the right one to call at compile time, contributed by Gaetan Guidet.
#if AI_VERSION_ARCH_NUM > 4
# define AITEXTURELOAD_PROTO2
#else
#  if AI_VERSION_ARCH_NUM == 4
#    if AI_VERSION_MAJOR_NUM > 2
#      define AITEXTURELOAD_PROTO2
#    else
#      if AI_VERSION_MAJOR_NUM == 2
#        if AI_VERSION_MINOR_NUM >= 10
#          define AITEXTURELOAD_PROTO2
#        endif
#        if AI_VERSION_MINOR_NUM == 9
#          define AITEXTURELOAD_PROTO1
#        endif
#      endif
#    endif
#  endif
#endif
#ifdef AITEXTURELOAD_PROTO2
inline bool LoadTexture(const AtString path, void *pixelData){
    return AiTextureLoad(path, true, 0, pixelData);
}
#else
#  ifdef AITEXTURELOAD_PROTO1
inline bool LoadTexture(const AtString path, void *pixelData){
    return AiTextureLoad(path, true,  pixelData);
}
#  else
inline bool LoadTexture(const AtString path, void *pixelData){
    AiMsgError("Current arnold version doesn't have texture loading API");
    return false;
}
#  endif
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
            AiMsgInfo("[ZOIC] ----------------------------------------------");
            AiMsgInfo("[ZOIC] ----------------------------------------------");
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

        // for every pixel
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
    std::vector<double> lensCenter;
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


// little vector class, needed doubles instead of the float vector class arnold provides
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


vec3 vec3normalize(vec3 vector1){
    vec3 normalizedVector;
    double length = sqrt((vector1.x * vector1.x) + (vector1.y * vector1.y) + (vector1.z * vector1.z));

    if(length == 0.0){
        length = 0.0000001;
    }
    if(vector1.x == 0.0){
        vector1.x = 0.0000001;
    }
    if(vector1.y == 0.0){
        vector1.y = 0.0000001;
    }
    if(vector1.z == 0.0){
        vector1.z = 0.0000001;
    }

    normalizedVector.x = vector1.x / length;
    normalizedVector.y = vector1.y / length;
    normalizedVector.z = vector1.z / length;
    return normalizedVector;
}



// Improved concentric mapping code by Dave Cline [peter shirley´s blog]
AtVector2 ConcentricSampleDiskImproved(float ox, float oy) {
    float phi,r;
    float a = 2.0 * ox - 1.0;
    float b = 2.0 * oy - 1.0;
    if (a * a > b * b) {
        r = a;
        phi = (AI_PI / 4.0) * (b / a);
    } else {
        r = b;
        phi = (AI_PI / 2.0) - (AI_PI / 4.0) * (a / b);
    }

    AtVector2 coords = {r * std::cos(phi),  r * std::sin(phi)};
    return coords;
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

    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
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
    int apertureCount = 0;
    for (int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        // check if there is a 0.0 lensRadiusCurvature, which is the aperture.
        if (ld->lensRadiusCurvature[i] == 0.0){
            ld->apertureElement = i;
            AiMsgInfo("[ZOIC] Aperture is lens element number = [%d]", ld->apertureElement);
            apertureCount++;

            if(apertureCount > 1){
                AiMsgError("[ZOIC] Multiple apertures found. Provide lens description with 1 aperture.");
                AiRenderAbort();
            }

            AiMsgInfo("[ZOIC] Adjusted lensRadiusCurvature[%d] [%f] to [99999.0]", i, ld->lensRadiusCurvature[i]);
            ld->lensRadiusCurvature[i] = 99999.0;
        }

        if (ld->lensIOR[i] == 0.0){
            AiMsgInfo("[ZOIC] Changed lensIOR[%d] [%f] to [1.0]", i, ld->lensIOR[i]);
            ld->lensIOR[i] = 1.0;
        }
    }
}



void computeLensCenters(Lensdata *ld){
    ld->lensCenter.clear();
    double summedThickness;

    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        if(i == 0){
            summedThickness = ld->lensThickness[0];
        } else {
            summedThickness += ld->lensThickness[i];
        }

        ld->lensCenter.push_back(summedThickness - ld->lensRadiusCurvature[i]);
    }
}



// RAY SPHERE INTERSECTIONS
vec3 raySphereIntersection(vec3 ray_direction, vec3 ray_origin, vec3 sphere_center, double sphere_radius, bool reverse, bool tracingRealRays){

    vec3 norm_ray_direction = vec3normalize(ray_direction);
    vec3 L = vec3subtr(sphere_center, ray_origin);
    // project the directionvector onto the distancevector
    double tca = vec3dot(L, norm_ray_direction);

    double radius2 = sphere_radius * sphere_radius;
    double d2 = vec3dot(L, L) - tca * tca;

    // if the distance from the ray to the spherecenter is larger than its radius, don't worry about it
    // this is just some arbitrary value that I will use to check for errors
    if (tracingRealRays == true && d2 > radius2){return {0.0, 0.0, 0.0};}

    double thc = sqrt(radius2 - d2);

    vec3 hit_point;

    if(!reverse){
        hit_point.x = ray_origin.x + norm_ray_direction.x * (tca + thc * std::copysign(1.0, sphere_radius));
        hit_point.y = ray_origin.y + norm_ray_direction.y * (tca + thc * std::copysign(1.0, sphere_radius));
        hit_point.z = ray_origin.z + norm_ray_direction.z * (tca + thc * std::copysign(1.0, sphere_radius));
        return hit_point;
    }
   else{
        hit_point.x = ray_origin.x + norm_ray_direction.x * (tca - thc * std::copysign(1.0, sphere_radius));
        hit_point.y = ray_origin.y + norm_ray_direction.y * (tca - thc * std::copysign(1.0, sphere_radius));
        hit_point.z = ray_origin.z + norm_ray_direction.z * (tca - thc * std::copysign(1.0, sphere_radius));
        return hit_point;
    }
}


// COMPUTE NORMAL HITPOINT
vec3 intersectionNormal(vec3 hit_point, vec3 sphere_center, double sphere_radius){
    vec3 normalized = vec3normalize(vec3subtr(sphere_center, hit_point));
    normalized.x *= std::copysign(1.0, sphere_radius);
    normalized.y *= std::copysign(1.0, sphere_radius);
    normalized.z *= std::copysign(1.0, sphere_radius);
    return normalized;
}


// TRANSMISSION VECTOR
vec3 calculateTransmissionVector(double ior1, double ior2, vec3 incidentVector, vec3 normalVector, bool tracingRealRays){

    // VECTORS NEED TO BE NORMALIZED BEFORE USE!
    incidentVector = vec3normalize(incidentVector);
    normalVector = vec3normalize(normalVector);

    double eta;
    if (ior2 == 1.0){
        eta = ior1;
    } else{
        eta = ior1 / ior2;
    }

    double c1 = - vec3dot(incidentVector, normalVector);
    double cs2 = eta * eta * (1.0 - c1 * c1);

    // total internal reflection, can only occur when ior1 > ior2
    if( tracingRealRays && ior1 > ior2 && cs2 > 1.0){
        ld.totalInternalReflection += 1;
        // this is just some arbitrary value that I will use to check for errors
        return {0.0, 0.0, 0.0};
    }

    vec3 transmissionVector;
    transmissionVector.x = (incidentVector.x * eta) + (normalVector.x * ((eta * c1) - sqrt(1.0 - cs2)));
    transmissionVector.y = (incidentVector.y * eta) + (normalVector.y * ((eta * c1) - sqrt(1.0 - cs2)));
    transmissionVector.z = (incidentVector.z * eta) + (normalVector.z * ((eta * c1) - sqrt(1.0 - cs2)));

    return transmissionVector;
}



// LINE LINE INTERSECTIONS
AtVector2 lineLineIntersection(vec3 line1_origin, vec3 line1_direction, vec3 line2_origin, vec3 line2_direction){
    // Get A,B,C of first line - points : ps1 to pe1
    double A1 = line1_direction.y - line1_origin.y;
    double B1 = line1_origin.z - line1_direction.z;
    double C1 = A1 * line1_origin.z + B1 * line1_origin.y;

    // Get A,B,C of second line - points : ps2 to pe2
    double A2 = line2_direction.y - line2_origin.y;
    double B2 = line2_origin.z - line2_direction.z;
    double C2 = A2 * line2_origin.z + B2 * line2_origin.y;

    // Get delta and check if the lines are parallel
    double delta = A1 * B2 - A2 * B1;

    // now return the Vector2 intersection point
    AtVector2 intersectionPoint;
    intersectionPoint.x = (B2 * C1 - B1 * C2) / delta;
    intersectionPoint.y = (A1 * C2 - A2 * C1) / delta;

    return intersectionPoint;
}


// intersection with X axis, but only works well near origin.. strange
vec3 linePlaneIntersection(vec3 rayOrigin, vec3 rayDirection, vec3 normal) {

    // avoid division by 0
    if (rayDirection.x == 0.0){rayDirection.x = 0.000000001;}
    if (rayDirection.y == 0.0){rayDirection.y = 0.000000001;}
    if (rayDirection.z == 0.0){rayDirection.z = 0.000000001;}

    rayDirection = vec3normalize(rayDirection);

    // this would be the proper way, but since coord is 0 0 0 this can be thrown away
    // also normal will always be 0 1 0 so throw other terms away
    //double x = (vec3dot(normal, coord) - vec3dot(normal, rayOrigin)) / vec3dot(normal, rayDirection);
    double a = (- rayOrigin.y) / rayDirection.y;

    vec3 contact;
    contact.x = rayOrigin.x + (rayDirection.x * a);
    contact.y = rayOrigin.y + (rayDirection.y * a);
    contact.z = rayOrigin.z + (rayDirection.z * a);

    return contact;
}



// CALCULATE IMAGE DISTANCE
double calculateImageDistance(double objectDistance, Lensdata *ld, DrawData *dd, bool draw){

    double imageDistance;
    vec3 ray_origin_focus;
    ray_origin_focus.x = 0.0;
    ray_origin_focus.y = 0.0;
    ray_origin_focus.z = objectDistance;

    png.line((objectDistance * scale) + translateX, (9999.0 * scale) + translateY, (objectDistance * scale) + translateX, (-9999.0 * scale) + translateY, 0.64, 1.0, 0.0);

    vec3 ray_direction_focus;
    ray_direction_focus.x = 0.0;
    ray_direction_focus.y = ld->lensAperture[ld->lensAperture.size() - 1] * 0.1;
    ray_direction_focus.z = (- objectDistance);

    double summedThickness_focus = 0.0;

    // go through every lens element
    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        if(i==0){
            for(int k = 0; k < (int)ld->lensRadiusCurvature.size(); k++){
                summedThickness_focus += ld->lensThickness[k];
            }
        }

        // (condition) ? true : false;
        i == 0 ? summedThickness_focus = summedThickness_focus : summedThickness_focus -= ld->lensThickness[ld->lensRadiusCurvature.size() - i];

        vec3 sphere_center;
        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = summedThickness_focus - ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i];

        vec3 hit_point = raySphereIntersection(ray_direction_focus, ray_origin_focus, sphere_center, ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i], true, false);

        vec3 hit_point_normal = intersectionNormal(hit_point, sphere_center, - ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i]);

        png.line((hit_point.x * scale) + translateX, (hit_point.y * scale) + translateY, (ray_origin_focus.x * scale) + translateX, (ray_origin_focus.y * scale) + translateY, 0.64, 1.0, 0.0);


        if(i==0){
            ray_direction_focus = calculateTransmissionVector(1.0, ld->lensIOR[ld->lensRadiusCurvature.size() - 1 - i], ray_direction_focus, hit_point_normal, false);
        } else {
            ray_direction_focus = calculateTransmissionVector(ld->lensIOR[ld->lensRadiusCurvature.size() - i], ld->lensIOR[ld->lensRadiusCurvature.size() - i - 1], ray_direction_focus, hit_point_normal, false);
        }

        // set hitpoint to be the new origin
        ray_origin_focus = hit_point;

        // shoot off rays after last refraction
        if(i == (int)ld->lensRadiusCurvature.size() - 1){
            ray_direction_focus = calculateTransmissionVector(ld->lensIOR[ld->lensRadiusCurvature.size() - 1 - i], 1.0, ray_direction_focus, hit_point_normal, false);

            // find intersection point
            vec3 lineDirection = {0.0, ray_origin_focus.y + ray_direction_focus.y * 1000000.0, ray_origin_focus.z + ray_direction_focus.z * 1000000.0};

            imageDistance = linePlaneIntersection(ray_origin_focus, lineDirection, {0.0, 1.0, 0.0}).z;
            png.line((hit_point.x * scale) + translateX, (hit_point.y * scale) + translateY, (ray_direction_focus.x * 1000000.0 * scale) + translateX, (ray_direction_focus.y * 1000000.0 * scale) + translateY, 0.64, 1.0, 0.0);
        }

    }

    AiMsgInfo("[ZOIC] Object distance = [%f]", objectDistance);
    AiMsgInfo("[ZOIC] Image distance = [%f]", imageDistance);

    png.line((imageDistance * scale) + translateX, (9999.0 * scale) + translateY, (imageDistance * scale) + translateX, (-9999.0 * scale) + translateY, 0.64, 1.0, 0.0);

    return imageDistance;
}




void traceThroughLensElements(vec3 *ray_origin, vec3 *ray_direction, float *weight, Lensdata *ld, DrawData *dd, bool draw){

    vec3 hit_point;
    vec3 hit_point_normal;
    vec3 sphere_center;
    vec3 tmpRayDirection;

    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){

        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = ld->lensCenter[i];

        hit_point = raySphereIntersection(*ray_direction, *ray_origin, sphere_center, ld->lensRadiusCurvature[i], false, true);
        double hitPointHypotenuse = sqrt(hit_point.x * hit_point.x + hit_point.y * hit_point.y);


        if (hit_point.x == 0.0 && hit_point.y == 0.0 && hit_point.z == 0.0){
            ld->vignettedRays += 1;
            *weight = 0.0;
            break;
        }

        // ray hit outside of current lens radius
        if(hitPointHypotenuse > (ld->lensAperture[i]/2.0)){
            ld->vignettedRays += 1;
            *weight = 0.0;
            break;
        }

        // ray hit outside of actual aperture
        if(i == ld->apertureElement && hitPointHypotenuse > ld->userApertureRadius){
            ld->vignettedRays += 1;
           *weight = 0.0;
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
        if(i != (int)ld->lensRadiusCurvature.size() - 1){
            tmpRayDirection = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], *ray_direction, hit_point_normal, true);
        } else { // last lens element
            // assuming the material outside the lens is air
            tmpRayDirection = calculateTransmissionVector(ld->lensIOR[i], 1.0, *ray_direction, hit_point_normal, true);
            if (draw){
                dd->coordinates.push_back(hit_point.z);
                dd->coordinates.push_back(hit_point.y);
                dd->coordinates.push_back(tmpRayDirection.z * 10000000.0);
                dd->coordinates.push_back(tmpRayDirection.y * 10000000.0);
            }

        }

        // check for total internal reflection case
        if (tmpRayDirection.x == 0.0 && tmpRayDirection.y == 0.0 && tmpRayDirection.z == 0.0){
            ld->vignettedRays += 1;
            *weight = 0.0;
            break;
        }

        *ray_direction = tmpRayDirection;

    }

    ld->succesRays += 1;
}




void drawLenses(Lensdata *ld, DrawData *dd){

    vec3 hit_point;
    vec3 sphere_center;
    float summedThickness;
    vec3 rayOrigin = {0.0, 0.0, 0.0};
    vec3 rayDirection;

    std::vector<float> lensFrontSurfaceX;
    std::vector<float> lensFrontSurfaceY;
    std::vector<float> lensBackSurfaceX;
    std::vector<float> lensBackSurfaceY;

    int samples = 1000;

    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){

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
double traceThroughLensElementsForFocalLength(Lensdata *ld){

    double tracedFocalLength;
    double focalPointDistance;
    double principlePlaneDistance;
    double summedThickness_fp = 0.0;

    float rayOriginHeight = ld->lensAperture[0] * 0.1;

    vec3 ray_origin_fp = {0.0, rayOriginHeight, 0.0};
    vec3 ray_direction_fp = {0.0, 0.0, 99999.0};

    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        i == 0 ? summedThickness_fp = ld->lensThickness[0] : summedThickness_fp += ld->lensThickness[i];

        vec3 sphere_center;
        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = summedThickness_fp - ld->lensRadiusCurvature[i];

        vec3 hit_point = raySphereIntersection(ray_direction_fp, ray_origin_fp, sphere_center, ld->lensRadiusCurvature[i], false, false);

        vec3 hit_point_normal = intersectionNormal(hit_point, sphere_center, ld->lensRadiusCurvature[i]);

        if(i != (int)ld->lensRadiusCurvature.size() - 1){
            ray_direction_fp = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], ray_direction_fp, hit_point_normal, true);
        } else { // last element in array
            ray_direction_fp = calculateTransmissionVector(ld->lensIOR[i], 1.0, ray_direction_fp, hit_point_normal, true);

            vec3 pp_line1start; //original parallel ray start
            pp_line1start.x = 0.0;
            pp_line1start.y = rayOriginHeight;
            pp_line1start.z = 0.0;

            vec3 pp_line1end; //original parallel ray end
            pp_line1end.x = 0.0;
            pp_line1end.y = rayOriginHeight;
            pp_line1end.z = 999999.0;

            vec3 pp_line2end; //direction ray end
            pp_line2end.x = 0.0;
            pp_line2end.y = ray_origin_fp.y + (ray_direction_fp.y * 100000.0);
            pp_line2end.z = ray_origin_fp.z + (ray_direction_fp.z * 100000.0);

            principlePlaneDistance = lineLineIntersection(pp_line1start, pp_line1end, ray_origin_fp, pp_line2end).x;
            //principlePlaneDistance = linePlaneIntersection(ray_origin_fp, ray_direction_fp, {0.0, 1.0, 0.0}).x;

            AiMsgInfo("[ZOIC] Principle Plane distance = [%f]", principlePlaneDistance);

            // find intersection point
            vec3 axialStart = {0.0, 0.0, 0.0};
            vec3 axialEnd = {0.0, 0.0, 999999.0};
            vec3 lineDirection;
            lineDirection.x = 0.0;
            lineDirection.y = ray_origin_fp.y + ray_direction_fp.y * 100000.0;
            lineDirection.z = ray_origin_fp.z + ray_direction_fp.z * 100000.0;

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



bool traceThroughLensElementsForApertureSize(vec3 ray_origin, vec3 ray_direction, Lensdata *ld){
    vec3 hit_point_normal;
    vec3 sphere_center;

    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        sphere_center = {0.0, 0.0, ld->lensCenter[i]};

        ray_origin = raySphereIntersection(ray_direction, ray_origin, sphere_center, ld->lensRadiusCurvature[i], false, true);

        double hitPointHypotenuse = sqrt(ray_origin.x * ray_origin.x + ray_origin.y * ray_origin.y);
        if(i == ld->apertureElement && hitPointHypotenuse > ld->userApertureRadius){
            return false;
        }

        hit_point_normal = intersectionNormal(ray_origin, sphere_center, ld->lensRadiusCurvature[i]);

        ray_direction = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], ray_direction, hit_point_normal, true);
    }

    return true;
}



void adjustFocalLength(Lensdata *ld){
    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        ld->lensRadiusCurvature[i] *= ld->focalLengthRatio;
        ld->lensThickness[i] *= ld->focalLengthRatio;
        ld->lensAperture[i] *= ld->focalLengthRatio;
    }
}



node_parameters {
    AiParameterFLT("sensorWidth", 3.6f); // 35mm film
    AiParameterFLT("sensorHeight", 2.4f); // 35 mm film
    AiParameterFLT("focalLength", 100.0f); // distance between sensor and lens
    AiParameterFLT("fStop", 4.4f);
    AiParameterFLT("focalDistance", 120.0f); // distance from lens to focal point
    AiParameterBOOL("useImage", false);
    AiParameterStr("bokehPath", ""); //bokeh shape image location
    AiParameterBOOL("kolb", true);
    AiParameterStr("lensDataPath", ""); // lens data file location
    AiParameterBOOL("kolbSamplingMethod", true);
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
        if ((int)ld.lensRadiusCurvature.size() == 0 ||
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

        // calculate focal length by tracing a parallel ray through the lens system (2nd time for new focallength)
        traceThroughLensElementsForFocalLength(&ld);

        // calculate how much origin should be shifted so that the image distance at a certain object distance falls on the film plane
        ld.lensShift = calculateImageDistance(_focalDistance * 10.0, &ld, &dd, false);

        // find how far the aperture is from the film plane
        ld.apertureDistance = 0.0;
        for(int i = 0; i < (int)ld.lensRadiusCurvature.size(); i++){
            ld.apertureDistance += ld.lensThickness[i];
            if(i == ld.apertureElement){
                AiMsgInfo("[ZOIC] Aperture distance after lens shift = [%f]", ld.apertureDistance);
                break;
            }
        }


        // precompute lens centers
        computeLensCenters(&ld);


        // search for ideal max height to shoot rays to on first lens element, by tracing test rays and seeing which one fails
        // maybe this varies based on where on the filmplane we are shooting the ray from? In this case this wouldn´t work..
        // and I don't think it does..
        // use lookup table instead!
        if(_kolbSamplingMethod == 1){
            int sampleCount = 1024;
            vec3 sampleOrigin = {0.0, 0.0, 0.0};
            for (int i = 0; i < sampleCount; i++){
                float heightVariation = ld.lensAperture[0] / float(sampleCount);
                vec3 sampleDirection = {0.0, heightVariation * float(i), float(ld.lensThickness[0])};

                if (!traceThroughLensElementsForApertureSize(sampleOrigin, sampleDirection, &ld)){
                    AiMsgInfo("[ZOIC] Positive failure at sample [%d] out of [%d]", i, sampleCount);
                    ld.optimalAperture = sampleDirection.y - heightVariation;
                    AiMsgInfo("[ZOIC] Optimal max height to shoot rays to on first lens element = [%f]", ld.optimalAperture);
                    break;
                }
            }
        }



        // lookup table method
        // Compute the ideal directions per origin point. Store these in a map of a map.
        // Then query these, and depending on the value inbetween do some kind of linear interpolation with the next up value.
        // otherwise it might be jaggy

        /*
        // PSEUDOCODE

        number of samples = 64;
        for every sample
            r0 = i / samples * 2 * filmdiagonal
            r1 = i+1 / samples * 2 * filmDiagonal

            filmsamples = 16
            rearsamples = 16

            rearZ = last element Z
            rearAperture = aperture Z

            int numhit = 0
            int numtraced = 0

            for every film sample (i)
                vec3 filmP = {lerp(i/filmsamples, r0, r1), 0, 0}
                for (int x = - rearsamples; x <= rearsamples, ++x)
                    for (int y = - rearsamples; y <= rearsamples; ++y)
                        vec3 rearP = {(rearAperture / RearSamples), y * (rearAperture / RearSamples), rearZ}
                        if tracefromfilm = true
                            add rearP.x and rearP.y to vector

        */

        /* HOW IT SHOULD BE DONE

        // Compute exit pupil lookup table
        int NumSamples = 64;
        for (int i = 0; i < NumSamples; ++i) {
            float r0 = float(i) / NumSamples * 0.5f * m_diagonal;
            float r1 = float(i + 1) / NumSamples * 0.5f * m_diagonal;
            auto bounds = computeExitPupilBounds(r0, r1);
            m_exitPupilBounds.emplace_back(bounds);
        }

        BoundingBox2f RealisticCamera::computeExitPupilBounds(float r0, float r1) const {
            BoundingBox2f bounds;

            const int FilmSamples = 16;
            const int RearSamples = 16;

            float rearZ = getRearElement().z;
            float rearAperture = getRearElement().aperture;

            int numHit = 0;
            int numTraced = 0;
            for (int i = 0; i <= FilmSamples; ++i) {
                Vector3f filmP(nori::lerp(float(i) / FilmSamples, r0, r1), 0.f, 0.f);
                for (int x = -RearSamples; x <= RearSamples; ++x) {
                    for (int y = -RearSamples; y <= RearSamples; ++y) {
                        Vector3f rearP(x * (rearAperture / RearSamples), y * (rearAperture / RearSamples), rearZ);
                        Ray3f sceneRay;
                        if (traceFromFilm<false, true>(Ray3f(filmP, (rearP - filmP).normalized()), sceneRay, [] (const Ray3f &) {} )) {
                            bounds.expandBy(Vector2f(rearP.x(), rearP.y()));
                            ++numHit;
                        }
                        ++numTraced;
                    }
                }
            }

            // Expand due to sampling error and clip to actual aperture
            bounds.expandBy(rearAperture / RearSamples);
            bounds.clip(BoundingBox2f(-rearAperture, rearAperture));

            return bounds;
        }

        */
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

    png.close();
}


camera_create_ray {
    // get values
    const AtParamValue* params = AiNodeGetParams(node);
    cameraData *camera = (cameraData*) AiCameraGetLocalData(node);

    if (counter == 100000){
        draw = true;
        for (int i = 0; i < (int)dd.coordinates.size() / 4; i++){
            png.line((dd.coordinates[i * 4] * scale) + translateX,
                    (dd.coordinates[(i * 4) + 1] * scale) + translateY,
                    (dd.coordinates[(i * 4) + 2] * scale) + translateX,
                    (dd.coordinates[(i * 4) + 3] * scale) + translateY, 0.6, 0.6, 0.6);
        }
        for (int i = 0; i < (int)dd.points.size() / 2; i++){
            png.filledcircle((dd.points[i * 2] * scale) + translateX, (dd.points[(i * 2) + 1] * scale) + translateY, 2.0, 1.0, 1.0, 1.0);
        }
        dd.coordinates.clear();
        dd.points.clear();
        counter = 0;
    }


    // chang this to an enum, thinlens, raytraced
    if(!_kolb){

        // create point on lens
        AtPoint p;
        p.x = input->sx * camera->tan_fov;
        p.y = input->sy * camera->tan_fov;
        p.z = 1.0;

        // compute direction
        output->dir = AiV3Normalize(p - output->origin);



        // DOF CALCULATIONS
        if (_useDof == true) {

            // Initialize point on lens
            float lensU, lensV = 0.0;

            // sample disk with proper sample distribution, lensU & lensV (positions on lens) are updated.
            if (_useImage == false){
                AtVector2 lens = ConcentricSampleDiskImproved(input->lensx, input->lensy);
                lensU = lens.x;
                lensV = lens.y;

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

            // Emperical optical vignetting (cat eye effect)
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
                if (ABS(pointHypotenuse) > virtualApertureTrueRadius){
                    output->weight = 0.0f;
                }

                // inner highlight,if point is within domain between lens radius and new inner radius (defined by the width)
                // adding weight to opposite edges to get nice rim on the highlights
                else if (ABS(pointHypotenuse) < virtualApertureTrueRadius && ABS(pointHypotenuse) > (virtualApertureTrueRadius - _highlightWidth)){
                    output->weight *= _highlightStrength *
                                      (1.0 - (virtualApertureTrueRadius - ABS(pointHypotenuse))) *
                                      sqrt(input->sx * input->sx + input->sy * input->sy);
                }
            }
        }

        if (draw){
            dd.coordinates.push_back(output->origin.z);
            dd.coordinates.push_back(output->origin.y);
            dd.coordinates.push_back(output->dir.z * 100000000.0);
            dd.coordinates.push_back(output->dir.y * 100000000.0);
        }

        draw = false;
        // now looking down -Z
        output->dir.z *= -1.0;

    }


    if(_kolb){

        vec3 origin, direction;

        origin.x = 0.0;
        origin.y = 0.0;
        origin.z = ld.lensShift;

        //origin.x = input->sx * (ld.filmDiagonal * 0.5);
        //origin.y = input->sy * (ld.filmDiagonal * 0.5);
        //origin.z = ld.lensShift;

        // sample disk with proper sample distribution
        float lensU, lensV = 0.0;
        if (_useImage == false){
            AtVector2 lens = ConcentricSampleDiskImproved(input->lensx, input->lensy);
            lensU = lens.x;
            lensV = lens.y;
        } else { // sample bokeh image
            camera->image.bokehSample(input->lensx, input->lensy, &lensU, &lensV);
        }


        // pick between different sampling methods (change to enum)
        // sampling first element is "ground truth" but wastes a lot of rays
        // sampling optimal aperture is efficient, but might not make a whole image
        if (_kolbSamplingMethod == 0){ // using noisy ground truth
            direction.x = lensU * ld.lensAperture[0];
            direction.y = lensV * ld.lensAperture[0];
            direction.z = ld.lensThickness[0];
        } else if (_kolbSamplingMethod == 1){ // using binary aperture search
            direction.x = lensU * ld.optimalAperture;
            direction.y = lensV * ld.optimalAperture;
            direction.z = ld.lensThickness[0];
        }

        direction.x = 0.0; //tmp

        //direction.x = input->sx * ld.lensAperture[0];// * (ld.filmDiagonal / 5.0);
        //direction.y = input->sy * ld.lensAperture[0];// * (ld.filmDiagonal / 5.0);
        //direction.z = ld.lensThickness[0];

        traceThroughLensElements(&origin, &direction, &output->weight, &ld, &dd, draw);

        /*
        // count rays
        if (traceRay){
            ld.succesRays += 1;
        } else {
            ld.vignettedRays += 1;
            output->weight = 0.0;
        }
        */

        output->origin = {(float)origin.x, (float)origin.y, (float)origin.z};
        output->dir = {(float)direction.x, (float)direction.y, (float)direction.z};

        //AiMsgInfo("[ZOIC] output->origin [%f %f %f]", output->origin.x, output->origin.y, output->origin.z);
        //AiMsgInfo("[ZOIC] output->dir [%f %f %f]", output->dir.x, output->dir.y, output->dir.z);

        // flip ray direction
        output->dir *= -1.0;
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
