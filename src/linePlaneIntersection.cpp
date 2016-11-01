#include <iostream>
#include <iomanip>
#include <cmath>

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
    double length = std::sqrt(vector1.x * vector1.x + vector1.y * vector1.y + vector1.z * vector1.z);
 
    if(length == 0.0){
        length = 0.000000001;
    }
    if(vector1.x == 0.0){
        vector1.x = 0.000000001;
    }
    if(vector1.y == 0.0){
        vector1.y = 0.000000001;
    }
    if(vector1.z == 0.0){
        vector1.z = 0.000000001;
    }
 
    normalizedVector.x = vector1.x / length;
    normalizedVector.y = vector1.y / length;
    normalizedVector.z = vector1.z / length;
    return normalizedVector;
}

// intersection with y = 0
vec3 linePlaneIntersection(vec3 rayOrigin, vec3 rayDirection) {

    vec3 coord = {100.0, 0.0, 100.0};
    vec3 planeNormal = {0.0, 1.0, 0.0};

    rayDirection = vec3normalize(rayDirection);
    coord = vec3normalize(coord);

    double x = (vec3dot(coord, planeNormal) - vec3dot(planeNormal, rayOrigin)) / vec3dot(planeNormal, rayDirection);

    vec3 contact;
    contact.x = rayOrigin.x + (rayDirection.x * x);
    contact.y = rayOrigin.y + (rayDirection.y * x);
    contact.z = rayOrigin.z + (rayDirection.z * x);

    return contact;
}


int main(){

    vec3 rayDirection = {-3.0, -4.0, 0.0};
    vec3 rayOrigin = {2.0, 2.0, 0.0};

    vec3 intersection = linePlaneIntersection(rayOrigin, rayDirection);

    std::cout << std::fixed << std::setprecision(10) << "(" << intersection.x << ", " << intersection.y << ", " << intersection.z << ")" << std::endl;
}