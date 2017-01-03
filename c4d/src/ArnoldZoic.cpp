#include "ArnoldZoic.h"

#include "ainode_zoic.h"

Bool ArnoldZoic::Init(GeListNode* node)
{
   return ArnoldObjectData::Init(node);
}

const String ArnoldZoic::GetAiNodeEntryName(GeListNode* node)
{
   return String("zoic");
}

Bool ArnoldZoic::Message(GeListNode* node, Int32 type, void* data)
{
   return ArnoldObjectData::Message(node, type, data);
}

