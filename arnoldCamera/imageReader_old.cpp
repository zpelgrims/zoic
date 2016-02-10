// PPM IMAGE READER BY RUIFEL GUEIRAS
// I WROTE THE NORMALIZE FUNCTION

// TO COMPILE: g++ -std=c++11 imageReader.cpp -o imageReader

#ifndef IMAGEREADER_H
#define IMAGEREADER_H

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>    // std::sort
#include <functional>   // std::greater
#include <random>
#include <cstring>

typedef struct {
     unsigned char red,green,blue;
} PPMPixel;

typedef struct {
     int x, y;
     PPMPixel *data;

} PPMImage;

#define CREATOR "RPFELGUEIRAS"
#define RGB_COMPONENT_COLOR 255

static PPMImage *readPPM(const char *filename)
{
         char buff[16];
         PPMImage *img;
         FILE *fp;
         int c, rgb_comp_color;
         //open PPM file for reading
         fp = fopen(filename, "rb");
         if (!fp) {
              fprintf(stderr, "Unable to open file '%s'\n", filename);
              exit(1);
         }

         //read image format
         if (!fgets(buff, sizeof(buff), fp)) {
              perror(filename);
              exit(1);
         }

    //check the image format
    if (buff[0] != 'P' || buff[1] != '6') {
         fprintf(stderr, "Invalid image format (must be 'P6')\n");
         exit(1);
    }

    //alloc memory form image
    img = (PPMImage *)malloc(sizeof(PPMImage));
    if (!img) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

    //check for comments
    c = getc(fp);
    while (c == '#') {
    while (getc(fp) != '\n') ;
         c = getc(fp);
    }

    ungetc(c, fp);
    //read image size information
    if (fscanf(fp, "%d %d", &img->x, &img->y) != 2) {
         fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
         exit(1);
    }

    //read rgb component
    if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
         fprintf(stderr, "Invalid rgb component (error loading '%s')\n", filename);
         exit(1);
    }

    //check rgb component depth
    if (rgb_comp_color!= RGB_COMPONENT_COLOR) {
         fprintf(stderr, "'%s' does not have 8-bits components\n", filename);
         exit(1);
    }

    while (fgetc(fp) != '\n') ;
    //memory allocation for pixel data
    img->data = (PPMPixel*)malloc(img->x * img->y * sizeof(PPMPixel));

    if (!img) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

    //read pixel data from file
    if (fread(img->data, 3 * img->x, img->y, fp) != img->y) {
         fprintf(stderr, "Error loading image '%s'\n", filename);
         exit(1);
    }

    fclose(fp);
    return img;
}

/*
void writePPM(const char *filename, PPMImage *img)
{
    FILE *fp;
    //open file for output
    fp = fopen(filename, "wb");
    if (!fp) {
         fprintf(stderr, "Unable to open file '%s'\n", filename);
         exit(1);
    }

    //write the header file
    //image format
    fprintf(fp, "P6\n");

    //comments
    fprintf(fp, "# Created by %s\n",CREATOR);

    //image size
    fprintf(fp, "%d %d\n",img->x,img->y);

    // rgb component depth
    fprintf(fp, "%d\n",RGB_COMPONENT_COLOR);

    // pixel data
    fwrite(img->data, 3 * img->x, img->y, fp);
    fclose(fp);
}
*/

void changeColorPPM(PPMImage *img)
{
    if(img){
        for(int i=0; i < img->x * img->y; i++){
              //img->data[i].red = RGB_COMPONENT_COLOR - img->data[i].green;
              //img->data[i].green = RGB_COMPONENT_COLOR - img->data[i].green;
              //img->data[i].blue = RGB_COMPONENT_COLOR - img->data[i].green;

              //img->data[i].red = img->data[i].green;
              //img->data[i].green = img->data[i].green;
              //img->data[i].blue = img->data[i].green;
         }
    }
}


