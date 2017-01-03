#pragma once

#include "c4dtoa_api.h"
#include "c4dtoa_symbols.h"

#include "c4d.h"

// unique id obtained from http://www.plugincafe.com/forum/developer.asp 
#define ZOIC_ID {id}

class ArnoldZoic : public ArnoldObjectData
{
public:

   ///
   /// Constructor.
   ///
   static NodeData* Alloc()
   {
      return NewObjClear(ArnoldZoic);
   }

   ///
   /// C4D node initialization function.
   ///
   virtual Bool Init(GeListNode* node);

   ///
   /// Defines related Arnold node entry.
   ///
   virtual const String GetAiNodeEntryName(GeListNode* node);

   ///
   /// Event handler.
   ///
   virtual Bool Message(GeListNode* node, Int32 type, void* data);
};

static Bool RegisterArnoldZoic()
{
   return RegisterObjectPlugin(ZOIC_ID, "ArnoldZoic", 0, ArnoldZoic::Alloc, "ainode_zoic", 0, 0);
}

