#ifndef _aitag_zoic_h_
#define _aitag_zoic_h_

enum
{
   C4DAI_ZOIC_MAIN_GRP                                = 2001,
   C4DAI_ZOIC_MAIN_ATTRIBUTES_GRP                     = 3001,
   C4DAI_ZOIC_ADVANCED_GRP                            = 4001,

   C4DAIP_ZOIC_SENSORWIDTH                            = 1184180162,
   C4DAIP_ZOIC_SENSORHEIGHT                           = 168369253,
   C4DAIP_ZOIC_FOCALLENGTH                            = 860016785,
   C4DAIP_ZOIC_USEDOF                                 = 1290326798,
   C4DAIP_ZOIC_FSTOP                                  = 1021024428,
   C4DAIP_ZOIC_FOCALDISTANCE                          = 819210152,
   C4DAIP_ZOIC_OPTICALVIGNETTING                      = 158969043,
   C4DAIP_ZOIC_HIGHLIGHTWIDTH                         = 1715727008,
   C4DAIP_ZOIC_HIGHLIGHTSTRENGTH                      = 1587378065,
   C4DAIP_ZOIC_USEIMAGE                               = 717433016,
   C4DAIP_ZOIC_BOKEHPATH                              = 1001292574,
   C4DAIP_ZOIC_EXPOSURECONTROL                        = 1841413956,
   C4DAIP_ZOIC_POSITION                               = 2088783869,
   C4DAIP_ZOIC_LOOK_AT                                = 749503249,
   C4DAIP_ZOIC_UP                                     = 1114613555,
   C4DAIP_ZOIC_MATRIX                                 = 956486045,
   C4DAIP_ZOIC_NEAR_CLIP                              = 1830425835,
   C4DAIP_ZOIC_FAR_CLIP                               = 153831560,
   C4DAIP_ZOIC_SHUTTER_START                          = 870164764,
   C4DAIP_ZOIC_SHUTTER_END                            = 758053907,
   C4DAIP_ZOIC_SHUTTER_TYPE                           = 754576376,
   C4DAIP_ZOIC_SHUTTER_CURVE                          = 889084933,
   C4DAIP_ZOIC_ROLLING_SHUTTER                        = 1427317517,
   C4DAIP_ZOIC_ROLLING_SHUTTER_DURATION               = 606045970,
   C4DAIP_ZOIC_FILTERMAP                              = 202804212,
   C4DAIP_ZOIC_HANDEDNESS                             = 584272731,
   C4DAIP_ZOIC_TIME_SAMPLES                           = 1656296245,
   C4DAIP_ZOIC_SCREEN_WINDOW_MIN                      = 387664094,
   C4DAIP_ZOIC_SCREEN_WINDOW_MAX                      = 387664348,
   C4DAIP_ZOIC_EXPOSURE                               = 65342787,
   C4DAIP_ZOIC_NAME                                   = 1661319177,

   C4DAIP_ZOIC_SHUTTER_TYPE__BOX                      = 0,
   C4DAIP_ZOIC_SHUTTER_TYPE__TRIANGLE                 = 1,
   C4DAIP_ZOIC_SHUTTER_TYPE__CURVE                    = 2,

   C4DAIP_ZOIC_ROLLING_SHUTTER__OFF                   = 0,
   C4DAIP_ZOIC_ROLLING_SHUTTER__TOP                   = 1,
   C4DAIP_ZOIC_ROLLING_SHUTTER__BOTTOM                = 2,
   C4DAIP_ZOIC_ROLLING_SHUTTER__LEFT                  = 3,
   C4DAIP_ZOIC_ROLLING_SHUTTER__RIGHT                 = 4,

   C4DAIP_ZOIC_HANDEDNESS__RIGHT                      = 0,
   C4DAIP_ZOIC_HANDEDNESS__LEFT                       = 1,
};

#endif

