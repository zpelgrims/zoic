/* ARNOLD API CAMERA STUFF

 INPUTS

 * sx, sy (screen-space coordinates - within the screen window)
 * dsx, dsy (derivatives of the screen-space coordinates with respect to pixel coordinates)
 * lensx, lensy (lens sampling coordinates in [0,1)^2
 * relative_time (time relative to this camera (in [0,1))

 OUTPUTS

 * origin (ray origin in camera space (required))
 * dir (ray direction in camera space (required))
 * dOdx, dOdy (derivative of the ray origin with respect to the pixel coordinates (optional - defaults to 0))
 * dDdx, dDdy (derivative of the ray direction with respect to the pixel coordinates (optional - defaults to 0))
 * weight (weight of this ray (used for vignetting) (optional - defaults to 1))

*/

/*

    Shutter speed should affect motion blur and should read scene fps

    24 fps
    1/50th speed
    ideal -> 0.5 frames before and 0.5 frames after

*/

/*
    FILTER MAP:

    From arnold website: "Weights the camera sample by a scalar amount defined by the shader linked to the filtermap.
    This shader will use as an input the u,v coordinates in image-space coords [0,1) and x,y in pixel coordinates.
    This allows you to darken certain regions of the image, perfect to simulate vignetting effects.
    There is an optimization in place where if the filter returns pure black then the camera ray is not fired.
    This can help in cases such as when rendering with the fisheye camera where, depending on its autocrop setting, parts of the frame trace no rays at all."

*/

/*

    Physically based bloom: http://www.cs.utah.edu/~shirley/papers/spencer95.pdf

    From Mitsuba: "This fast convolution method used to implement Spencer et al’s physically-based bloom filter in the mtsutil tonemap utility.
    This can be useful when rendering images where pixels are clipped because they are so bright.
    Take for instance the rendering below: there are many reflections of the sun, but they are quite hard to perceive due to the limited dynamic range.
    After convolving the image with an empirical point spread function of the human eye, their brightness is much more apparent."

    Not sure if this is done as a post process or not. Figure out.

*/

/*

    SAMPLING IDEA

    Would be cool to have a function to reduce the diff/spec/etc samples in out of focus areas.
    Not sure how to tackle this and not sure if this is possible withing the range of a camera shader.

*/

/*

    SAMPLING IDEA

    To get hard edged bokeh shapes with high aperture sizes, maybe do a prepass (render image with low sampling and no dof) and then use that prepass in the same way as the bokeh sampling?
    More samples for the highlights makes sense if you're defocusing.
    This should be done by picking ray directions that will hit highlight areas of the image more often.

    I could also make the highlights brighter (and therefore the bokeh shapes more apparent) by adding to the weight of these rays.

*/

#include <ai.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <functional>
#include <random>
#include <cstring>
#include <OpenImageIO/imageio.h>

AI_CAMERA_NODE_EXPORT_METHODS(zenoCameraMethods)

bool debug = false;

#define _sensorWidth  (params[0].FLT)
#define _sensorHeight  (params[1].FLT)
#define _focalLength  (params[2].FLT)
#define _useDof  (params[3].BOOL)
#define _fStop  (params[4].FLT)
#define _focalDistance  (params[5].FLT)
#define _opticalVignetting  (params[6].FLT)
#define _useImage  (params[7].BOOL)
#define _bokehPath (params[8].STR)

struct imageData{
     int x, y;
     int nchannels;
     std::vector<uint8_t> pixelData;
     float* cdfRow;
     float* cdfColumn;
     float* summedRowValues;
     float* normalizedValuesPerRow;
     std::vector<int> rowIndices;
     std::vector<int> columnIndices;
};

imageData *image = nullptr;


// modified PBRT v2 source code to sample circle in a more uniform way
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



