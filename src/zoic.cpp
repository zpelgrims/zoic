// ZOIC - Extended Arnold camera shader with options for:
// Refracting through lens elements read from ground truth lens data
        // Physically plausible lens distortion and optical vignetting
// Image based bokeh shapes
// Emperical optical vignetting using the thin-lens equation

// Special thanks to Marc-Antoine Desjardins for the information on the image sampling
// Special thanks to Benedikt Bitterli for the information on emperical optical vignetting
// Special thanks to Tom Minor for getting me started with C++
// Special thanks to Gaetan Guidet for the C++ cleanup on Github.

// (C) Zeno Pelgrims, www.zenopelgrims.com

// TODO
// focus still doesn´t work
// fix the difference between thin lens and kolb fstop result
// implement LUT
// make visualisation for all parameters
// Improve reading safety (include spaces, etc) -> multiple delimiters
// Add colours to output ("\x1b[1;36m ..... \e[0m")
// change defines to enum
// use concentric mapping + rejection for image based bokeh? Not sure if possible
// implement correct exposure based on film plane sample position

/* COMPILE AT WORK:
g++ -std=c++11 -O3 -I"C:/ilionData/Users/zeno.pelgrims/Desktop/Arnold-4.2.13.4-windows/include" -L"C:/ilionData/Users/zeno.pelgrims/Desktop/Arnold-4.2.13.4-windows/bin" -lai -DNO_OIIO --shared C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/zoic.cpp -o C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/zoic.dll
C:/ilionData/Users/zeno.pelgrims/Desktop/Arnold-4.2.13.4-windows/bin/kick -i C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/lights.ass -g 2.2 -v 2 -l C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile -o C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/testScene_lights.exr -set options.threads 1 -dp
C:/ilionData/Users/zeno.pelgrims/Desktop/Arnold-4.2.13.4-windows/bin/kick -i C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/distance.ass -g 2.2 -v 2 -l C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile -set options.skip_license_check on -l "C:/Program Files/Ilion/IlionMayaFramework/2015/modules/mtoa/1.2.7.2.2-4.2.13.6/shaders" -o C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/testScene_distance.exr -set options.threads 1 -dp
*/



#include <ai.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>

std::ofstream myfile;
bool draw = false;
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

#define _sensorWidth (params[0].FLT)
#define _sensorHeight (params[1].FLT)
#define _focalLength (params[2].FLT)
#define _fStop (params[3].FLT)
#define _focalDistance (params[4].FLT)
#define _useImage (params[5].BOOL)
#define _bokehPath (params[6].STR)
#define _kolb (params[7].BOOL)
#define _lensDataPath (params[8].STR)
#define _kolbSamplingMethod (params[9].BOOL)
#define _useDof (params[10].BOOL)
#define _opticalVignettingDistance (params[11].FLT)
#define _opticalVignettingRadius (params[12].FLT)
#define _highlightWidth (params[13].FLT)
#define _highlightStrength (params[14].FLT)
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
            //AiMsgWarning("[zoic] %.9f larger than last biggest cdfColumn[%d][%d] = %.9f", randomNumberColumn, r, x-1, cdfColumn[startPixel+x-1]);
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
    double originShift;
    double focalDistance;
} ld;


