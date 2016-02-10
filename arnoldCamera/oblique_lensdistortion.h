/*
MUST DO with newer ldpk in public :
    In tde4_ldp public:
        bool setParameterValue2(const char *identifier,double v){return setParameterValue(identifier, v);}
        bool initializeParameters2(){return initializeParameters();}
        bool undistort2(double x0,double y0,double &x1,double &y1){return undistort(x0,y0,x1,y1);}
        //std::ostream& info(std::ostream& stream) const {return _.out(stream);}
    ldpk_plugin_loader
        comment the code but not include in ldpk_plugin_loader
    Where needed:
        + #define M_PI 3.14159265358979
        + change cout et cerr pour AiMsgInfo et AiMsgError
*/

#ifndef OBQLENSDISTORTION_H
#define OBQLENSDISTORTION_H

#include "O_Common.h"

// Enum for distortion model
//
enum ObqDistortionModel{NUKE,CLASSIC3DE,ANAMORPHIC6,FISHEYE8,STANDARD4,RADIAL_DECENTERED_CYLINDRIC4, ANAMORPHIC4, PFBARREL, NONE};


#endif //OBQUVREMAPLENSDISTORTION_H