imageData* readImage(char const *bokeh_kernel_filename){

    imageData* img = new imageData;

    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    std::cerr << "Reading image <" << bokeh_kernel_filename << "> with OpenImageIO" << std::endl;

    /* Search for an ImageIO plugin that is capable of reading the file ("foo.jpg"), first by
    trying to deduce the correct plugin from the file extension, but if that fails, by opening
    every ImageIO plugin it can find until one will open the file without error. When it finds
    the right plugin, it creates a subclass instance of ImageInput that reads the right kind of
    file format, and tries to fully open the file. */
    OpenImageIO::ImageInput *in = OpenImageIO::ImageInput::open (bokeh_kernel_filename);
    if (! in){
        return nullptr; // Return a null pointer if we have issues
    }

    const OpenImageIO::ImageSpec &spec = in->spec();
    img->x = spec.width;
    img->y = spec.height;
    img->nchannels = spec.nchannels;

    img->pixelData.reserve(img->x * img->y * img->nchannels);
    in->read_image (OpenImageIO::TypeDesc::UINT8, &img->pixelData[0]);
    in->close ();
    delete in;

    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    std::cerr << "Image Width: " << img->x << std::endl;
    std::cerr << "Image Height: " << img->y << std::endl;
    std::cerr << "Image Channels: " << img->nchannels << std::endl;
    std::cout << "Total amount of pixels: " << img->x * img->y << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;

    if (debug == true){
        // print out raw pixel data
        for (int i = 0; i < img->x * img->y * img->nchannels; i++){
            int j = 0;
            if(img->nchannels == 3){
               if (j == 0){
                   std::cout << "[";
                    std::cout << (int)img->pixelData[i];
                   std::cout << ", ";
                    j += 1;
                }
                if (j == 1){
                    std::cout << (int)img->pixelData[i];
                    std::cout << ", ";
                    j += 1;
                }
               if (j == 2){
                    std::cout << (int)img->pixelData[i];
                    std::cout << "], ";
                    j = 0;
                }
            }

            else if(img->nchannels == 4){
                if (j == 0){
                    std::cout << "[";
                    std::cout << (int)img->pixelData[i];
                    std::cout << ", ";
                    j += 1;
                }
                if (j == 1){
                    std::cout << (int)img->pixelData[i];
                    std::cout << ", ";
                    j += 1;
                }
                if (j == 2){
                    std::cout <<  (int)img->pixelData[i];
                    std::cout << ", ";
                    j += 1;
                }
                if (j == 3){
                    std::cout << (int)img->pixelData[i];
                    std::cout << "], ";
                   j = 0;
                }
            }

        }

        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
    }

    return img;
}

