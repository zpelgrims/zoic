// WATCH OUT, OVER HERE X is the axial direction, in the path tracer it is Z.

// drawing and coordinate code borrowed from the23 (27 02 2015)

const vec3 backgroundColor = vec3(0.15, 0.2, 0.25);
const vec3 white = vec3(1.0, 1.0, 1.0);
const vec3 bluegreen = vec3(0.14, 0.7, 0.5);
const vec3 blue = vec3(0.039, 0.174, 0.246);
const vec3 orange = vec3(0.8, 0.4, 0.15);
const vec3 red = vec3(0.9, 0.4, 0.5);
const vec3 green = vec3(0.50, 0.7, 0.25);

bool debug = false;

// define the visible area
const vec2 bot = vec2(310,-150);
const vec2 top = vec2(150,150);
//const vec2 bot = vec2(750,-550);
//const vec2 top = vec2(1050,550);

float lineWidth = (top.x-bot.x)/iResolution.x + 0.5;
float pointWidth = (top.x-bot.x)/iResolution.x + 1.0;
const float soft = 0.15;


struct LensData{
    float lensRadiusCurvature[11];
    float lensThickness[11];
    float lensIOR[11];
    float lensAperture[11];
} ld;


vec3 drawLine(vec3 col, vec3 lineColor, vec2 coord, vec2 p1, vec2 p2, float thickness){
    float d = dot(coord - p1, p2 - p1) / length(p2 - p1);
    d /= length(p2 - p1);
    d = clamp(step(0.0, d) * d, 0.0, 1.0);
    d = distance(p1 + d * (p2 - p1), coord);
    float dst = 1.0 - smoothstep(thickness - soft, thickness + soft, d);
    return mix(col, lineColor, dst);
}

vec3 drawDot(vec3 col, vec3 pointColor, vec2 coord, vec2 p, float thickness){
    float dst = distance(coord, p);
    dst = 1.0 - smoothstep(thickness - soft, thickness + soft, dst);
    return mix(col, pointColor, dst);
}



// RAY SPHERE INTERSECTIONS
vec3 raySphereIntersection(vec3 ray_direction, vec3 ray_origin, vec3 sphere_center, float sphere_radius, bool reverse){

    ray_direction = normalize(ray_direction);
    vec3 L = sphere_center - ray_origin;

    // project the directionvector onto the distancevector
    float tca = dot(L, ray_direction);

    float radius2 = sphere_radius * sphere_radius;

    // if intersection is in the opposite direction of the ray, don't worry about it
    //if (tca < 0.0) {return vec3(0.0,0.0,0.0);}

    float d2 = dot(L, L) - tca * tca;

    // if the distance from the ray to the spherecenter is larger than its radius, don't worry about it
    // come up with a better way of killing the ray (already there in path tracer)
    //if (d2 > radius2){return vec3(0.0,0.0,0.0);}

    // pythagoras' theorem
    float thc = sqrt(radius2 - d2);

    if(!reverse){
        return ray_origin + ray_direction * (tca + thc * sign(sphere_radius));
    }
    else{
        return ray_origin + ray_direction * (tca - thc * sign(sphere_radius));
    }
}


// COMPUTE NORMAL HITPOINT
vec3 intersectionNormal(vec3 hit_point, vec3 sphere_center, float sphere_radius){
    return normalize(sphere_center - hit_point) * sign(sphere_radius);
}


// TRANSMISSION VECTOR
vec3 calculateTransmissionVector(float ior1, float ior2, vec3 incidentVector, vec3 normalVector){

    vec3 transmissionVector;

    // VECTORS NEED TO BE NORMALIZED BEFORE USE!
    incidentVector = normalize(incidentVector);
    normalVector = normalize(normalVector);


    float eta = ior1 / ior2;
    float c1 = - dot(incidentVector, normalVector); // std::inner_product(begin(incidentVector), end(incidentVector), begin(normalVector), 0.0);
    float cs2 = eta * eta * (1.0 - c1 * c1);

    //if (cs2 > 1.0){ // total internal reflection, can only occur when ior1 > ior2
        //std::cout << "Total internal reflection case";
        // kill ray here
    //}

    float cosT = sqrt(abs(1.0 - cs2));

    transmissionVector.x = incidentVector.x * eta + normalVector.x * (eta * c1 - cosT);
    transmissionVector.y = incidentVector.y * eta + normalVector.y * (eta * c1 - cosT);
    transmissionVector.z = incidentVector.z * eta + normalVector.z * (eta * c1 - cosT);

    return transmissionVector;

}


