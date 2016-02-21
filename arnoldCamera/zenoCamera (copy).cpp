// Arnold camera shader with options for image based bokeh shapes and optical vignetting.

// Special thanks to Marc-Antoine Desjardins for the help on the image sampling
// Special thanks to Benedikt Bitterli for the information on optical vignetting
// Special thanks to Tom Minor for the help with C++ (it was needed!)

// (C) Zeno Pelgrims, www.zenopelgrims.com


// IDEAS

/* Shutter speed should affect motion blur and should read scene fps

    24 fps
    1/50th speed
    ideal -> 0.5 frames before and 0.5 frames after

*/

/* FILTER MAP:

    From arnold website: "Weights the camera sample by a scalar amount defined by the shader linked to the filtermap.
    This shader will use as an input the u,v coordinates in image-space coords [0,1) and x,y in pixel coordinates.
    This allows you to darken certain regions of the image, perfect to simulate vignetting effects.
    There is an optimization in place where if the filter returns pure black then the camera ray is not fired.
    This can help in cases such as when rendering with the fisheye camera where, depending on its autocrop setting, parts of the frame trace no rays at all."

*/

/* Physically based bloom: http://www.cs.utah.edu/~shirley/papers/spencer95.pdf

    From Mitsuba: "This fast convolution method used to implement Spencer et al’s physically-based bloom filter in the mtsutil tonemap utility.
    This can be useful when rendering images where pixels are clipped because they are so bright.
    Take for instance the rendering below: there are many reflections of the sun, but they are quite hard to perceive due to the limited dynamic range.
    After convolving the image with an empirical point spread function of the human eye, their brightness is much more apparent."

    Not sure if this is done as a post process or not. Figure out.

*/

/* SAMPLING IDEA

    Would be cool to have a function to reduce the diff/spec/etc samples in out of focus areas.
    Not sure how to tackle this and not sure if this is possible withing the range of a camera shader.

*/

/* SAMPLING IDEA

    To get hard edged bokeh shapes with high aperture sizes, maybe do a prepass (render image with low sampling and no dof) and then use that prepass in the same way as the bokeh sampling?
    More samples for the highlights makes sense if you're defocusing.
    This should be done by picking ray directions that will hit highlight areas of the image more often.

    I could also make the highlights brighter (and therefore the bokeh shapes more apparent) by adding to the weight of these rays.

*/

/* CATEYE IDEA

    Cateye effect gives the inner edges of the spheres a brighter rim, so give more weight to the samples on the opposite corner of the screen.
    I think I can easily do this by scaling the weight of the samples based on the relationship between the screen space coordinates.

*/