void bokehProbability(imageData *img){

    if(img){

        // initialize arrays
        float pixelValues[img->x * img->y];
        float normalizedPixelValues[img->x * img->y];

        // for every pixel, stuff going wrong here
        int tmpPixelCounter = 0;
        for(int i=0; i < img->x * img->y; ++i){
            // store pixel value in array
            // calculate luminance [Y = 0.3 R + 0.59 G + 0.11 B]
            pixelValues[i] = (img->pixelData[tmpPixelCounter] * 0.3) + (img->pixelData[tmpPixelCounter+1] * 0.59) + (img->pixelData[tmpPixelCounter+2] * 0.11f);

            if (debug == true){
                // print array
                std::cout << "Pixel Luminance: " << i << " -> " << pixelValues[i] << std::endl;
            }

            if(img->nchannels == 3){
                tmpPixelCounter += 3;
            }
            else if(img->nchannels == 4){
                tmpPixelCounter += 4;
            }
        }

        // calculate sum of all pixel values
        float totalValue = 0.0f;
        for(int i=0; i < img->x *  img->y; ++i){
            totalValue += pixelValues[i];
        }

        if (debug == true){
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "DEBUG: Total Pixel Value: " << totalValue << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        }



        // normalize pixel values so sum = 1
        for(int i=0; i < img->x *  img->y; ++i){
            normalizedPixelValues[i] = pixelValues[i] / totalValue;

            if (debug == true){
                // print array
                std::cout << "Normalized Pixel Value: " << i << ": " << normalizedPixelValues[i] << std::endl;
            }
        }



        // calculate sum of all normalized pixel values, to check
        float totalNormalizedValue = 0.0f;
        for(int i=0; i < img->x *  img->y; ++i){
            totalNormalizedValue += normalizedPixelValues[i];
        }

        if (debug == true){
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "DEBUG: Total Normalized Pixel Value: " << totalNormalizedValue << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        }



        // calculate sum for each row
        img->summedRowValues = new float [img->y]();
        float summedHorizontalNormalizedValues;
        int counterRow = 0;

        for(int i=0; i < img->y; ++i){

            summedHorizontalNormalizedValues = 0.0f;

            for(int j=0; j < img->x; ++j){

                summedHorizontalNormalizedValues += normalizedPixelValues[counterRow];
                counterRow += 1;
            }

            img->summedRowValues[i] = summedHorizontalNormalizedValues;
            if (debug == true){
                std::cout << "Summed Values row [" << i << "]: " << img->summedRowValues[i] << std::endl;
            }
        }



        // calculate sum of all row values, just to debug
        float totalNormalizedRowValue = 0.0f;
        for(int i=0; i < img->y; ++i){
            totalNormalizedRowValue += img->summedRowValues[i];
        }

        if (debug == true){
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "Debug: Summed Row Value: " << totalNormalizedRowValue << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        }


        // sort row values from highest to lowest (probability density function)
        // needed to make a copy of array, can't use the one in struct for some reason?
        float summedRowValueCopy[img->y];
        for(int i = 0; i < img->y; i++){
            summedRowValueCopy[i] = img->summedRowValues[i];
        }

        // make array of indices
        size_t summedRowValueCopyIndices[img->y];
        for(int i = 0; i < img->y; i++){
            summedRowValueCopyIndices[i] = i;
        }

        std::sort(summedRowValueCopyIndices, summedRowValueCopyIndices + img->y, [&summedRowValueCopy]( size_t _lhs,  size_t _rhs){
            return summedRowValueCopy[_lhs] > summedRowValueCopy[_rhs];
        });

        if (debug == true){
            // print values
            for(int i = 0; i < img->y; ++i){
                std::cout << "PDF row [" <<  summedRowValueCopyIndices[i] << "]: " << summedRowValueCopy[summedRowValueCopyIndices[i]] << std::endl;
            }

            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        }


        // For every row, add the sum of all previous row (cumulative distribution function)
        img->cdfRow = new float [img->y]();
        img->rowIndices.reserve(img->y);

        for (int i = 0; i < img->y; ++i){

            if(i == 0){
                img->cdfRow[i] = img->cdfRow[i] + summedRowValueCopy[summedRowValueCopyIndices[i]];
            }
            else{
                img->cdfRow[i] = img->cdfRow[i-1] + summedRowValueCopy[summedRowValueCopyIndices[i]];
            }

            img->rowIndices[i] = summedRowValueCopyIndices[i];

            if (debug == true){
                std::cout << "CDF row [" << img->rowIndices[i] << "]: " << img->cdfRow[i] << std::endl;
            }
        }

        if (debug == true){
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        }



        // divide pixel values of each pixel by the sum of the pixel values of that row (Normalize)
        int rowCounter = 0;
        int tmpCounter = 0;
        img->normalizedValuesPerRow = new float [img->x * img->y]();

        for (int i = 0; i < img->x * img->y; ++i){

            // avoid division by 0
            if ((normalizedPixelValues[i] != 0) && (img->summedRowValues[rowCounter] != 0)){
                img->normalizedValuesPerRow[i] = normalizedPixelValues[i] / img->summedRowValues[rowCounter];
            }
            else{
                img->normalizedValuesPerRow[i] = 0;
            }

            tmpCounter += 1;

            // silly counter, there must be faster ways to do this but i'm not exactly a genius
            if (tmpCounter == img->x){
                rowCounter += 1;
                tmpCounter = 0;
            }

            if (debug == true){
                std::cout << "Normalized Pixel value per row: " << i << ": " << img->normalizedValuesPerRow[i] << std::endl;
            }
        }

        if (debug == true){
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        }



        // sort column values from highest to lowest per row (probability density function)
        // needed to make a copy of array, can't use the one in struct for some reason?
        float summedColumnValueCopy[img->x * img->y];
        for(int i = 0; i < img->x * img->y; ++i){
            summedColumnValueCopy[i] = img->normalizedValuesPerRow[i];
        }

        // make array of indices
        size_t summedColumnValueCopyIndices[img->x * img->y];
        for(int i = 0; i < img->x * img->y; i++){
            summedColumnValueCopyIndices[i] = i;
        }
        for (int i = 0; i < img->x * img->y; i+=img->x){
            std::sort(summedColumnValueCopyIndices + i, summedColumnValueCopyIndices + i + img->x, [&summedColumnValueCopy]( size_t _lhs,  size_t _rhs){
                return summedColumnValueCopy[_lhs] > summedColumnValueCopy[_rhs];
            });
        }

        if (debug == true){
            // print values
            for(int i = 0; i < img->x * img->y; ++i){
                std::cout << "PDF column [" << summedColumnValueCopyIndices[i] << "]: " << summedColumnValueCopy[summedColumnValueCopyIndices[i]] << std::endl;
            }
            std::cout << "----------------------------------------------" << std::endl;
            std::cout << "----------------------------------------------" << std::endl;
        }


        // For every column per row, add the sum of all previous columns (cumulative distribution function)
        img->cdfColumn = new float [img->x * img->y]();
        img->columnIndices.reserve(img->x * img->y);
        int cdfCounter = 0;

        for (int i = 0; i < img->x * img->y; ++i){
            if (cdfCounter == img->x) {
                    img->cdfColumn[i] = summedColumnValueCopy[summedColumnValueCopyIndices[i]];
                    cdfCounter = 0;
            }
            else {
                img->cdfColumn[i] = img->cdfColumn[i-1] + summedColumnValueCopy[summedColumnValueCopyIndices[i]];
            }

            cdfCounter += 1;

            img->columnIndices[i] = summedColumnValueCopyIndices[i];

            if (debug == true){
                std::cout << "CDF column [" <<  img->columnIndices[i] << "]: " << img->cdfColumn[i] << std::endl;
            }
         }

        if (debug == true){
            std::cout << "----------------------------------------------" << std::endl;
        }
    }
}