// LINE LINE INTERSECTIONS
vec2 lineLineIntersection(vec3 line1_origin, vec3 line1_direction, vec3 line2_origin, vec3 line2_direction){
    // Get A,B,C of first line - points : ps1 to pe1
    float A1 = line1_direction.y - line1_origin.y;
    float B1 = line1_origin.x - line1_direction.x;
    float C1 = A1 * line1_origin.x + B1 * line1_origin.y;

    // Get A,B,C of second line - points : ps2 to pe2
    float A2 = line2_direction.y - line2_origin.y;
    float B2 = line2_origin.x - line2_direction.x;
    float C2 = A2 * line2_origin.x + B2 * line2_origin.y;

    // Get delta and check if the lines are parallel
    float delta = A1 * B2 - A2 * B1;

    // now return the Vector2 intersection point
    return vec2((B2 * C1 - B1 * C2) / delta, (A1 * C2 - A2 * C1) / delta);
}


float calculateImageDistance(float objectDistance){

    float imageDistance;
    vec3 ray_origin_focus = vec3(objectDistance, 0.0, 0.0);

    // 20.0 needs to be changed to a number as small as possible whilst still getting no numerical errors. (eg 0.001)
    vec3 ray_direction_focus = vec3(- objectDistance, 20.0, 0.0); //why do i need crazy high numbers to achieve a good result?! Check with doubles.
    float summedThickness_focus = 0.0;

    // change this to i < ld.lensRadiusCurvature.size()
    const int lensElementsCount = 11;
    for(int i = 0; i < lensElementsCount; i++){ // change this to i < ld.lensRadiusCurvature.size()

        if(i==0){
            for(int k = 0; k < lensElementsCount; k++){
                summedThickness_focus += ld.lensThickness[k];
            }
        }

        // (condition) ? true : false;
        i == 0 ? summedThickness_focus = summedThickness_focus : summedThickness_focus -= ld.lensThickness[lensElementsCount - i];

        if(ld.lensRadiusCurvature[i] == 0.0){
            ld.lensRadiusCurvature[i] = 99999.0;
        }

        vec3 sphere_center;

        sphere_center.x = summedThickness_focus - ld.lensRadiusCurvature[lensElementsCount -1 -i];
        sphere_center.y = 0.0;
        sphere_center.z = 0.0;

        vec3 hit_point = raySphereIntersection(ray_direction_focus, ray_origin_focus, sphere_center, ld.lensRadiusCurvature[lensElementsCount - 1 - i], true);
        vec3 hit_point_normal = intersectionNormal(hit_point, sphere_center, - ld.lensRadiusCurvature[lensElementsCount - 1 - i]);


        if(i==0){
            ray_direction_focus = calculateTransmissionVector(1.0, ld.lensIOR[lensElementsCount - 1 - i], ray_direction_focus, hit_point_normal);
        }
        else{
            ray_direction_focus = calculateTransmissionVector(ld.lensIOR[lensElementsCount - i], ld.lensIOR[lensElementsCount - i - 1], ray_direction_focus, hit_point_normal);

        }

        // draw ray
        //col = drawLine(col, vec3(.04, .6, .4), coord, vec2(ray_origin_focus.x, ray_origin_focus.y), vec2(hit_point.x, hit_point.y), lineWidth / 3.0);


        // set hitpoint to be the new origin
        ray_origin_focus = hit_point;

        // shoot off rays after last refraction
        // change 10 to something like .size() - 1
        if(i == 10){ // last element in array, change this in path tracer
            ray_direction_focus = calculateTransmissionVector(ld.lensIOR[lensElementsCount - 1 - i], 1.0, ray_direction_focus, hit_point_normal);
            //col = drawLine(col, vec3(.04, .6, .4), coord, vec2(ray_origin_focus.x, ray_origin_focus.y), vec2(ray_origin_focus.x + ray_direction_focus.x * 10000.0, ray_origin_focus.y + ray_direction_focus.y * 10000.0), lineWidth / 3.0);

            // give these some variable values instead of arbitrary ones
            // find intersection point
            imageDistance = lineLineIntersection(vec3(-99999.0, 0.0, 0.0), vec3(99999.0, 0.0, 0.0), ray_origin_focus, vec3(ray_origin_focus.x + ray_direction_focus.x, ray_origin_focus.y + ray_direction_focus.y , 0.0)).x;
            //col = drawLine(col, red, coord, vec2(imageDistance.x, 10.0), vec2(imageDistance.x, -10.0), lineWidth);
        }
    }

    return imageDistance;

}