// Improved concentric mapping code by Dave Cline [peter shirley´s blog]
inline void ConcentricSampleDiskImproved(float ox, float oy, float *lensU, float *lensV) {
    float phi, r;
    float a = 2.0 * ox - 1.0;
    float b = 2.0 * oy - 1.0;

    if (a * a > b * b) {
        r = a;
        phi = (AI_PI / 4.0) * (b / a);
    } else {
        r = b;
        phi = (AI_PI / 2.0) - (AI_PI / 4.0) * (a / b);
    }

    *lensU = r * std::cos(phi);
    *lensV = r * std::sin(phi);
}



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
    int commentCounter = 0;

    AiMsgInfo("[ZOIC] ##############################################");
    AiMsgInfo("[ZOIC] ############# READING LENS DATA ##############");
    AiMsgInfo("[ZOIC] ##############################################");
    AiMsgInfo("[ZOIC] Welcome to the nerd club :-D");
    while (getline(lensDataFile, line))
    {
        if (line.length() == 0 || line[0] == '#'){
            ++commentCounter;
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
    AiMsgInfo( "%-40s %12d", "[ZOIC] Comment lines ignored", commentCounter);
    AiMsgInfo("[ZOIC] ##############################################");
    AiMsgInfo("[ZOIC] #   ROC       Thickness     IOR     Aperture #");
    AiMsgInfo("[ZOIC] ##############################################");
    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        AiMsgInfo("[ZOIC] %10.4f  %10.4f  %10.4f  %10.4f", ld->lensRadiusCurvature[i], ld->lensThickness[i], ld->lensIOR[i], ld->lensAperture[i]);
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

            apertureCount++;
            if(apertureCount > 1){
                AiMsgError("[ZOIC] Multiple apertures found. Provide lens description with 1 aperture.");
                AiRenderAbort();
            }
            AiMsgInfo("[ZOIC] Adjusted ROC[%d] [%.4f] to [99999.0]", i, ld->lensRadiusCurvature[i]);
            ld->lensRadiusCurvature[i] = 99999.0;
        }
        if (ld->lensIOR[i] == 0.0){
            AiMsgInfo("[ZOIC] Changed IOR[%d] [%.4f] to [1.0000]", i, ld->lensIOR[i]);
            ld->lensIOR[i] = 1.0;
        }
    }
    AiMsgInfo( "%-40s %12d", "[ZOIC] Aperture is lens element number", ld->apertureElement);
    // scale from mm to cm
    for (int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        ld->lensRadiusCurvature[i] *= 0.1;
        ld->lensThickness[i] *= 0.1;
        ld->lensAperture[i] *= 0.1;
    }
    // move lenses so last lens is at origin
    double summedThickness = 0.0;
    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        summedThickness += ld->lensThickness[i];
    }
    ld->lensThickness[0] -= summedThickness;
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


inline AtVector raySphereIntersection(AtVector ray_direction, AtVector ray_origin, AtVector sphere_center, double sphere_radius, bool reverse, bool tracingRealRays){
    ray_direction = AiV3Normalize(ray_direction);
    AtVector L = sphere_center - ray_origin;
   // project the directionvector onto the distancevector
    double tca = AiV3Dot(L, ray_direction);
    double radius2 = sphere_radius * sphere_radius;
   double d2 = AiV3Dot(L, L) - tca * tca;
    // if the distance from the ray to the spherecenter is larger than its radius, don't worry about it
    // some arbitrary value that I use to check for errors
    if (tracingRealRays == true && d2 > radius2){return {0.0, 0.0, 0.0};}
    double thc = std::sqrt(ABS(radius2 - d2));
    if(!reverse){
        return ray_origin + ray_direction * (tca + thc * std::copysign(1.0, sphere_radius));
    } else{
        return ray_origin + ray_direction * (tca - thc * std::copysign(1.0, sphere_radius));
    }
}


inline AtVector intersectionNormal(AtVector hit_point, AtVector sphere_center, double sphere_radius){
    return AiV3Normalize(sphere_center - hit_point) * std::copysign(1.0, sphere_radius);
}


inline AtVector calculateTransmissionVector(double ior1, double ior2, AtVector incidentVector, AtVector normalVector, bool tracingRealRays){
    incidentVector = AiV3Normalize(incidentVector);
    normalVector = AiV3Normalize(normalVector);
    double eta;
    ior2 == 1.0 ? eta = ior1 : eta = ior1 / ior2;
    double c1 = - AiV3Dot(incidentVector, normalVector);
    double cs2 = eta * eta * (1.0 - c1 * c1);
    // total internal reflection, can only occur when ior1 > ior2
    if( tracingRealRays && ior1 > ior2 && cs2 > 1.0){
        ++ld.totalInternalReflection;
        // arbitrary value that I use to check for errors
        return {0.0, 0.0, 0.0};
    }
    return (incidentVector * eta) + (normalVector * ((eta * c1) - std::sqrt(ABS(1.0 - cs2))));
}