void bokehSample(imageData *img, float randomNumberRow, float randomNumberColumn, float *dx, float *dy){

    if (debug == true){
        // print random number between 0 and 1
        std::cout << "RANDOM NUMBER ROW: " << randomNumberRow << std::endl;
    }

    // find upper bound of random number in the array
    float *pUpperBound = std::upper_bound(img->cdfRow, img->cdfRow + img->y, randomNumberRow);
    if (debug == true){
        std::cout << "UPPER BOUND: " << *pUpperBound << std::endl;
    }

    // find which element of the array the upper bound is
    int x = std::distance(img->cdfRow, std::find(img->cdfRow, img->cdfRow + img->y, *pUpperBound));

    // find actual pixel row
    int actualPixelRow = img->rowIndices[x];

    // recalculate pixel row so that the center pixel is (0,0) - might run into problems with images of dimensions like 2x2, 4x4, 6x6, etc
    int recalulatedPixelRow = actualPixelRow - ((img->x - 1) / 2);

    if (debug == true){
        // print values
        std::cout << "INDEX IN CDF ROW: " << x << std::endl;
        std::cout << "ACTUAL PIXEL ROW: " << actualPixelRow << std::endl;
        std::cout << "RECALCULATED PIXEL ROW: " << recalulatedPixelRow << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;


        // print random number between 0 and 1
        std::cout << "RANDOM NUMBER COLUMN: " << randomNumberColumn << std::endl;
    }

    int startPixel = actualPixelRow * img->x;
    if (debug == true){
        std::cout << "START PIXEL: " << startPixel << std::endl;
    }


    // find upper bound of random number in the array
    float *pUpperBoundColumn = std::upper_bound(img->cdfColumn + startPixel, img->cdfColumn + startPixel + img->x, randomNumberColumn);
    if (debug == true){
        std::cout << "UPPER BOUND: " << *pUpperBoundColumn << std::endl;
    }

    // find which element of the array the upper bound is
    int y = std::distance(img->cdfColumn, std::find(img->cdfColumn + startPixel, img->cdfColumn + startPixel + img->x, *pUpperBoundColumn));

    // find actual pixel column
    int actualPixelColumn = img->columnIndices[y];
    int relativePixelColumn = actualPixelColumn - startPixel;
    int recalulatedPixelColumn = relativePixelColumn - ((img->y - 1) / 2);

    if (debug == true){
        // print values
        std::cout << "INDEX IN CDF COLUMN: " << y << std::endl;
        std::cout << "ACTUAL PIXEL COLUMN: " << actualPixelColumn << std::endl;
        std::cout << "RELATIVE PIXEL COLUMN (starting from 0): " << relativePixelColumn << std::endl;
        std::cout << "RECALCULATED PIXEL COLUMN: " << recalulatedPixelColumn << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
    }

    // to get the right image orientation, flip the x and y coordinates and then multiply the y values by -1 to flip the pixels vertically
    float flippedRow = recalulatedPixelColumn * -1.0f;
    float flippedColumn = recalulatedPixelRow;



    // send values back
    *dx = (float)flippedRow / (float)img->x;
    *dy = (float)flippedColumn / (float)img->y;

}


