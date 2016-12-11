import mtoa.ui.ae.templates as templates
import pymel.core as pm
import maya.cmds as cmds
import mtoa.ui.ae.utils as aeUtils

class aiZoicTemplate(templates.AttributeTemplate):

    def filenameEditBokeh(self, mData) :
        attr = self.nodeAttr('aiBokehPath')
        cmds.setAttr(attr,mData,type="string")

    def filenameEditLensData(self, mData) :
        attr = self.nodeAttr('aiLensDataPath')
        cmds.setAttr(attr,mData,type="string")

    def LoadFilenameButtonPushBokeh(self, *args):
        basicFilter = 'All Files (*.*)'
        ret = cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=2, cap='Load File',okc='Load',fm=4)
        if ret is not None and len(ret):
            self.filenameEditBokeh(ret[0])
            cmds.textFieldButtonGrp("filenameBokehGrp", edit=True, text=ret[0])

    def LoadFilenameButtonPushLensData(self, *args):
        basicFilter = 'All Files (*.*)'
        ret = cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=2, cap='Load File',okc='Load',fm=4)
        if ret is not None and len(ret):
            self.filenameEditLensData(ret[0])
            cmds.textFieldButtonGrp("filenameLensDataGrp", edit=True, text=ret[0])

    def filenameNewBokeh(self, nodeName):
        path = cmds.textFieldButtonGrp("filenameBokehGrp", label="Bokeh image location", changeCommand=self.filenameEditBokeh, width=300)
        cmds.textFieldButtonGrp(path, edit=True, text=cmds.getAttr(nodeName))
        cmds.textFieldButtonGrp(path, edit=True, buttonLabel="...",
        buttonCommand=self.LoadFilenameButtonPushBokeh)

    def filenameNewLensData(self, nodeName):
        path = cmds.textFieldButtonGrp("filenameLensDataGrp", label="Lens data location", changeCommand=self.filenameEditLensData, width=300)
        cmds.textFieldButtonGrp(path, edit=True, text=cmds.getAttr(nodeName))
        cmds.textFieldButtonGrp(path, edit=True, buttonLabel="...",
        buttonCommand=self.LoadFilenameButtonPushLensData)

    def filenameReplaceBokeh(self, nodeName):
        cmds.textFieldButtonGrp("filenameBokehGrp", edit=True, text=cmds.getAttr(nodeName) )

    def filenameReplaceLensData(self, nodeName):
        cmds.textFieldButtonGrp("filenameLensDataGrp", edit=True, text=cmds.getAttr(nodeName) )


    def setup(self):
        self.beginLayout("General", collapse=False)
        self.addControl("aiSensorWidth", label="Sensor Width (cm)")
        self.addControl("aiSensorHeight", label="Sensor Height (cm)")
        self.addControl("aiFocalLength", label="Focal Length (cm)", annotation="")
        self.addControl("aiFStop", label="F-stop")
        self.addControl("aiFocalDistance", label="Focus distance (cm)")
        self.addControl("aiLensModel", label="Lens Model", annotation="RAYTRACED:\n\n Reads in a lens data file and traces rays through that lens. \nThis model is generally preferred, but a bit slower. \nFeatures physically plausible optical vignetting and lens distortions. \n\n\nThin-Lens: \n\nThe classic lens approximation found in all renderers.\nFeatures emperical hack to achieve optical vignetting.")
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()


        self.beginLayout("Image based bokeh shape", collapse=False)
        self.addControl("aiUseImage", label="Enable Image based bokeh", annotation="Uses an image as bokeh shape. \n\nThis can be used with both models, but keep in mind that the raytraced model will already produce a non-constant bokeh shape due to the lens geometry.")
        self.addCustom("aiBokehPath", self.filenameNewBokeh, self.filenameReplaceBokeh, annotation="Path to bokeh shape image")
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()


        self.beginLayout("Raytraced model", collapse=False)
        self.addCustom("aiLensDataPath", self.filenameNewLensData, self.filenameReplaceLensData, annotation="Path to lens description file [.dat].")
        self.addControl("aiKolbSamplingLUT", label="Precalculate LUT", annotation="When enabling the LUT, zoic will precalculate the aperture shape at certain points on the sensor. \n\nThis speeds up the rendering process significantly, especially with small aperture sizes.")
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()


        self.beginLayout("Thin-lens model", collapse=False)
        self.addControl("aiUseDof", label="Enable thin-lens depth of field", annotation="Enable depth of field")
        self.addControl("aiOpticalVignettingDistance", label="Optical Vignetting Distance")
        self.addControl("aiOpticalVignettingRadius", label="Optical Vignetting Radius")
        self.addControl("aiHighlightWidth", label="Highlight Width")
        self.addControl("aiHighlightStrength", label="Highlight Strength")
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()

        self.addControl("aiExposureControl", label="Exposure", annotation="Multiplier on the ray weight")



templates.registerTranslatorUI(aiZoicTemplate, "camera", "zoic")