AtVector2 lineLineIntersection(AtVector line1_origin, AtVector line1_direction, AtVector line2_origin, AtVector line2_direction){
    float A1 = line1_direction.y - line1_origin.y;
    float B1 = line1_origin.z - line1_direction.z;
    float C1 = A1 * line1_origin.z + B1 * line1_origin.y;
    float A2 = line2_direction.y - line2_origin.y;
    float B2 = line2_origin.z - line2_direction.z;
    float C2 = A2 * line2_origin.z + B2 * line2_origin.y;
    float delta = A1 * B2 - A2 * B1;
    return {(B2 * C1 - B1 * C2) / delta, (A1 * C2 - A2 * C1) / delta};
}


AtVector linePlaneIntersection(AtVector rayOrigin, AtVector rayDirection) {
    // intersection with y = 0
    AtVector coord = {100.0, 0.0, 100.0};
    AtVector planeNormal = {0.0, 1.0, 0.0};
    rayDirection = AiV3Normalize(rayDirection);
    coord = AiV3Normalize(coord);

    double x = (AiV3Dot(coord, planeNormal) - AiV3Dot(planeNormal, rayOrigin)) / AiV3Dot(planeNormal, rayDirection);
    return rayOrigin + (rayDirection * x);
}

double calculateImageDistance(double objectDistance, Lensdata *ld, bool draw){
    double imageDistance;
    AtVector ray_origin_focus;
    ray_origin_focus.x = 0.0;
    ray_origin_focus.y = 0.0;
    ray_origin_focus.z = objectDistance;
    AtVector ray_direction_focus;
    ray_direction_focus.x = 0.0;
    ray_direction_focus.y = (ld->lensAperture[ld->lensAperture.size() - 1] / 2.0) * 0.3;
    ray_direction_focus.z = (- objectDistance * 1.1);
    double summedThickness_focus = 0.0;
    for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        if(i==0){
            for(int k = 0; k < (int)ld->lensRadiusCurvature.size(); k++){
                summedThickness_focus += ld->lensThickness[k];
            }
        }
        i == 0 ? summedThickness_focus = summedThickness_focus : summedThickness_focus -= ld->lensThickness[ld->lensRadiusCurvature.size() - i];
        AtVector sphere_center;
        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = summedThickness_focus - ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i];
        AtVector hit_point = raySphereIntersection(ray_direction_focus, ray_origin_focus, sphere_center, ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i], true, false);
        AtVector hit_point_normal = intersectionNormal(hit_point, sphere_center, - ld->lensRadiusCurvature[ld->lensRadiusCurvature.size() - 1 - i]);
        if(draw){
            myfile << std::fixed << std::setprecision(10) << ray_origin_focus.z;
            myfile << " ";
            myfile << std::fixed << std::setprecision(10) << ray_origin_focus.y;
            myfile << " ";
            myfile << std::fixed << std::setprecision(10) << hit_point.z;
            myfile << " ";
            myfile << std::fixed << std::setprecision(10) << hit_point.y;
            myfile << " ";
        }
        if(i==0){
            ray_direction_focus = calculateTransmissionVector(1.0, ld->lensIOR[ld->lensRadiusCurvature.size() - i - 1], ray_direction_focus, hit_point_normal, false);
        } else {
            ray_direction_focus = calculateTransmissionVector(ld->lensIOR[ld->lensRadiusCurvature.size() - i], ld->lensIOR[ld->lensRadiusCurvature.size() - i - 1], ray_direction_focus, hit_point_normal, false);
        }
        // shoot off rays after last refraction
        if(i == (int)ld->lensRadiusCurvature.size() - 1){
            // NOT ONE CALCULATION TOO MUCH HERE????
            // ray_direction_focus = calculateTransmissionVector(ld->lensIOR[ld->lensRadiusCurvature.size() - 1 - i], 1.0, ray_direction_focus, hit_point_normal, false);

            // find intersection point
            imageDistance = linePlaneIntersection(hit_point, ray_direction_focus).z;

            if(draw){
                myfile << std::fixed << std::setprecision(10) << hit_point.z + ray_direction_focus.z * 100000.0;
                myfile << " ";
                myfile << std::fixed << std::setprecision(10) << hit_point.y + ray_direction_focus.y * 100000.0;
                myfile << " ";
                myfile << std::fixed << std::setprecision(10) << hit_point.z;
                myfile << " ";
                myfile << std::fixed << std::setprecision(10) << hit_point.y;
                myfile << " ";
            }
        }

        // set hitpoint to be the new origin
        ray_origin_focus = hit_point;
    }
    AiMsgInfo( "%-40s %12.8f", "[ZOIC] Object distance [cm]", objectDistance);
    AiMsgInfo( "%-40s %12.8f", "[ZOIC] Image distance [cm]", imageDistance);
    return imageDistance;
}