node_parameters {
   AiParameterFLT("sensorWidth", 3.6f); // 35mm film
   AiParameterFLT("sensorHeight", 2.4f); // 35 mm film
   AiParameterFLT("focalLength", 8.0f); // distance between sensor and lens
   AiParameterBOOL("useDof", true);
   AiParameterFLT("fStop", 0.4f);
   AiParameterFLT("focalDistance", 110.0f); // distance from lens to focal point
   AiParameterFLT("opticalVignetting", 0.0f); //distance of the opticalVignetting virtual aperture
   AiParameterBOOL("useImage", false);
   AiParameterStr("bokehPath", ""); //bokeh shape image location
}


node_initialize {
   AiCameraInitialize(node, NULL);

}

node_update {
   AiCameraUpdate(node, false);

   bool useImage = _useImage;
   const char bokehPath = _bokehPath;

   std::cout << bokehPath << std::endl;

   if (useImage == true){
  //make sure to change the string back to the variable!
       image = readImage("bokehPath");

       // Check if image is valid (is the pointer null?)
       if(!image){
            std::cout << "Couldn't open image, please try again\n";
            exit(1);
       }

       bokehProbability(image);
      }
}

node_finish {
    // get values
    const AtParamValue* params = AiNodeGetParams(node);

    // send statements to output log
    //    AiMsgWarning("-------DEPTH OF FIELD---------");
    //    AiMsgWarning("useDof = %s", (_useDof?"True":"False"));
    //    AiMsgWarning("focusDistance = %f", _focalDistance);
    //    AiMsgWarning("fStop = %f", _fStop);
    //    AiMsgWarning("------------------------------");

    AiCameraDestroy(node);
}


