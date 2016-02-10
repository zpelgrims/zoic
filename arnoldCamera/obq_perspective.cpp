#include "Obq_LensDistortion.h"

// Arnold thingy
//
AI_CAMERA_NODE_EXPORT_METHODS(ObqLensDistortionMethods);

// Param enum for fast direct access
//
enum Obq_LensDistortionParams { p_useDof, p_focusDistance, p_apertureSize, p_apertureAspectRatio, p_usePolygonalAperture, p_apertureBlades, p_apertureBladeCurvature, p_apertureRotation, p_focusPlaneIsPlane, p_bokehInvert, p_bokehBias, p_bokehGain, p_distortionModel, p_k1, p_k2, p_centerX, p_centerY, p_anamorphicSqueeze, p_asymmetricDistortionX,  p_asymmetricDistortionY, p_centerX3DEq, p_centerY3DEq,p_filmbackX3DEq, p_filmbackY3DEq,p_pixelRatio3DEq, p_c3dc00, p_c3dc01, p_c3dc02, p_c3dc03, p_c3dc04, p_ana6c00, p_ana6c01,p_ana6c02,p_ana6c03,p_ana6c04,p_ana6c05,p_ana6c06,p_ana6c07,p_ana6c08,p_ana6c09, p_ana6c10, p_ana6c11,p_ana6c12,p_ana6c13,p_ana6c14,p_ana6c15,p_ana6c16,p_ana6c17, p_fish8c00, p_fish8c01, p_fish8c02, p_fish8c03, p_stand4c00, p_stand4c01, p_stand4c02, p_stand4c03, p_stand4c04, p_stand4c05, p_raddec4c00, p_raddec4c01, p_raddec4c02, p_raddec4c03, p_raddec4c04, p_raddec4c05, p_raddec4c06, p_raddec4c07, p_ana4c00, p_ana4c01,p_ana4c02,p_ana4c03,p_ana4c04,p_ana4c05,p_ana4c06,p_ana4c07,p_ana4c08,p_ana4c09, p_ana4c10, p_ana4c11,p_ana4c12, p_focal3DEq, p_focusDistance3DEq,p_pfC3, p_pfC5, p_pfSqueeze, p_pfXp, p_pfYp, p_fov};


// Shader Data Structure
//
typedef struct
{
        float aspect;
        float width;
        float height;
        float pixelRatio;
        float tan_myFov;
        bool useDof;
        float focusDistance;
        bool focusPlaneIsPlane;
        float apertureSize;
}
ShaderData;

// Parameters
//
node_parameters
{
        AiParameterBOOL("useDof",false);
        AiParameterFLT("focusDistance",100.0f);
        AiParameterFLT("apertureSize",0.1f);

        AtArray* a = AiArray(2, 1, AI_TYPE_FLOAT, 0.0f,0.0f);
        AiParameterARRAY("fov",a);

        AiMetaDataSetBool(mds, NULL, "is_perspective", true);
}


node_initialize
{
        ShaderData *data = (ShaderData*) AiMalloc(sizeof(ShaderData));
        data->aspect = 1.0f;

        // Initialize
        data->aspect = 1.0f;
        data->width = 1920.0f;
        data->height = 1080.0f;
        data->pixelRatio = 1.0f;
        data->pixelOffset.x = 0.0f;//-0.5f/data->width;
        data->pixelOffset.y = 0.0f;//-0.5f/data->height;

        data->tan_myFov = 1.0f;
        data->useDof = false;
        data->apertureSize = 0.1f;
        data->focusDistance = 100.0f;
        data->focusPlaneIsPlane = true;

        // Set data
        AiCameraInitialize(node, data);
}