bool traceThroughLensElements(AtVector *ray_origin, AtVector *ray_direction, Lensdata *ld, bool draw){
   AtVector hit_point;
   AtVector hit_point_normal;
   AtVector sphere_center;
   AtVector tmpRayDirection;

   for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
        sphere_center.x = 0.0;
        sphere_center.y = 0.0;
        sphere_center.z = ld->lensCenter[i];

        hit_point = raySphereIntersection(*ray_direction, *ray_origin, sphere_center, ld->lensRadiusCurvature[i], false, true);

        double hitPoint2 = hit_point.x * hit_point.x + hit_point.y * hit_point.y;

        if (hit_point.x == 0.0 && hit_point.y == 0.0 && hit_point.z == 0.0){
           return false;
       }

       // ray hit outside of current lens radius
       if(hitPoint2 > (ld->lensAperture[i]/2.0) * (ld->lensAperture[i]/2.0)){
           return false;
       }

       // ray hit outside of actual aperture
       if((i == ld->apertureElement) && (hitPoint2 > ld->userApertureRadius * ld->userApertureRadius)){
           return false;
       }

       hit_point_normal = intersectionNormal(hit_point, sphere_center, ld->lensRadiusCurvature[i]);

       if(draw){
           myfile << std::fixed << std::setprecision(10) << ray_origin->z;
           myfile << " ";
           myfile << std::fixed << std::setprecision(10) << ray_origin->y;
           myfile << " ";
           myfile << std::fixed << std::setprecision(10) << hit_point.z;
           myfile << " ";
           myfile << std::fixed << std::setprecision(10) << hit_point.y;
           myfile << " ";
       }

       *ray_origin = hit_point;

       // if not last lens element
       if(i != (int)ld->lensRadiusCurvature.size() - 1){
           tmpRayDirection = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], *ray_direction, hit_point_normal, true);
       } else { // last lens element
           // assuming the material outside the lens is air
           tmpRayDirection = calculateTransmissionVector(ld->lensIOR[i], 1.0, *ray_direction, hit_point_normal, true);

           if (draw){
               myfile << std::fixed << std::setprecision(10) << hit_point.z;
               myfile << " ";
               myfile << std::fixed << std::setprecision(10) << hit_point.y;
               myfile << " ";
               myfile << std::fixed << std::setprecision(10) << hit_point.z + tmpRayDirection.z * 10000.0;
               myfile << " ";
               myfile << std::fixed << std::setprecision(10) << hit_point.y + tmpRayDirection.y * 10000.0;
               myfile << " ";
           }
       }

       // check for total internal reflection case
       if (tmpRayDirection.x == 0.0 && tmpRayDirection.y == 0.0 && tmpRayDirection.z == 0.0){
           return false;
       }

       *ray_direction = tmpRayDirection;
   }

    return true;
}