void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // coordinate stuff, borrowed!
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec2 area = top - bot;
    float ratioScreen = iResolution.x/ iResolution.y;
    float ratioArea = area.x / area.y;
    vec2 padding = vec2(ratioScreen / ratioArea, ratioArea /ratioScreen);
    padding = max(vec2(0.0), area * padding - area);
    area += padding;
    vec2 coord = uv * area + bot - (padding * 0.5);

    // set background color
    vec3 col = backgroundColor;



    // put some lens data in the vectors, taken from the paper by Kolb. Need to parse a file instead
    // typing it out explicitely like this is quite ashaming. Hey ho.

    ld.lensRadiusCurvature[0] = -79.460;
    ld.lensRadiusCurvature[1] = 874.130;
    ld.lensRadiusCurvature[2] = -40.770;
    ld.lensRadiusCurvature[3] = 81.540;
    ld.lensRadiusCurvature[4] = -28.99;
    ld.lensRadiusCurvature[5] = 0.0;
    ld.lensRadiusCurvature[6] = 25.5;
    ld.lensRadiusCurvature[7] = 81.54;
    ld.lensRadiusCurvature[8] = 38.55;
    ld.lensRadiusCurvature[9] = 169.66;
    ld.lensRadiusCurvature[10] = 58.95;

    ld.lensThickness[0] = 72.228;// - 3.5; //not sure why lens is converging rays by default without this adjustment, must be my mistake!
    ld.lensThickness[1] = 6.44;
    ld.lensThickness[2] = 0.38;
    ld.lensThickness[3] = 12.13;
    ld.lensThickness[4] = 2.36;
    ld.lensThickness[5] = 9.0;
    ld.lensThickness[6] = 11.41;
    ld.lensThickness[7] = 6.55;
    ld.lensThickness[8] = 9.05;
    ld.lensThickness[9] = 0.24;
    ld.lensThickness[10] = 7.52;

    ld.lensIOR[0] = 1.0;
    ld.lensIOR[1] = 1.717;
    ld.lensIOR[2] = 1.0;
    ld.lensIOR[3] = 1.658;
    ld.lensIOR[4] = 1.603;
    ld.lensIOR[5] = 1.0;
    ld.lensIOR[6] = 1.0;
    ld.lensIOR[7] = 1.699;
    ld.lensIOR[8] = 1.670;
    ld.lensIOR[9] = 1.0;
    ld.lensIOR[10] = 1.67;

    ld.lensAperture[0] = 40.0;
    ld.lensAperture[1] = 40.0;
    ld.lensAperture[2] = 40.0;
    ld.lensAperture[3] = 40.0;
    ld.lensAperture[4] = 34.0;
    ld.lensAperture[5] = 34.2;
    ld.lensAperture[6] = 36.0;
    ld.lensAperture[7] = 46.0;
    ld.lensAperture[8] = 46.0;
    ld.lensAperture[9] = 50.4;
    ld.lensAperture[10] = 50.4;



    vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    vec3 ray_origin_original = vec3(0.0, 0.0, 0.0);
    vec3 ray_direction;
    vec3 hit_point ;
    vec3 hit_point_normal;
    vec3 sphere_center;
    float summedThickness;
    float summedThickness_pp;
    const int lensElementsCount = 11;
    vec2 pp1;
    vec2 pp2;
    vec2 focalPointImageSide;
    vec2 focalPointObjectSide;
    float apertureRadius = 30.0;
    bool lensElementAperture;
    float objectDistance = 1500.0;
    vec2 imageDistance;


    // shoot two parallel rays to find priciple planes (thick lens approximation)
    // right now i´m using the same loop written twice with different values, chuck this into a function and reuse instead
    /*
    vec3 ray_direction_pp;
    vec3 ray_origin_pp;

    const int ray_count_pp = 2;

    for (int j = 0; j < ray_count_pp; j++){

        if(j == 0){

            ray_origin_pp = vec3(0.0, 5.0, 0.0);
            ray_direction_pp = vec3(1000.0, 0.0, 0.0);

            for(int i = 0; i < lensElementsCount; i++){ // change this to i < ld.lensRadiusCurvature.size()

                // (condition) ? true : false;
                i == 0 ? summedThickness_pp = ld.lensThickness[0] : summedThickness_pp += ld.lensThickness[i];

                if(ld.lensRadiusCurvature[i] == 0.0){
                    ld.lensRadiusCurvature[i] = 99999.0;
                }

                sphere_center.x = summedThickness_pp - ld.lensRadiusCurvature[i];
                sphere_center.y = 0.0;
                sphere_center.z = 0.0;

                hit_point = raySphereIntersection(ray_direction_pp, ray_origin_pp, sphere_center, ld.lensRadiusCurvature[i], false);
                hit_point_normal = intersectionNormal(hit_point, sphere_center, ld.lensRadiusCurvature[i]);
                ray_direction_pp = calculateTransmissionVector(ld.lensIOR[i], ld.lensIOR[i+1], ray_direction_pp, hit_point_normal);

                // draw rays in different colours depending on the medium they travel in
                //if(ld.lensIOR[i] != 1.0){col = drawLine(col, lineColorRays, coord, vec2(ray_origin_pp.x, ray_origin_pp.y), vec2(hit_point.x, hit_point.y), lineWidth / 2.0);}
                //else{col = drawLine(col, vec3(1.0, 1.0, 1.0), coord, vec2(ray_origin_pp.x, ray_origin_pp.y), vec2(hit_point.x, hit_point.y), lineWidth / 2.0);}
                if(debug){col = drawLine(col, vec3(.04, .6, .4), coord, vec2(ray_origin_pp.x, ray_origin_pp.y), vec2(hit_point.x, hit_point.y), lineWidth / 3.0);}


                // set hitpoint to be the new origin
                ray_origin_pp = hit_point;

                // shoot off rays after last refraction
                if(i == 10){ // last element in array
                    ray_direction_pp = calculateTransmissionVector(ld.lensIOR[i], 1.0, ray_direction_pp, hit_point_normal);
                    if(debug){col = drawLine(col, vec3(.04, .6, .4), coord, vec2(ray_origin_pp.x, ray_origin_pp.y), vec2(ray_origin_pp.x + ray_direction_pp.x * 10000.0, ray_origin_pp.y + ray_direction_pp.y * 10000.0), lineWidth / 3.0);}

                    // give these some variable values instead of arbitrary ones
                    // find intersection points
                    pp1 = lineLineIntersection(vec3(0.0, 5.0, 0.0), vec3(9999.0, 5.0, 0.0), ray_origin_pp, vec3(ray_origin_pp.x + ray_direction_pp.x, ray_origin_pp.y + ray_direction_pp.y , 0.0));
                    if(debug){col = drawDot(col, white, coord, pp1, pointWidth);}
                    col = drawLine(col, orange, coord, vec2(pp1.x, 30.0), vec2(pp1.x, -30.0), lineWidth);
                    if(debug){col = drawLine(col, vec3(0.5, 0.5, 0.5), coord, vec2(pp1.x, pp1.y), vec2(ray_origin_pp.x, ray_origin_pp.y), lineWidth / 3.0);}

                    focalPointObjectSide = lineLineIntersection(vec3(0.0, 0.0, 0.0), vec3(9999.0, 0.0, 0.0), ray_origin_pp, vec3(ray_origin_pp.x + ray_direction_pp.x, ray_origin_pp.y + ray_direction_pp.y , 0.0));
                    col = drawLine(col, bluegreen, coord, vec2(focalPointObjectSide.x, 30.0), vec2(focalPointObjectSide.x, -30.0), lineWidth);

                }
            }
        }else{

            ray_origin_pp = vec3(50.0, -5.0, 0.0);
            ray_direction_pp = vec3(-1000.0, 0.0, 0.0);
            summedThickness_pp = 0.0;

            for(int i = 0; i < lensElementsCount; i++){ // change this to i < ld.lensRadiusCurvature.size()

                if(i==0){
                    for(int k = 0; k < lensElementsCount; k++){
                        summedThickness_pp += ld.lensThickness[k];
                    }
                }

                // (condition) ? true : false;
                i == 0 ? summedThickness_pp = summedThickness_pp : summedThickness_pp -= ld.lensThickness[lensElementsCount - i];

                if(ld.lensRadiusCurvature[i] == 0.0){
                    ld.lensRadiusCurvature[i] = 99999.0;
                }

                sphere_center.x = summedThickness_pp - ld.lensRadiusCurvature[lensElementsCount -1 -i];
                sphere_center.y = 0.0;
                sphere_center.z = 0.0;

                hit_point = raySphereIntersection(ray_direction_pp, ray_origin_pp, sphere_center, ld.lensRadiusCurvature[lensElementsCount - 1 - i], true);
                hit_point_normal = intersectionNormal(hit_point, sphere_center, - ld.lensRadiusCurvature[lensElementsCount - 1 - i]);

                if(i==0){
                    ray_direction_pp = calculateTransmissionVector(1.0, ld.lensIOR[lensElementsCount - 1 - i], ray_direction_pp, hit_point_normal);
                }
                else{
                    ray_direction_pp = calculateTransmissionVector(ld.lensIOR[lensElementsCount - i], ld.lensIOR[lensElementsCount - i - 1], ray_direction_pp, hit_point_normal);

                }

                // draw rays in different colours depending on the medium they travel in
                if(debug){col = drawLine(col, vec3(.04, .6, .4), coord, vec2(ray_origin_pp.x, ray_origin_pp.y), vec2(hit_point.x, hit_point.y), lineWidth / 3.0);}


                // set hitpoint to be the new origin
                ray_origin_pp = hit_point;

                // shoot off rays after last refraction
                if(i == 10){ // last element in array
                    ray_direction_pp = calculateTransmissionVector(ld.lensIOR[lensElementsCount - 1 - i], 1.0, ray_direction_pp, hit_point_normal);
                    if(debug){col = drawLine(col, vec3(.04, .6, .4), coord, vec2(ray_origin_pp.x, ray_origin_pp.y), vec2(ray_origin_pp.x + ray_direction_pp.x * 10000.0, ray_origin_pp.y + ray_direction_pp.y * 10000.0), lineWidth / 3.0);}

                    // give these some variable values instead of arbitrary ones
                    // find intersection points
                    pp2 = lineLineIntersection(vec3(0.0, -5.0, 0.0), vec3(9999.0, -5.0, 0.0), ray_origin_pp, vec3(ray_origin_pp.x + ray_direction_pp.x, ray_origin_pp.y + ray_direction_pp.y , 0.0));
                    if(debug){col = drawDot(col, white, coord, pp2, pointWidth);} // debug drawing
                    if(debug){col = drawLine(col, vec3(0.5, 0.5, 0.5), coord, vec2(pp2.x, pp2.y), vec2(ray_origin_pp.x, ray_origin_pp.y), lineWidth / 3.0);}
                    col = drawLine(col, red, coord, vec2(pp2.x, 30.0), vec2(pp2.x, -30.0), lineWidth);


                    focalPointImageSide = lineLineIntersection(vec3(0.0, 0.0, 0.0), vec3(9999.0, 0.0, 0.0), ray_origin_pp, vec3(ray_origin_pp.x + ray_direction_pp.x, ray_origin_pp.y + ray_direction_pp.y , 0.0));
                    col = drawLine(col, bluegreen, coord, vec2(focalPointImageSide.x, 30.0), vec2(focalPointImageSide.x, -30.0), lineWidth);

                }
            }

        }
    }
    */




    // this goes into the update section
    ld.lensThickness[0] -= calculateImageDistance(objectDistance);



    // compute ray directions
    // in path tracer this will be random within limitations
    // don't fire rays that will not hit the first lens element
    // firing rays within the solidangle of the first lens elements will always work
    // but this is extremely wasteful
    // use thick lens approximation for exit pupil instead?

    const int ray_count = 11; // obviously change this
    float ray_direction_height_variation;
    bool apertureSampling = true;

    if(!apertureSampling){
        ray_direction_height_variation = (ld.lensAperture[0] / 2.0) / float(ray_count);
    }else{
        ray_direction_height_variation = (apertureRadius) / float(ray_count);
    }



    for (int j = 0; j < ray_count; j++){

        int boundaryDrawCounter = 0; // just for drawing

        if(!apertureSampling){
            // spatialy distribute rays to hit the first element, this is the risk-free method but can be optimised
            ray_direction = vec3(ld.lensThickness[0], - (ld.lensAperture[0] / 2.0) + (ray_direction_height_variation * 2.0 * float(j)) + ray_direction_height_variation, 0.0);
        }else{
            // find how far the aperture is
            float apertureDistance = 0.0;
            for(int i = 0; i < lensElementsCount; i++){
                apertureDistance += ld.lensThickness[i];
                if(ld.lensRadiusCurvature[i] == 0.0 || ld.lensRadiusCurvature[i] == 99999.0){
                    break;
                }
            }
            ray_direction = vec3(apertureDistance, - apertureRadius + (ray_direction_height_variation * 2.0 * float(j)) + ray_direction_height_variation, 0.0);
        }



        for(int i = 0; i < lensElementsCount; i++){ // change this to i < ld.lensRadiusCurvature.size()

            // (condition) ? true : false;
            i == 0 ? summedThickness = ld.lensThickness[0] : summedThickness += ld.lensThickness[i];

            if(ld.lensRadiusCurvature[i] == 0.0 || ld.lensRadiusCurvature[i] == 99999.0){
                ld.lensRadiusCurvature[i] = 99999.0;
                lensElementAperture = true;
            }

            sphere_center.x = summedThickness - ld.lensRadiusCurvature[i];
            sphere_center.y = 0.0;
            sphere_center.z = 0.0;

            hit_point = raySphereIntersection(ray_direction, ray_origin, sphere_center, ld.lensRadiusCurvature[i], false);

            // coordinate space switch here! watch out
            float hitPointHypotenuse = sqrt(hit_point.y * hit_point.y + hit_point.z * hit_point.z);
            if(hitPointHypotenuse > (ld.lensAperture[i-1]/2.0)){
                // kill ray (weight = 0)
                 break;
            }

            //watch out with this, not sure if it's behaving like expected.. hard to tell!
            if(lensElementAperture == true && hitPointHypotenuse > apertureRadius){
                // kill ray (weight = 0)
                break;
            }

            hit_point_normal = intersectionNormal(hit_point, sphere_center, ld.lensRadiusCurvature[i]);

            // if ior1 and ior2 are not the same, calculate new ray direction vector
            if(ld.lensIOR[i] != ld.lensIOR[i+1]){
                ray_direction = calculateTransmissionVector(ld.lensIOR[i], ld.lensIOR[i+1], ray_direction, hit_point_normal);
            }


            // draw rays in different colours depending on the medium they travel in
            if(ld.lensIOR[i] != 1.0){col = drawLine(col, red, coord, vec2(ray_origin.x, ray_origin.y), vec2(hit_point.x, hit_point.y), lineWidth);}
            else{col = drawLine(col, white, coord, vec2(ray_origin.x, ray_origin.y), vec2(hit_point.x, hit_point.y), lineWidth);}

            // draw dots on hitpoints
            col = drawDot(col, white, coord, vec2(hit_point.x, hit_point.y), pointWidth);

            // draw normals
            if(debug){col = drawLine(col, vec3(1.0, 1.0, 1.0), coord, vec2(hit_point.x, hit_point.y), vec2((hit_point.x + hit_point_normal.x / 5.0), (hit_point.y + hit_point_normal.y / 5.0)), 0.0025);}

            // draw lens boundaries
            if (boundaryDrawCounter == 0){
                // top
                col = drawLine(col, green, coord, vec2(summedThickness, ld.lensAperture[i]/2.0), vec2(summedThickness + ld.lensThickness[i+1], ld.lensAperture[i]/2.0), lineWidth);

                if(ld.lensAperture[i] != ld.lensAperture[i+1]){
                    col = drawLine(col, green, coord, vec2(summedThickness + ld.lensThickness[i+1], ld.lensAperture[i]/2.0), vec2(summedThickness + ld.lensThickness[i+1], ld.lensAperture[i+1]/2.0), lineWidth);
                }

                // bottom
                col = drawLine(col, green, coord, vec2(summedThickness, - ld.lensAperture[i]/2.0), vec2(summedThickness + ld.lensThickness[i+1], - ld.lensAperture[i]/2.0), lineWidth);

                if(ld.lensAperture[i] != ld.lensAperture[i+1]){
                    col = drawLine(col, green, coord, vec2(summedThickness + ld.lensThickness[i+1], - ld.lensAperture[i]/2.0), vec2(summedThickness + ld.lensThickness[i+1], - ld.lensAperture[i+1]/2.0), lineWidth);
                }
            }


            // set hitpoint to be the new origin
            ray_origin = hit_point;

            // shoot off rays after last refraction
            if(i == 10){ // last element in array, change 10.0 to size() - 1
                ray_direction = calculateTransmissionVector(ld.lensIOR[i], 1.0, ray_direction, hit_point_normal);
                // remove
                col = drawLine(col, vec3(1.0, 1.0, 1.0), coord, vec2(ray_origin.x, ray_origin.y), vec2(ray_origin.x + ray_direction.x * 9999.0, ray_origin.y + ray_direction.y * 9999.0), lineWidth);

            }

        }
        // this is done automatically in path tracer, remove!
        ray_origin = ray_origin_original;

        // remove in path tracer
        boundaryDrawCounter = boundaryDrawCounter + 1;

    }




    // draw origin point
    col = drawDot(col, white, coord, vec2(0.0,0.0), pointWidth);

    // draw focus line
    col = drawLine(col, vec3(1.0, 1.0, 1.0), coord, vec2(objectDistance, 30.0), vec2(objectDistance, -30.0), lineWidth * 2.0);

    // draw film plane line
    col = drawLine(col, vec3(1.0, 1.0, 1.0), coord, vec2(0.0, 30.0), vec2(0.0, -30.0), lineWidth);

    // draw axial lines
    if(debug){col = drawLine(col, vec3(0.5, 1.0, 1.0), coord, vec2(0.0, 0.0), vec2(9999.0, 0.0), lineWidth / 2.0);}

    // draw line on height of rays that find principle planes
    if(debug){col = drawLine(col, vec3(1.0, 1.0, 0.5), coord, vec2(0.0, 5.0), vec2(9999.0, 5.0), lineWidth / 2.0);}
    if(debug){col = drawLine(col, vec3(1.0, 1.0, 0.5), coord, vec2(0.0, -5.0), vec2(9999.0, -5.0), lineWidth / 2.0);}

    // set fragcolor
    fragColor = vec4(col,1.0);

}
