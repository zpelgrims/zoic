#pragma once

#include "c4dtoa_api.h"
#include "c4dtoa_symbols.h"

#include "c4d.h"

// translator id
#define ZOIC_TRANSLATOR "ArnoldZoicTranslator"

class ArnoldZoicTranslator : public AbstractTranslator
{
public:

   ///
   /// Constructor.
   ///
   ArnoldZoicTranslator(BaseList2D* node, RenderContext* context);

   ///
   /// Returns the name of the Arnold node entry this translator
   /// is responsible to translate to.
   ///
   virtual char* GetAiNodeEntryName();

   ///
   /// Called before the export starts.
   /// Allocate motion blur dependent node parameter arrays here.
   ///
   /// @param nsteps: number of motion blur steps during the export.
   ///
   virtual void InitSteps(int nsteps);

   ///
   /// Translation logic.
   ///
   virtual void Export(int step);
};

