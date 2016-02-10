// TO COMPILE: g++ -std=c++11 imageReader.cpp -o imageReader -L/usr/lib64 -lOpenImageIO -L/opt/appleseed/lib -ltiff


#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>    // std::sort
#include <functional>   // std::greater
#include <random>
#include <cstring>
#include <OpenImageIO/imageio.h>
#include <stdint.h>

struct imageData{
     int x, y;
     std::vector<uint8_t> pixelData;
     float* cdfRow;
     float* cdfColumn;
     float* summedRowValues;
     float* normalizedValuesPerRow;
     std::vector<float*> rowPointers;
     std::vector<float*> columnPointers;
     std::vector<int> rowIndices;
     std::vector<int> columnIndices;
};


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

    /* The specification, accessible as in->spec(), contains vital information such as the di-
    mensions of the image, number of color channels, and data type of the pixel values. This
    is enough to allow us to allocate enough space for the image. */
    const OpenImageIO::ImageSpec &spec = in->spec();
    img->x = spec.width;
    img->y = spec.height;
    int channels = spec.nchannels;

    img->pixelData.reserve(img->x * img->y * channels);
    in->read_image (OpenImageIO::TypeDesc::UINT8, &img->pixelData[0]);
    in->close ();
    delete in;

    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    std::cerr << "Image Width: " << img->x << std::endl;
    std::cerr << "Image Height: " << img->y << std::endl;
    std::cerr << "Image Channels: " << channels << std::endl;
    std::cout << "Total amount of pixels: " << img->x * img->y << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;


    for (int i = 0; i < img->x * img->y * channels; i++)
    {
        std::cout << "PIXELVALUERGB: " << (int)img->pixelData[i] << std::endl;
    }

    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;

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
            //store pixel value in array
            // calculate luminance [Y = 0.3 R + 0.59 G + 0.11 B]
            pixelValues[i] = (img->pixelData[tmpPixelCounter] * 0.3) + (img->pixelData[tmpPixelCounter+1] * 0.59) + (img->pixelData[tmpPixelCounter+2] * 0.11f);
            //std::cout << "Adding Pixel Number: " << tmpPixelCounter << ", " << tmpPixelCounter+1 << ", " << tmpPixelCounter+2 << std::endl;

            // print array
            std::cout << "Pixel Luminance: " << i << " -> " << pixelValues[i] << std::endl;
            tmpPixelCounter += 3;
        }


        // calculate sum of all pixel values
        float totalValue = 0.0f;
        for(int i=0; i < img->x *  img->y; ++i){
            totalValue += pixelValues[i];
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "Total Pixel Value: " << totalValue << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // normalize pixel values so sum = 1
        for(int i=0; i < img->x *  img->y; ++i){

            normalizedPixelValues[i] = pixelValues[i] / totalValue;

            // print array
            std::cout << "Normalized Pixel Value: " << i << ": " << normalizedPixelValues[i] << std::endl;
        }



        // calculate sum of all normalized pixel values, to check
        float totalNormalizedValue = 0.0f;
        for(int i=0; i < img->x *  img->y; ++i){
            totalNormalizedValue += normalizedPixelValues[i];
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "Total Normalized Pixel Value: " << totalNormalizedValue << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // calculate sum for each row
        img->summedRowValues = new float [img->y];
        float summedHorizontalNormalizedValues;
        int counterRow = 0;

        for(int i=0; i < img->y; ++i){

            summedHorizontalNormalizedValues = 0.0f;

            for(int j=0; j < img->x; ++j){

                summedHorizontalNormalizedValues += normalizedPixelValues[counterRow];
                counterRow += 1;
            }

            img->summedRowValues[i] = summedHorizontalNormalizedValues;
            std::cout << "Summed Values row [" << i << "]: " << img->summedRowValues[i] << std::endl;
        }



        // calculate sum of all row values, just to debug
        float totalNormalizedRowValue = 0.0f;

        for(int i=0; i < img->y; ++i){
            totalNormalizedRowValue += img->summedRowValues[i];
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "Debug: Summed Row Value: " << totalNormalizedRowValue << std::endl;
        std::cout << "----------------------------------------------" << std::endl;


        // sort row values from highest to lowest (probability density function)
        // create array of pointers
        float* newRow = new float[ img->y ];
        img->rowPointers.reserve( img->y );


        // for every row, assign the address of the summed row values to the pointer in the array
        for (int i = 0; i < img->y; ++i){
            img->rowPointers[i] = &img->summedRowValues[i];
        }

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

        // print values, NEW
        for(int i = 0; i < img->y; ++i){
            std::cout << "PDF row [" <<  summedRowValueCopyIndices[i] << "]: " << summedRowValueCopy[summedRowValueCopyIndices[i]] << std::endl;
        }

        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;


        // For every row, add the sum of all previous row (cumulative distribution function)
        // Position in list doesn't change with this function, so no need for extra pointer shit

        img->cdfRow = new float [img->y];
        img->rowIndices.reserve(img->y);

        for (int i = 0; i < img->y; ++i){

            img->cdfRow[i] = img->cdfRow[i-1] + summedRowValueCopy[summedRowValueCopyIndices[i]];
            img->rowIndices[i] = summedRowValueCopyIndices[i];
            std::cout << "CDF row [" << summedRowValueCopyIndices[i] << "]: " << img->cdfRow[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // divide pixel values of each pixel by the sum of the pixel values of that row (Normalize)
        int rowCounter = 0;
        int tmpCounter = 0;
        img->normalizedValuesPerRow = new float [img->x * img->y];

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

            std::cout << "Normalized Pixel value per row: " << i << ": " << img->normalizedValuesPerRow[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // sort column values from highest to lowest per row (probability density function)
        // create array of pointers
        // float* newImage = new float[ img->y * img->x ];
        img->columnPointers.reserve( img->y * img->x);

        // for every pixel, assign the address of the normalizedValuesPerRow values to the pointer in the array
        for (int i = 0; i < img->x * img->y; ++i){
            img->columnPointers[i] = &img->normalizedValuesPerRow[i];
        }

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

        // print values
        for(int i = 0; i < img->x * img->y; ++i){
            std::cout << "PDF column [" << summedColumnValueCopyIndices[i] << "]: " << summedColumnValueCopy[summedColumnValueCopyIndices[i]] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // For every column per row, add the sum of all previous columns (cumulative distribution function)
        // Position in list doesn't change with this function, so no need for extra pointer shit

        img->cdfColumn = new float [img->x];
        img->columnIndices.reserve(img->x * img->y);
        int cdfCounter = 0;

        //something goes wrong here, img->x values changes???? Only with certain image sizes. Weird.
        for (int i = 0; i < img->x * img->y; ++i){
            if(cdfCounter == img->x){
                    img->cdfColumn[i] = summedColumnValueCopy[summedColumnValueCopyIndices[i]];
                    cdfCounter = 0;
                    //std::cout << "DEBUG: img->x: " << img->x << std::endl;
            }
            else{

                img->cdfColumn[i] = img->cdfColumn[i-1] + summedColumnValueCopy[summedColumnValueCopyIndices[i]];
                //std::cout << "DEBUG: img->x: " << img->x << std::endl;
            }
            img->columnIndices[i] = summedColumnValueCopyIndices[i];
            cdfCounter += 1;

            std::cout << "CDF column [" << summedColumnValueCopyIndices[i] << "]: " << img->cdfColumn[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;


    }
}

// need to update all pointer shit, not using it any more
void bokehSample(imageData *img, float randomNumberRow, float randomNumberColumn){

    // print random number between 0 and 1
    std::cout << "RANDOM NUMBER ROW: " << randomNumberRow << std::endl;

    // find upper bound of random number in the array
    float *pUpperBound = std::upper_bound(img->cdfRow, img->cdfRow + img->y, randomNumberRow);
    std::cout << "UPPER BOUND: " << *pUpperBound << std::endl;

    // find which element of the array the upper bound is
    int x = std::distance(img->cdfRow, std::find(img->cdfRow, img->cdfRow + img->y, *pUpperBound));

    //std::cout << "DEBUG: img->rowPointers[x]: " << img->rowPointers[x] << std::endl;
    //std::cout << "DEBUG: img->summedRowValues: " << img->summedRowValues << std::endl;

    // find actual pixel row
    int actualPixelRow = img->rowIndices[x];


    // recalculate pixel row so that the center pixel is (0,0) - might run into problems with images of dimensions like 2x2, 4x4, 6x6, etc
    int recalulatedPixelRow = actualPixelRow - ((img->x - 1) / 2);

    // print values
    std::cout << "INDEX IN CDF ROW: " << x << std::endl;
    std::cout << "ACTUAL PIXEL ROW: " << actualPixelRow << std::endl;
    std::cout << "RECALCULATED PIXEL ROW: " << recalulatedPixelRow << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;




    // print random number between 0 and 1
    std::cout << "RANDOM NUMBER COLUMN: " << randomNumberColumn << std::endl;

    int startPixel = actualPixelRow * img->x;
    std::cout << "START PIXEL: " << startPixel << std::endl;



    // find upper bound of random number in the array
    float *pUpperBoundColumn = std::upper_bound(img->cdfColumn + startPixel, img->cdfColumn + startPixel + img->x, randomNumberRow);
    std::cout << "UPPER BOUND: " << *pUpperBoundColumn << std::endl;

    // find which element of the array the upper bound is
    int y = std::distance(img->cdfColumn, std::find(img->cdfColumn + startPixel, img->cdfColumn + startPixel + img->x, *pUpperBoundColumn));

    // find actual pixel column
    int actualPixelColumn = img->columnIndices[y];
    int relativePixelColumn = actualPixelColumn - startPixel;
    int recalulatedPixelColumn = relativePixelColumn - ((img->y - 1) / 2);

    // print values
    std::cout << "INDEX IN CDF COLUMN: " << y << std::endl;
    std::cout << "ACTUAL PIXEL COLUMN: " << actualPixelColumn << std::endl;
    std::cout << "RELATIVE PIXEL COLUMN (starting from 0): " << relativePixelColumn << std::endl;
    std::cout << "RECALCULATED PIXEL COLUMN: " << recalulatedPixelColumn << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;

}


int main(){

    imageData *image = nullptr;
    image = readImage("lena2.jpg");
    // Check if image is valid (is the pointer null?)
    if(!image){
        std::cout << "Couldn't open image, shit\n";
        exit(1);
    }



    // random number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distribution (0.0, 1.0);

    bokehProbability(image);
    bokehSample(image, distribution(gen), distribution(gen));

    // Clean up after ourselves
    delete image;
}