double traceThroughLensElementsForFocalLength(Lensdata *ld){
   double tracedFocalLength;
   double focalPointDistance;
   double principlePlaneDistance;
   double summedThickness_fp = 0.0;

   float rayOriginHeight = ld->lensAperture[0] * 0.1;

   AtVector ray_origin_fp = {0.0, rayOriginHeight, 0.0};
   AtVector ray_direction_fp = {0.0, 0.0, 99999.0};

   for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
       // need to keep the summedthickness method since the sphere centers get computed only later on
       i == 0 ? summedThickness_fp = ld->lensThickness[0] : summedThickness_fp += ld->lensThickness[i];

       AtVector sphere_center;
       sphere_center.x = 0.0;
       sphere_center.y = 0.0;
       sphere_center.z = summedThickness_fp - ld->lensRadiusCurvature[i];

       AtVector hit_point = raySphereIntersection(ray_direction_fp, ray_origin_fp, sphere_center, ld->lensRadiusCurvature[i], false, false);

       AtVector hit_point_normal = intersectionNormal(hit_point, sphere_center, ld->lensRadiusCurvature[i]);

       if(i != (int)ld->lensRadiusCurvature.size() - 1){
           ray_direction_fp = calculateTransmissionVector(ld->lensIOR[i], ld->lensIOR[i+1], ray_direction_fp, hit_point_normal, true);
       } else { // last element in array
           ray_direction_fp = calculateTransmissionVector(ld->lensIOR[i], 1.0, ray_direction_fp, hit_point_normal, true);

           // original parallel ray start and end
           AtVector pp_line1start = {0.0, rayOriginHeight, 0.0};
           AtVector pp_line1end = {0.0, rayOriginHeight, 999999.0};

           // direction ray end
           AtVector pp_line2end;
           pp_line2end.x = 0.0;
           pp_line2end.y = ray_origin_fp.y + (ray_direction_fp.y * 100000.0);
           pp_line2end.z = ray_origin_fp.z + (ray_direction_fp.z * 100000.0);

           principlePlaneDistance = lineLineIntersection(pp_line1start, pp_line1end, ray_origin_fp, pp_line2end).x;
           AiMsgInfo( "%-40s %12.8f", "[ZOIC] Principle Plane distance [cm]", principlePlaneDistance);

           focalPointDistance = linePlaneIntersection(ray_origin_fp, ray_direction_fp).z;
           AiMsgInfo( "%-40s %12.8f", "[ZOIC] Focal point distance [cm]", focalPointDistance);
       }

       ray_origin_fp = hit_point;
   }

   tracedFocalLength = focalPointDistance - principlePlaneDistance;
   AiMsgInfo( "%-40s %12.8f", "[ZOIC] Raytraced Focal Length [cm]", tracedFocalLength);

   return tracedFocalLength;
}