camera_create_ray {

    // get values
    const AtParamValue* params = AiNodeGetParams(node);

    // variables
    float sensorWidth = _sensorWidth; // 35mm film
    float sensorHeight = _sensorHeight; // 35 mm film
    float focalLength = _focalLength; // distance between sensor and lens
    float fStop = _fStop;
    float focalDistance = _focalDistance; // distance from lens to focal point
    bool useDof = _useDof;
    float opticalVignetting = _opticalVignetting; //distance of the opticalVignetting virtual aperture
    bool useImage = _useImage;

    // calculate diagonal length of sensor
    float sensorDiagonal = sqrtf((sensorWidth * sensorWidth) + (sensorHeight * sensorHeight));

    // calculate field of view (theta = 2arctan*(sensorSize/focalLength))
    float fov = 2.0f * atan((sensorDiagonal / (2.0f * focalLength))); // in radians
    fov = fov * AI_RTOD; // in degrees
    float tan_fov = tanf((fov * AI_DTOR) / 2);

    // calculate aperture radius (apertureRadius = focalLength / 2*fStop)
    float apertureRadius = focalLength / (2*fStop);

    AtPoint p;
    p.x = input->sx * tan_fov;
    p.y = input->sy * tan_fov;
    p.z = 1;

    output->dir = AiV3Normalize(p - output->origin);

    // now looking down -Z
    output->dir.z *= -1;

    // DOF CALCULATIONS
    if (useDof == true) {
        // Initialize point on lens
        float lensU = 0.0f;
        float lensV = 0.0f;

        // sample disk with proper sample distribution, lensU & lensV (positions on lens) are updated.
        if (useImage == false){
            ConcentricSampleDisk(input->lensx, input->lensy, &lensU, &lensV);
        }
        else{
            // sample bokeh image
            bokehSample(image, input->lensx, input->lensy, &lensU, &lensV);
        }

        // this creates a square bokeh!
        // lensU = input->lensx * apertureRadius;
        // lensV = input->lensy * apertureRadius;

        // scale new lens coordinates by the aperture radius
        lensU = lensU * apertureRadius;
        lensV = lensV * apertureRadius;

        // Compute point on plane of focus, intersection on z axis
        float intersection = std::abs(focalDistance / output->dir.z);
        AtPoint focusPoint = output->dir * intersection;

        // update arnold ray origin
        output->origin.x = lensU;
        output->origin.y = lensV;
        output->origin.z = 0.0;

        // update arnold ray direction, normalize
        output->dir = AiV3Normalize(focusPoint - output->origin);

        // TODO: make a standard for this optical vignetting thing, not just random values
        // TODO: send extra samples to edges of the image (based on gradient) // not sure if this possible since sx, sy are read only variables.
        // TODO: something wrong when I change focal length to high number, losing a lot of samples for some reason
        //            even losing samples in the middle..
        // Optical Vignetting (CAT EYE EFFECT)
        if (opticalVignetting > 0.0f){
            float opticalVignetDistance = opticalVignetting;

            // because the first intersection point of the aperture is already known, I can just linearly scale it by the distance to the second aperture
            AtPoint opticalVignetPoint;
            opticalVignetPoint = output->dir * opticalVignetDistance;

            // re-center point
            opticalVignetPoint -= output->origin;

            // find hypotenuse of x, y points.
            float pointHypotenuse = sqrt((opticalVignetPoint.x * opticalVignetPoint.x) + (opticalVignetPoint.y * opticalVignetPoint.y));

            float opticalVignetApertureRadius = apertureRadius * 1.0f;

            // if intersection point on the optical vignetting virtual aperture is within the radius of the aperture from the plane origin, kill ray
            if (ABS(pointHypotenuse) > opticalVignetApertureRadius){
                // set ray weight to 0, there is an optimisation inside Arnold that doesn't send rays if they will return black anyway.
                output->weight = 0.0f;
            }
        }
    }


    // vignetting
    // float dist2 = input->sx * input->sx + input->sy * input->sy;
    // output->weight = 1.0f - .5*dist2;


    // not sure if needed, but can't hurt. Taken from solidangle website.
    // ----------------------------------------------------------------------------------------------
    // scale derivatives
    float dsx = input->dsx * tan_fov;
    float dsy = input->dsy * tan_fov;

    AtVector d = p;  // direction vector == point on the image plane
    double d_dot_d = AiV3Dot(d, d);
    double temp = 1.0 / sqrt(d_dot_d * d_dot_d * d_dot_d);

    // already initialized to 0's, only compute the non zero coordinates
    output->dDdx.x = (d_dot_d * dsx - (d.x * dsx) * d.x) * temp;
    output->dDdx.y = (              - (d.x * dsx) * d.y) * temp;
    output->dDdx.z = (              - (d.x * dsx) * d.z) * temp;
    output->dDdy.x = (              - (d.y * dsy) * d.x) * temp;
    output->dDdy.y = (d_dot_d * dsy - (d.y * dsy) * d.y) * temp;
    output->dDdy.z = (              - (d.y * dsy) * d.z) * temp;
    // ----------------------------------------------------------------------------------------------

}


node_loader {
   if (i > 0)
      return false;
   node->methods      = zenoCameraMethods;
   node->output_type  = AI_TYPE_NONE;
   node->name         = "zenoCamera";
   node->node_type    = AI_NODE_CAMERA;
   strcpy(node->version, AI_VERSION);
   return true;
}

// http://www.scratchapixel.com/old/lessons/2d-image-processing/reading-and-writing-images-a-simple-image-class/reading-and-writing-images-a-simple-image-class/
// http://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/monte-carlo-methods-mathematical-foundations/inverse-transform-sampling-method
// http://noobody.org/is-report/simple.html