node_update
{
        AiCameraUpdate(node, false);

        ShaderData *data = (ShaderData*)AiCameraGetLocalData(node);

        // Update shaderData variables
        AtNode* options = AiUniverseGetOptions();
        data->width  = static_cast<float>(AiNodeGetInt(options,"xres"));
        data->height = static_cast<float>(AiNodeGetInt(options,"yres"));


        // Aspect
        data->pixelRatio = 1.0f/AiNodeGetFlt(options,"aspect_ratio");
        data->aspect = data->width/(data->height/data->pixelRatio);

        // Maybe not useful in this case
        data->pixelOffset.x = 0.0f;//-0.5f/data->width;
        data->pixelOffset.y = 0.0f;//-0.5f/data->height;

        // Field of view
        float fov = AiArrayGetFlt(AiNodeGetArray(node, "fov"),0);
        data->tan_myFov = static_cast<float>(std::tan(fov * c_Radians__d / 2.0));

        // DOF
        data->useDof = params[p_useDof].BOOL;

        // DOF related values
        data->focusDistance = params[p_focusDistance].FLT;
        data->focusPlaneIsPlane = params[p_focusPlaneIsPlane].BOOL;
        data->apertureSize = params[p_apertureSize].FLT;

        //AiMsgInfo("-------DEPTH OF FIELD---------");
        //AiMsgInfo("useDof = %s", (data->useDof?"True":"False"));
        //AiMsgInfo("focusDistance = %f", data->focusDistance);
        //AiMsgInfo("apertureSize = %f", data->apertureSize);
        //AiMsgInfo("------------------------------");

}

node_finish
{
        ShaderData *data = (ShaderData*)AiCameraGetLocalData(node);

        AiFree(data);
        AiCameraDestroy(node);
}

camera_create_ray
{
        // User params
        //const AtParamValue* params = AiNodeGetParams(node);

        // AspectRatio
        ShaderData *data = (ShaderData*)AiCameraGetLocalData(node);

        // Scale derivatives
        float dsx = input->dsx*data->tan_myFov;
        float dsy = input->dsy*data->tan_myFov;

        // Direction
        AtPoint p = { sx, sy, -1.0f/data->tan_myFov };
        output->dir = AiV3Normalize(p - output->origin);

        //////////////////
        // DEPTH OF FIELD
        /////////////////
        if(data->useDof && data->apertureSize > 0.0f)
        {
                float lensU = 0.0f, lensV = 0.0f;
                ConcentricSampleDisk(input->lensx, input->lensy, (data->usePolygonalAperture?data->apertureBlades:0), data->apertureBladeCurvature, data->apertureRotation,&lensU, &lensV, data->bokehInvert, data->bokehBias, data->bokehGain);
                lensU*=data->apertureSize;
                lensV*=data->apertureSize;
                float ft = ((data->focusPlaneIsPlane)?std::abs(data->focusDistance/output->dir.z):data->focusDistance);
                AtPoint Pfocus = output->dir*ft;

                // Focal Aspect Ratio
                lensV*=data->apertureAspectRatio;
                output->origin.x = lensU;
                output->origin.y = lensV;
                output->origin.z = 0.0;
                output->dir = AiV3Normalize(Pfocus - output->origin);
        }


        ///////////////////////////////////
        // Derivatives thanks to Alan King
        ///////////////////////////////////
        AtVector d = output->dir*std::abs(((-1.0f/data->tan_myFov)/output->dir.z));

        float d_dot_d = AiV3Dot(d, d);
        float temp = 1.0f / std::sqrt(d_dot_d * d_dot_d * d_dot_d);

        // Already initialized to 0's, only compute the non zero coordinates
        output->dDdx.x = (d_dot_d * dsx - (d.x * dsx) * d.x) * temp;
        output->dDdx.y = (              - (d.x * dsx) * d.y) * temp;
        output->dDdx.z = (              - (d.x * dsx) * d.z) * temp;

        output->dDdy.x = (              - (d.y * dsy) * d.x) * temp;
        output->dDdy.y = (d_dot_d * dsy - (d.y * dsy) * d.y) * temp;
        output->dDdy.z = (              - (d.y * dsy) * d.z) * temp;
}

//node_loader
//{
//	if (i != 0)
//		return FALSE;
//	node->methods     = ObqLensDistortionMethods;
//	node->output_type = AI_TYPE_UNDEFINED;
//	node->name        = "Obq_LensDistortion";
//	node->node_type   = AI_NODE_CAMERA;
//	strcpy(node->version, AI_VERSION);
//	return TRUE;
//}