/* TODO

Need to completely clean up after myself, otherwise I get seg faults when inputting new images etc

Make a standard for this optical vignetting thing, not just random values

Send extra samples to edges of the image (based on gradient) // not sure if this possible since sx, sy are read only variables.

Something wrong when I change focal length to high number, losing a lot of samples for some reason
even losing samples in the middle..

sometimes there's no defocussing in the y axis, probably because of an initialization to 0 i set.

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
     std::vector<float> cdfRow;
     std::vector<float> cdfColumn;
     std::vector<float> summedRowValues;
     std::vector<float> normalizedValuesPerRow;
     std::vector<int> rowIndices;
     std::vector<int> columnIndices;
};

struct cameraData{
    float fov;
    float tan_fov;
    float apertureRadius;
};

imageData *image = nullptr;
cameraData camera;


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

// Read bokeh image
imageData* readImage(char const *bokeh_kernel_filename){

    imageData* img = new imageData;

    AiMsgInfo("Reading image using OpenImageIO: %s", bokeh_kernel_filename);

    //Search for an ImageIO plugin that is capable of reading the file ("foo.jpg"), first by
    //trying to deduce the correct plugin from the file extension, but if that fails, by opening
    //every ImageIO plugin it can find until one will open the file without error. When it finds
    //the right plugin, it creates a subclass instance of ImageInput that reads the right kind of
    //file format, and tries to fully open the file.
    OpenImageIO::ImageInput *in = OpenImageIO::ImageInput::open (bokeh_kernel_filename);
    if (! in){
        return nullptr; // Return a null pointer if we have issues
    }

    const OpenImageIO::ImageSpec &spec = in->spec();
    img->x = spec.width;
    img->y = spec.height;
    img->nchannels = spec.nchannels;

    img->pixelData.clear();
    img->pixelData.reserve(img->x * img->y * img->nchannels);
    in->read_image (OpenImageIO::TypeDesc::UINT8, &img->pixelData[0]);
    in->close ();
    delete in;

    AiMsgInfo("Image Width: %d", img->x);
    AiMsgInfo("Image Height: %d", img->y);
    AiMsgInfo("Image Channels: %d", img->nchannels);
    AiMsgInfo("Total amount of pixels to process: %d", img->x * img->y);

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

// Importance sampling
void bokehProbability(imageData *img){
    if(img){

        // initialize arrays
        std::vector<float> pixelValues(img->x * img->y, 0.0f);
        std::vector<float> normalizedPixelValues(img->x * img->y, 0.0f);

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
            else if(img->nchannels == 1){
                tmpPixelCounter += 1;
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
        img->summedRowValues.clear();
        img->summedRowValues.reserve(img->y);
        float summedHorizontalNormalizedValues = 0.0f;
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
        std::vector<float> summedRowValueCopy(img->y, 0.0f);
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
        // not 100% sure if clears are needed, but doing it anyway
        img->cdfRow.clear();
        img->rowIndices.clear();
        img->cdfRow.reserve(img->y);
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
        img->normalizedValuesPerRow.clear();
        img->normalizedValuesPerRow.reserve(img->x * img->y);

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
        // float summedColumnValueCopy[img->x * img->y];
        std::vector<float> summedColumnValueCopy(img->x * img->y, 0.0f);
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
        img->cdfColumn.clear();
        img->columnIndices.clear();
        img->cdfColumn.reserve(img->x * img->y);
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

// Sample image
void bokehSample(imageData *img, float randomNumberRow, float randomNumberColumn, float *dx, float *dy){

    if (debug == true){
        // print random number between 0 and 1
        AiMsgInfo("RANDOM NUMBER ROW:  %f", randomNumberRow);
        // std::cout << "RANDOM NUMBER ROW: " << randomNumberRow << std::endl;
    }

    // find upper bound of random number in the array
    float pUpperBound = *std::upper_bound(img->cdfRow.begin(), img->cdfRow.end(), randomNumberRow);
    if (debug == true){
        AiMsgInfo("UPPER BOUND:  %f", pUpperBound);
    }

    // find which element of the array the upper bound is
    int x = std::distance(img->cdfRow.begin(), std::find(img->cdfRow.begin(), img->cdfRow.end(), pUpperBound));

    // find actual pixel row
    int actualPixelRow = img->rowIndices[x];

    // recalculate pixel row so that the center pixel is (0,0) - might run into problems with images of dimensions like 2x2, 4x4, 6x6, etc
    int recalulatedPixelRow = actualPixelRow - ((img->x - 1) / 2);

    if (debug == true){
        // print values
        AiMsgInfo("INDEX IN CDF ROW:  %d", x);
        AiMsgInfo("ACTUAL PIXEL ROW:  %d", actualPixelRow);
        AiMsgInfo("RECALCULATED PIXEL ROW:  %d", recalulatedPixelRow);
        AiMsgInfo("----------------------------------------------  %s");
        AiMsgInfo("----------------------------------------------  %s");

        // print random number between 0 and 1
        AiMsgInfo("RANDOM NUMBER COLUMN:  %f", randomNumberColumn);
    }

    int startPixel = actualPixelRow * img->x;
    if (debug == true){
        AiMsgInfo("START PIXEL: %d", startPixel);
    }


    // find upper bound of random number in the array
    float pUpperBoundColumn = *std::upper_bound(img->cdfColumn.begin() + startPixel, img->cdfColumn.begin() + startPixel + img->x, randomNumberColumn);
    if (debug == true){
        AiMsgInfo("UPPER BOUND: %f", pUpperBoundColumn);
    }

    // find which element of the array the upper bound is
    int y = std::distance(img->cdfColumn.begin(), std::find(img->cdfColumn.begin() + startPixel, img->cdfColumn.begin() + startPixel + img->x, pUpperBoundColumn));

    // find actual pixel column
    int actualPixelColumn = img->columnIndices[y];
    int relativePixelColumn = actualPixelColumn - startPixel;
    int recalulatedPixelColumn = relativePixelColumn - ((img->y - 1) / 2);

    if (debug == true){
        // print values
        AiMsgInfo("INDEX IN CDF COLUMN:  %d", y);
        AiMsgInfo("ACTUAL PIXEL COLUMN:  %d", actualPixelColumn);
        AiMsgInfo("RELATIVE PIXEL COLUMN (starting from 0):  %d", relativePixelColumn);
        AiMsgInfo("RECALCULATED PIXEL COLUMN:  %d", recalulatedPixelColumn);
        AiMsgInfo("----------------------------------------------  %s");
        AiMsgInfo("----------------------------------------------  %s");
    }

    // to get the right image orientation, flip the x and y coordinates and then multiply the y values by -1 to flip the pixels vertically
    float flippedRow = recalulatedPixelColumn;
    float flippedColumn = recalulatedPixelRow * -1.0f;

    // send values back
    *dx = (float)flippedRow / (float)img->x * 2.0f;
    *dy = (float)flippedColumn / (float)img->y * 2.0f;
}



node_parameters {
   AiParameterFLT("sensorWidth", 3.6f); // 35mm film
   AiParameterFLT("sensorHeight", 2.4f); // 35 mm film
   AiParameterFLT("focalLength", 65.0f); // distance between sensor and lens
   AiParameterBOOL("useDof", true);
   AiParameterFLT("fStop", 1.4f);
   AiParameterFLT("focalDistance", 110.0f); // distance from lens to focal point
   AiParameterFLT("opticalVignetting", 0.0f); //distance of the opticalVignetting virtual aperture
   AiParameterBOOL("useImage", true);
   AiParameterStr("bokehPath", "/home/i7210038/qt_arnoldCamera/arnoldCamera/imgs/collected_real/bokeh_18_graded.jpg"); //bokeh shape image location
}


node_initialize {
   AiCameraInitialize(node, NULL);

}

node_update {
   AiCameraUpdate(node, false);

   // calculate field of view (theta = 2arctan*(sensorSize/focalLength))
   camera.fov = 2.0f * atan((_sensorWidth / (2.0f * (_focalLength/10)))); // in radians
   camera.tan_fov = tanf(camera.fov/ 2);

   // calculate aperture radius (apertureRadius = focalLength / 2*fStop)
   camera.apertureRadius = (_focalLength/10) / (2*_fStop);

   // make probability functions of the bokeh image
   if (_useImage == true){
       image = readImage(_bokehPath);
       if(!image){
            AiMsgError("Couldn't open image, please check that it is RGB/RGBA.");
            exit(1);
       }

       bokehProbability(image);
      }

}

node_finish {
    AiCameraDestroy(node);
}


camera_create_ray {
    // get values
    const AtParamValue* params = AiNodeGetParams(node);

    AtPoint p;
    p.x = input->sx * camera.tan_fov;
    p.y = input->sy * camera.tan_fov;
    p.z = 1;

    output->dir = AiV3Normalize(p - output->origin);

    // now looking down -Z
    output->dir.z *= -1;

    // DOF CALCULATIONS
    if (_useDof == true) {
        // Initialize point on lens
        float lensU = 0.0f;
        float lensV = 0.0f;

        // sample disk with proper sample distribution, lensU & lensV (positions on lens) are updated.
        if (_useImage == false){
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
        lensU = lensU * camera.apertureRadius;
        lensV = lensV * camera.apertureRadius;

        // Compute point on plane of focus, intersection on z axis
        float intersection = std::abs(_focalDistance / output->dir.z);
        AtPoint focusPoint = output->dir * intersection;

        // update arnold ray origin
        output->origin.x = lensU;
        output->origin.y = lensV;
        output->origin.z = 0.0;

        // update arnold ray direction, normalize
        output->dir = AiV3Normalize(focusPoint - output->origin);

        // Optical Vignetting (CAT EYE EFFECT)
        if (_opticalVignetting > 0.0f){
            // because the first intersection point of the aperture is already known, I can just linearly scale it by the distance to the second aperture
            AtPoint opticalVignetPoint;
            opticalVignetPoint = output->dir * _opticalVignetting;

            // re-center point
            opticalVignetPoint -= output->origin;

            // find hypotenuse of x, y points.
            float pointHypotenuse = sqrt((opticalVignetPoint.x * opticalVignetPoint.x) + (opticalVignetPoint.y * opticalVignetPoint.y));

            float opticalVignetApertureRadius = camera.apertureRadius * 1.0f;

            // if intersection point on the optical vignetting virtual aperture is within the radius of the aperture from the plane origin, kill ray
            if (ABS(pointHypotenuse) > opticalVignetApertureRadius){
                // set ray weight to 0, there is an optimisation inside Arnold that doesn't send rays if they will return black anyway.
                output->weight = 0.0f;
            }

            // testing cateye technique, adding weight to opposite edges to get nice rim on the highlights, this will need to be more complicated
            //if (input->sy < 0){
            //    output->weight *= 1 + (input->sx * output->origin.x + input->sy * output->origin.y) / 2;
            //}
            //else{
            //    output->weight *= 1 + (input->sx * output->origin.x - input->sy * output->origin.y) / 2;
            //}
        }
    }


    // vignetting
    // float dist2 = input->sx * input->sx + input->sy * input->sy;
    // output->weight = 1.0f - .5*dist2;


    // not sure if needed, but can't hurt. Taken from solidangle website.
    // ----------------------------------------------------------------------------------------------
    // scale derivatives
    float dsx = input->dsx * camera.tan_fov;
    float dsy = input->dsy * camera.tan_fov;

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
