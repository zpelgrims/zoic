#include <ai.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <cmath>
#include <functional>
#include <algorithm>
#include <numeric>



// READ IN TABULAR LENS DATA
void readTabularLensData(std::string lensDataFileName, std::vector<float>& lensRadiusCurvature,
                         std::vector<float>& lensThickness, std::vector<float>& lensIOR,
                         std::vector<float>& lensAperture){

    std::ifstream lensDataFile(lensDataFileName);
    std::string line;
    std::string token;
    std::stringstream iss;
    int lensDataCounter = 1;

    while (getline(lensDataFile, line))
    {
        if (line.length() == 0 || line[0] == '#'){
            std::cout << "Comment or empty line, skip dat shit" << std::endl;
        }
        else {
            iss << line;

            // put values (converting from string to float) into the vectors
            while (getline(iss, token, '\t') ){
                if (lensDataCounter == 1){
                    lensRadiusCurvature.push_back (std::stof(token));
                }

                if (lensDataCounter == 2){
                    lensThickness.push_back (std::stof(token));
                }

                if (lensDataCounter == 3){
                    lensIOR.push_back (std::stof(token));
                }

                if (lensDataCounter == 4){
                    lensAperture.push_back (std::stof(token));
                    lensDataCounter = 0;
                }
                lensDataCounter += 1;
            }

            iss.clear();
        }
    }
}




// RAY SPHERE INTERSECTIONS
void raySphereIntersection(AtVector rayDirection, AtVector rayOrigin, AtVector sphereCenter,
                           float sphereRadius, AtVector& hitPoint, AtVector& normal_hitPoint){

    // normalize raydirection
    rayDirection = AiV3Normalize(rayDirection);

    // calculate distance between sphere and rayorigin
    AtVector L = sphereCenter - rayOrigin;

    // project the directionvector onto the distancevector
    float tca = AiV3Dot(L, rayDirection);

    //
    float radius2 = sphereRadius * sphereRadius;

    // if intersection is in the opposite direction of the ray, don't worry about it
    if (tca < 0) {std::cout << "no intersection"<< std::endl;}

    // ?
    float d2 = AiV3Dot(L, L) - tca * tca;

    // if the distance from the ray to the spherecenter is larger than its radius, don't worry about it
    if (d2 > radius2){ std::cout << "no intersection"<< std::endl;}

    // pythagoras' theorem
    float thc = sqrt(radius2 - d2);

    // only calculate the first intersection point, no need for second one in this case!
    float t0 = tca - thc;
    //float t1 = tca + thc;

    // calculate hit point with the vector equation
    hitPoint = rayOrigin + rayDirection * t0;

    std::cout << hitPoint.x << " " << hitPoint.y << " " << hitPoint.z << std::endl;

    // compute normal of hitpoint
    normal_hitPoint = AiV3Normalize(hitPoint - sphereCenter);

    std:: cout << "normal_hitpoint: " << normal_hitPoint.x << " " << normal_hitPoint.y << " " << normal_hitPoint.z << std::endl;
}




// TRANSMISSION VECTOR
void calculateTransmissionVector(float ior1, float ior2, AtVector incidentVector, AtVector normalVector, AtVector& transmissionVector){

    // VECTORS NEED TO BE NORMALIZED BEFORE USE!
    double eta = ior1 / ior2;
    double c1 = - AiV3Dot(incidentVector, normalVector); // std::inner_product(begin(incidentVector), end(incidentVector), begin(normalVector), 0.0); // -VecDot(I, N);
    double cs2 = eta * eta * (1 - c1 * c1);

    if (cs2 > 1){ // total internal reflection, can only occur when ior1 > ior2
        std::cout << "Total internal reflection case";
    }

    double cosT = sqrt(1.0 - cs2);

    transmissionVector.x = incidentVector.x * eta + normalVector.x * (eta * c1 - cosT);
    transmissionVector.y = incidentVector.y * eta + normalVector.y * (eta * c1 - cosT);
    transmissionVector.z = incidentVector.z * eta + normalVector.z * (eta * c1 - cosT);

    std::cout << "Transmission vector ..SHOULD.. be: {" << transmissionVector.x << ", " << transmissionVector.y << ", " << transmissionVector.z << "}" << std::endl;
}





int main()
{

    std::string lensDataFileName;
    std::vector<float> lensRadiusCurvature;
    std::vector<float> lensThickness;
    std::vector<float> lensIOR;
    std::vector<float> lensAperture;

    lensDataFileName = "/Users/zpelgrims/Downloads/lens/dgauss.50mm.dat";

    readTabularLensData(lensDataFileName, lensRadiusCurvature, lensThickness, lensIOR, lensAperture);


    // print values
    for(int i=0; i<lensRadiusCurvature.size(); ++i){std::cout << lensRadiusCurvature[i] << ' ';}
    std::cout << std::endl;
    for(int i=0; i<lensThickness.size(); ++i){std::cout << lensThickness[i] << ' ';}
    std::cout << std::endl;
    for(int i=0; i<lensIOR.size(); ++i){std::cout << lensIOR[i] << ' ';}
    std::cout <<std::endl;
    for(int i=0; i<lensAperture.size(); ++i){std::cout << lensAperture[i] << ' ';}
    std::cout << std::endl;




    AtVector rayDirection = {5,5,0};
    AtVector rayOrigin = {0,0,0};
    AtVector sphereCenter = {3,3,0};
    float sphereRadius = 2.0f;
    AtVector hitPoint;
    AtVector normal_hitPoint;


    raySphereIntersection(rayDirection, rayOrigin, sphereCenter, sphereRadius, hitPoint, normal_hitPoint);





    float ior1 = 1.0; // the index of refraction of first medium
    float ior2 = 1.5; // the index of refraction of second mßedium

    AtVector incidentVector = {0.0, -1.0, 0.0}; // incoming vector
    AtVector normalVector = {1/sqrtf(2), 1/sqrtf(2), 0.0}; // normal vector of hitpoint
    AtVector transmissionVector = {0.0, 0.0, 0.0}; // transmitted vector

    calculateTransmissionVector(ior1, ior2, incidentVector, normalVector, transmissionVector);





    // actual loop
    for(int i=0; i<lensRadiusCurvature.size(); ++i){
        // write this shit tomorrow and smoke a spliff
    }


}
