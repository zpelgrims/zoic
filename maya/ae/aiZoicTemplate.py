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
        self.addControl("aiFocalLength", label="Focal Length (cm)")
        self.addControl("aiFStop", label="F-stop")
        self.addControl("aiFocalDistance", label="Focus distance (cm)")
        self.addControl("aiLensModel", label="Lens Model")
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()

        self.beginLayout("Image based bokeh shape", collapse=False)
        self.addControl("aiUseImage", label="Enable Image based bokeh")
        self.addCustom("aiBokehPath", self.filenameNewBokeh, self.filenameReplaceBokeh)
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()

        self.beginLayout("Raytraced model", collapse=False)
        self.addCustom("aiLensDataPath", self.filenameNewLensData, self.filenameReplaceLensData)
        self.addControl("aiKolbSamplingLUT", label="Precalculate LUT")
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()

        self.beginLayout("Thin-lens model", collapse=False)
        self.addControl("aiUseDof", label="Enable thin-lens depth of field")
        self.addControl("aiOpticalVignettingDistance", label="Optical Vignetting Distance")
        self.addControl("aiOpticalVignettingRadius", label="Optical Vignetting Radius")
        self.endLayout()

        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()
        self.addSeparator()

        self.addControl("aiExposureControl", label="Exposure")



templates.registerTranslatorUI(aiZoicTemplate, "camera", "zoic")