bool traceThroughLensElementsForApertureSize(AtVector ray_origin, AtVector ray_direction, Lensdata *ld){
   AtVector hit_point_normal;
   AtVector sphere_center = {0.0, 0.0, 0.0};

   for(int i = 0; i < (int)ld->lensRadiusCurvature.size(); i++){
       sphere_center.z = ld->lensCenter[i];

       ray_origin = raySphereIntersection(ray_direction, ray_origin, sphere_center, ld->lensRadiusCurvature[i], false, true);

       double hitPoint2 = ray_origin.x * ray_origin.x + ray_origin.y * ray_origin.y;
       if(i == ld->apertureElement && hitPoint2 > (ld->userApertureRadius * ld->userApertureRadius)){
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




void writeToFile(Lensdata *ld){
    myfile << "LENSES{";
    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        // lenscenter, radius, angle
        myfile << std::fixed << std::setprecision(10) << ld->lensCenter[i];
        myfile << " ";
        myfile << std::fixed << std::setprecision(10) << ld->lensRadiusCurvature[i];
        myfile << " ";
        myfile << std::fixed << std::setprecision(10) << (std::asin((ld->lensAperture[i] * 0.5) / ld->lensRadiusCurvature[i])) * (180 / AI_PI);
        myfile << " ";
    }
    myfile << "}\n";

    myfile << "IOR{";
    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        myfile << std::fixed << std::setprecision(10) << ld->lensIOR[i];
        myfile << " ";
    }
    myfile << "}\n";

    myfile << "APERTUREELEMENT{";
    myfile << std::fixed << std::setprecision(10) << ld->apertureElement;
    myfile << "}\n";

    myfile << "APERTUREDISTANCE{";
    myfile << std::fixed << std::setprecision(10) << ld->apertureDistance;
    myfile << "}\n";

    myfile << "APERTURE{";
    myfile << std::fixed << std::setprecision(10) << ld->userApertureRadius;
    myfile << "}\n";

    myfile << "APERTUREMAX{";
    double maxAperture = 0.0;
    for(int i = 0; i < ld->lensRadiusCurvature.size(); i++){
        if (ld->lensAperture[i] > maxAperture){
            maxAperture = ld->lensAperture[i];
        }
    }
    myfile << std::fixed << std::setprecision(10) << maxAperture;
    myfile << "}\n";

    myfile << "FOCUSDISTANCE{";
    myfile << std::fixed << std::setprecision(10) << ld->focalDistance;
    myfile << "}\n";

    myfile << "IMAGEDISTANCE{";
    myfile << std::fixed << std::setprecision(10) << ld->originShift;
    myfile << "}\n";

    myfile << "SENSORHEIGHT{";
    myfile << std::fixed << std::setprecision(10) << 1.7;
    myfile << "}\n";

}




node_parameters {
   AiParameterFLT("sensorWidth", 3.6); // 35mm film
   AiParameterFLT("sensorHeight", 2.4); // 35 mm film
   AiParameterFLT("focalLength", 5.0); // distance between sensor and lens in cm
   AiParameterFLT("fStop", 3.4);
   AiParameterFLT("focalDistance", 100.0); // distance from lens to focal point
   AiParameterBOOL("useImage", false);
   AiParameterStr("bokehPath", ""); //bokeh shape image location
   AiParameterBOOL("kolb", true);
   AiParameterStr("lensDataPath", ""); // lens data file location
   AiParameterBOOL("kolbSamplingMethod", true);
   AiParameterBOOL("useDof", true);
   AiParameterFLT("opticalVignettingDistance", 0.0); //distance of the opticalVignetting virtual aperture
   AiParameterFLT("opticalVignettingRadius", 0.0); // 1.0 - .. range float, to multiply with the actual aperture radius
   AiParameterFLT("highlightWidth", 0.2);
   AiParameterFLT("highlightStrength", 10.0);
   AiParameterFLT("exposureControl", 0.0);

}



node_initialize {
   cameraData *camera = new cameraData();
   AiCameraInitialize(node, (void*)camera);

}


node_update {
   AiCameraUpdate(node, false);
   cameraData *camera = (cameraData*) AiCameraGetLocalData(node);
   AtNode* options = AiUniverseGetOptions();

   // create file to transfer data to python drawing module
   // myfile.open ("/Volumes/ZENO_2016/projects/zoic/src/draw.zoic");
   myfile.open ("C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/draw.zoic", std::ofstream::out | std::ofstream::trunc);
    //myfile.open ("/Users/kassiawordley/Desktop/zoic_kassia/draw.zoic", std::ofstream::out | std::ofstream::trunc);

   if(!_kolb){
       //theta = 2arctan*(sensorSize/focalLength)
       camera->fov = 2.0f * atan((_sensorWidth / (2.0f * _focalLength))); // in radians
       camera->tan_fov = tanf(camera->fov/ 2.0f);

       // apertureRadius = focalLength / 2*fStop
       camera->apertureRadius = (_focalLength) / (2.0f * _fStop);
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
       // reset variables
       ld.lensRadiusCurvature.clear();
       ld.lensThickness.clear();
       ld.lensIOR.clear();
       ld.lensAperture.clear();
       ld.lensCenter.clear();
       ld.vignettedRays = 0;
       ld.succesRays = 0;
       ld.totalInternalReflection = 0;
       ld.userApertureRadius = 0.0;
       ld.apertureElement = 0;
       ld.apertureDistance = 0.0;
       ld.optimalAperture = 0.0;
       ld.focalLengthRatio = 0.0;
       ld.originShift = 0.0;

       // Update shaderData variables
       ld.xres = static_cast<float>(AiNodeGetInt(options,"xres"));
       ld.yres = static_cast<float>(AiNodeGetInt(options,"yres"));

       // not sure if this is the right way to do it.. probably more to it than this!
       // these values seem to produce the same image as the other camera which is correct.. hey ho
       ld.filmDiagonal = std::sqrt(_sensorWidth *_sensorWidth + _sensorHeight * _sensorHeight);

       // check if file is supplied
       // string is const char* so have to do it the oldskool way
       if ((_lensDataPath != NULL) && (_lensDataPath[0] == '\0')){
          AiMsgError("[ZOIC] Lens Data Path is empty");
          AiRenderAbort();
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
           AiRenderAbort();
       }

       // look for invalid numbers that would mess it all up bro
       cleanupLensData(&ld);

       // calculate focal length by tracing a parallel ray through the lens system
       float kolbFocalLength = traceThroughLensElementsForFocalLength(&ld);

       // find by how much all lens elements should be scaled
       ld.focalLengthRatio = _focalLength / kolbFocalLength;

       // scale lens elements
       adjustFocalLength(&ld);

       // calculate focal length by tracing a parallel ray through the lens system (2nd time for new focallength)
       kolbFocalLength = traceThroughLensElementsForFocalLength(&ld);

       // user specified aperture radius from fstop
       ld.userApertureRadius = kolbFocalLength / (2.0 * _fStop);
       AiMsgInfo( "%-40s %12.8f", "[ZOIC] User aperture radius [cm]", ld.userApertureRadius);

       // clamp aperture if fstop is wider than max aperture given by lens description
       if (ld.userApertureRadius > ld.lensAperture[ld.apertureElement]){
           AiMsgWarning("[ZOIC] Given FSTOP wider than maximum aperture radius provided by lens data.");
           AiMsgWarning("[ZOIC] Clamping aperture radius from [%.9f] to [%.9f]", ld.userApertureRadius, ld.lensAperture[ld.apertureElement]);
           ld.userApertureRadius = ld.lensAperture[ld.apertureElement];
       }

       // calculate how much origin should be shifted so that the image distance at a certain object distance falls on the film plane
       ld.originShift = calculateImageDistance(_focalDistance, &ld, false);

       // find how far the aperture is from the film plane
       ld.apertureDistance = 0.0;
       for(int i = 0; i < (int)ld.lensRadiusCurvature.size(); i++){
           ld.apertureDistance += ld.lensThickness[i];
           if(i == ld.apertureElement){
               AiMsgInfo( "%-40s %12.8f", "[ZOIC] Aperture distance [cm]", ld.apertureDistance);
               break;
           }
       }

       // precompute lens centers
       computeLensCenters(&ld);

       ld.focalDistance = _focalDistance;

       // write to file for lens drawing
       writeToFile(&ld);
       myfile << "IMAGERAYS{";
       calculateImageDistance(ld.focalDistance, &ld, true);
       myfile << "}";
       myfile << "\n";
       myfile << "RAYS{";


       // search for ideal max height to shoot rays to on first lens element, by tracing test rays and seeing which one fails
       // maybe this varies based on where on the filmplane we are shooting the ray from? In this case this wouldn´t work..
       // and I don't think it does..
       // use lookup table instead!
       if(_kolbSamplingMethod == 1){
           int sampleCount = 1024;
           AtVector sampleOrigin = {0.0, 0.0, 0.0};
           sampleOrigin.z = ld.originShift;
           for (int i = 0; i < sampleCount; i++){
               float heightVariation = ld.lensAperture[0] / float(sampleCount);
               AtVector sampleDirection = {0.0, heightVariation * float(i), - float(ld.lensThickness[0])};

               if (!traceThroughLensElementsForApertureSize(sampleOrigin, sampleDirection, &ld)){
                   AiMsgInfo("[ZOIC] Positive failure at sample [%d] out of [%d]", i, sampleCount);
                   ld.optimalAperture = sampleDirection.y - heightVariation;
                   AiMsgInfo("[ZOIC] Optimal max height to shoot rays to on first lens element = [%.9f]", ld.optimalAperture);
                   break;
               }
           }
       }

       // lookup table method
       // Compute the ideal directions per origin point. Store these in a map of a map.
       // Then query these, and depending on the value inbetween do some kind of linear interpolation with the next up value.
       // otherwise it might be jaggy
   }

}


node_finish {
   cameraData *camera = (cameraData*) AiCameraGetLocalData(node);

   AiMsgInfo( "%-40s %12d", "[ZOIC] Succesful rays", ld.succesRays);
   AiMsgInfo( "%-40s %12d", "[ZOIC] Vignetted rays", ld.vignettedRays);
   AiMsgInfo( "%-40s %12.8f", "[ZOIC] Vignetted Percentage", (float(ld.vignettedRays) / (float(ld.succesRays) + float(ld.vignettedRays))) * 100.0);
   AiMsgInfo( "%-40s %12d", "[ZOIC] Total internal reflection cases", ld.totalInternalReflection);


   myfile << "}";
   myfile.close();

   // execute python drawing
   // std::string filename = "/Volumes/ZENO_2016/projects/zoic/src/draw.py";
   std::string filename = "C:/ilionData/Users/zeno.pelgrims/Documents/zoic_compile/draw.py";
   // std::string filename = "/Users/kassiawordley/Desktop/zoic_kassia/draw.py";
   std::string command = "python ";
   command += filename;
   system(command.c_str());

   AiMsgInfo("[ZOIC] Drawing finished");

   delete camera;
   AiCameraDestroy(node);

}



camera_create_ray {
   // get values
   const AtParamValue* params = AiNodeGetParams(node);
   cameraData *camera = (cameraData*) AiCameraGetLocalData(node);

   // tmp draw counters
   if (counter == 100000){
       draw = true;
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
               ConcentricSampleDiskImproved(input->lensx, input->lensy, &lensU, &lensV);

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
               float pointHypotenuse = std::sqrt((opticalVignetPoint.x * opticalVignetPoint.x) + (opticalVignetPoint.y * opticalVignetPoint.y));

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
                                    std::sqrt(input->sx * input->sx + input->sy * input->sy);
               }
           }
       }

       if (draw){
           myfile << std::fixed << std::setprecision(10) << output->origin.z;
           myfile << " ";
           myfile << std::fixed << std::setprecision(10) << output->origin.y;
           myfile << " ";
           myfile << std::fixed << std::setprecision(10) << output->dir.z * 10000.0;
           myfile << " ";
           myfile << std::fixed << std::setprecision(10) << output->dir.y * 10000.0;
           myfile << " ";
       }

       draw = false;

       // now looking down -Z
       output->dir.z *= -1.0;
   }


   if(_kolb){
       // arbitrary values but they are the same as the thin lens so.. dunno man
       output->origin.x = input->sx * 1.7;//(ld.filmDiagonal * 0.5);
       output->origin.y = input->sy * 1.7;//(ld.filmDiagonal * 0.5);
       output->origin.z = ld.originShift;
       //output->origin.z = -12.58; // working value for 100cm focus
       output->origin.x = 0.0;
       output->origin.y = 0.0;

       // sample disk with proper sample distribution
       float lensU, lensV = 0.0;
       if (_useImage == false){
           ConcentricSampleDiskImproved(input->lensx, input->lensy, &lensU, &lensV);
       } else { // sample bokeh image
           camera->image.bokehSample(input->lensx, input->lensy, &lensU, &lensV);
       }

       // pick between different sampling methods (change to enum)
       // sampling first element is "ground truth" but wastes a lot of rays
       // sampling optimal aperture is efficient, but might not make a whole image
       if (_kolbSamplingMethod == 0){ // using noisy ground truth
           output->dir.x = lensU * ld.lensAperture[0];
           output->dir.y = lensV * ld.lensAperture[0];
           output->dir.z = -ld.lensThickness[0];
       } else if (_kolbSamplingMethod == 1){ // using binary aperture search
           output->dir.x = lensU * ld.optimalAperture;
           output->dir.y = lensV * ld.optimalAperture;
           output->dir.z = -ld.lensThickness[0];
       }

       output->dir.x = 0.0; //tmp for nicer 2d drawing

       if(!traceThroughLensElements(&output->origin, &output->dir, &ld, draw)){
           ++ld.vignettedRays;
           output->weight = 0.0;
       } else {
           ++ld.succesRays;
       }

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

   ++counter;

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