void probability(PPMImage *img, float randomNumberRow, float randomNumberColumn){

    if(img){


        // IDEALLY I WANT TO MOVE ALL THE CALCULATIONS THAT ONLY NEED TO BE DONE ONCE OUT OF THIS
        // EG DEFINING PIXEL VALUES ETC, CALCULATING PDF CDF, ..

        std::cout << "//////////// Image Width: " << img->x << std::endl;
        std::cout << "//////////// Image Height: " <<  img->y << std::endl;
        std::cout << "//////////// Total amount of pixels: " << img->x * img->y << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;

        // initialize arrays
        float pixelValues[img->x * img->y];
        float normalizedPixelValues[img->x * img->y];

        // for every pixel
        for(int i=0; i < img->x * img->y; ++i){

            //store pixel value in array
            // calculate luminance [Y = 0.3 R + 0.59 G + 0.11 B]
            pixelValues[i] = (img->data[i].red * 0.3) + (img->data[i].green * 0.59) + (img->data[i].blue * 0.11f);

            // print array
            std::cout << "Pixel Luminance: " << i << " -> " << pixelValues[i] << std::endl;
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
        float summedRowValues[img->y];
        float summedHorizontalNormalizedValues;
        int counterRow = 0;
        for(int i=0; i < img->y; ++i){

            summedHorizontalNormalizedValues = 0.0f;

            for(int j=0; j < img->x; ++j){

                summedHorizontalNormalizedValues += normalizedPixelValues[counterRow];
                counterRow += 1;
            }

            summedRowValues[i] = summedHorizontalNormalizedValues;
            std::cout << "Summed Values row [" << i << "]: " << summedRowValues[i] << std::endl;
        }



        // calculate sum of all row values, just to debug
        float totalNormalizedRowValue = 0.0f;
        for(int i=0; i < img->y; ++i){
            totalNormalizedRowValue += summedRowValues[i];
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "Debug: Summed Row Value: " << totalNormalizedRowValue << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // sort row values from highest to lowest (probability density function)
        // create array of pointers
        float *rowPointers[img->y];

        // for every row, assign the address of the summed row values to the pointer in the array
        for (int i = 0; i < img->y; ++i){
            rowPointers[i] = &summedRowValues[i];
        }

        // sort pointers based on their linked value?
        std::sort(rowPointers, rowPointers + img->y, [&] (float *a, float *b) {
            return *a > *b;
        });

        // print values
        for(int i = 0; i < img->y; ++i){
            std::cout << "PDF row [" << rowPointers[i] - summedRowValues << "]: " << *rowPointers[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // For every row, add the sum of all previous row (cumulative distribution function)
        // Position in list doesn't change with this function, so no need for extra pointer shit
        float cdfRow[img->y];
        for (int i = 0; i < img->y; ++i){
            cdfRow[i] = cdfRow[i-1] + *rowPointers[i];
            std::cout << "CDF row [" << rowPointers[i] - summedRowValues << "]: " << cdfRow[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // print random number between 0 and 1
        std::cout << "RANDOM NUMBER ROW: " << randomNumberRow << std::endl;

        // find upper bound of random number in the array
        float *pUpperBound = std::upper_bound(cdfRow, cdfRow + img->y, randomNumberRow);
        std::cout << "UPPER BOUND: " << *pUpperBound << std::endl;

        // find which element of the array the upper bound is
        int x = std::distance(cdfRow, std::find(cdfRow, cdfRow + img->y, *pUpperBound));

        // find actual pixel row
        int actualPixelRow = rowPointers[x] - summedRowValues;

        // print values
        std::cout << "INDEX IN CDF ROW: " << x << std::endl;
        std::cout << "ACTUAL PIXEL ROW: " << actualPixelRow << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // divide pixel values of each pixel by the sum of the pixel values of that row (Normalize)
        int rowCounter = 0;
        int tmpCounter = 0;
        float normalizedValuesPerRow[img->x * img->y];
        for (int i = 0; i < img->x * img->y; ++i){

            // avoid division by 0
            if ((normalizedPixelValues[i] != 0) && (summedRowValues[rowCounter] != 0)){
                normalizedValuesPerRow[i] = normalizedPixelValues[i] / summedRowValues[rowCounter];
            }
            else{
                normalizedValuesPerRow[i] = 0;
            }

            tmpCounter += 1;

            // silly counter, there must be faster ways to do this but i'm not exactly a genius
            if (tmpCounter == img->x){
                rowCounter += 1;
                tmpCounter = 0;
            }

            std::cout << "Normalized Pixel value per row: " << i << ": " << normalizedValuesPerRow[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // sort column values from highest to lowest per row (probability density function)
        // create array of pointers
        float *columnPointers[img->x * img->y];

        // for every pixel, assign the address of the normalizedValuesPerRow values to the pointer in the array
        for (int i = 0; i < img->x * img->y; ++i){
            columnPointers[i] = &normalizedValuesPerRow[i];
        }

        for (int i = 0; i < img->x * img->y; i+=img->x){
            // sort pointers based on their linked value?
            std::sort(columnPointers + i, columnPointers + i + img->x, [&] (float *a, float *b) {
                return *a > *b;
            });
        }

        // print values
        for(int i = 0; i < img->x * img->y; ++i){
            std::cout << "PDF column [" << columnPointers[i] - normalizedValuesPerRow << "]: " << *columnPointers[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;



        // For every column per row, add the sum of all previous columns (cumulative distribution function)
        // Position in list doesn't change with this function, so no need for extra pointer shit
        float cdfColumn[img->x * img->y];
        int cdfCounter = 0;

        for (int i = 0; i < img->x * img->y; ++i){
            if(cdfCounter == img->x){
                    cdfColumn[i] = *columnPointers[i];
                    cdfCounter = 0;
            }
            else{
                cdfColumn[i] = cdfColumn[i-1] + *columnPointers[i];
            }

            cdfCounter += 1;

            std::cout << "CDF column [" << columnPointers[i] - normalizedValuesPerRow << "]: " << cdfColumn[i] << std::endl;
        }
        std::cout << "----------------------------------------------" << std::endl;



        // print random number between 0 and 1
        std::cout << "RANDOM NUMBER COLUMN: " << randomNumberColumn << std::endl;

        int startPixel = actualPixelRow * img->x;
        std::cout << "START PIXEL: " << startPixel << std::endl;



        // find upper bound of random number in the array
        float *pUpperBoundColumn = std::upper_bound(cdfColumn + startPixel, cdfColumn + startPixel + img->x, randomNumberRow);
        std::cout << "UPPER BOUND: " << *pUpperBoundColumn << std::endl;

        // find which element of the array the upper bound is
        int y = std::distance(cdfColumn, std::find(cdfColumn + startPixel, cdfColumn + startPixel + img->x, *pUpperBoundColumn));

        // find actual pixel column
        int actualPixelColumn = columnPointers[y] - normalizedValuesPerRow;
        int relativePixelColumn = actualPixelColumn - startPixel;

        // print values
        std::cout << "INDEX IN CDF COLUMN: " << y << std::endl;
        std::cout << "ACTUAL PIXEL COLUMN: " << actualPixelColumn << std::endl;
        std::cout << "RELATIVE PIXEL COLUMN (starting from 0): " << relativePixelColumn << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
    }
}

int main(){
    PPMImage *image;
    image = readPPM("lena2.ppm");

    // random number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distribution (0.0, 1.0);

    probability(image, distribution(gen), distribution(gen));

    // changeColorPPM(image);
    // writePPM("lena_corrected.ppm",image);
}

#endif
