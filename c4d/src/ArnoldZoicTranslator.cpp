#include "ArnoldZoicTranslator.h"

#include "ainode_zoic.h"

ArnoldZoicTranslator::ArnoldZoicTranslator(BaseList2D* node, RenderContext* context) : AbstractTranslator(ZOIC_TRANSLATOR, node, context)
{
}

char* ArnoldZoicTranslator::GetAiNodeEntryName()
{
   return "zoic";
}

void ArnoldZoicTranslator::InitSteps(int nsteps)
{
   // init all node array parameters
   AbstractTranslator::InitSteps(nsteps);

   BaseList2D* node = (BaseList2D*)GetC4DNode();
   if (!m_aiNode || !node) return;
}

void ArnoldZoicTranslator::Export(int step)
{
   // exports all node parameters
   AbstractTranslator::Export(step);

   BaseList2D* node = (BaseList2D*)GetC4DNode();
   if (!m_aiNode || !node) return;

   // first motion step
   if (step == 0)
   {
   }
}

