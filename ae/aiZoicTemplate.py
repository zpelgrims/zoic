import mtoa.ui.ae.templates as templates
import pymel.core as pm
import maya.cmds as cmds
import mtoa.ui.ae.utils as aeUtils

class aiZoicTemplate(templates.AttributeTemplate):

    def filenameEdit(self, mData) :
            attr = self.nodeAttr('aiBokehPath')
            cmds.setAttr(attr,mData,type="string")

    def LoadFilenameButtonPush(self, *args):
        basicFilter = 'All Files (*.*)'
        ret = cmds.fileDialog2(fileFilter=basicFilter, dialogStyle=2, cap='Load File',okc='Load',fm=4)
        if ret is not None and len(ret):
            self.filenameEdit(ret[0])
            cmds.textFieldButtonGrp("filenameGrp", edit=True, text=ret[0])

    def filenameNew(self, nodeName):
        path = cmds.textFieldButtonGrp("filenameGrp", label="Bokeh image location", changeCommand=self.filenameEdit, width=300)
        cmds.textFieldButtonGrp(path, edit=True, text=cmds.getAttr(nodeName))
        cmds.textFieldButtonGrp(path, edit=True, buttonLabel="...",
        buttonCommand=self.LoadFilenameButtonPush)

    def filenameReplace(self, nodeName):
        cmds.textFieldButtonGrp("filenameGrp", edit=True, text=cmds.getAttr(nodeName) )



    def setup(self):
        self.addControl("aiSensorWidth", label="Sensor Width (cm)")
        self.addControl("aiSensorHeight", label="Sensor Height (cm)")
        self.addControl("aiFocalLength", label="Focal Length (mm)")
        self.addSeparator()
        self.addControl("aiUseDof", label="Enable depth of field")
        self.addControl("aiFStop", label="F-stop")
        self.addControl("aiFocalDistance", label="Focus distance (cm)")
        self.addSeparator()
        self.addControl("aiOpticalVignetting", label="Optical Vignetting")
        self.addControl("aiHighlightWidth", label="Highlight Width")
        self.addControl("aiHighlightStrength", label="Highlight Strength")
        self.addSeparator()
        self.addControl("aiUseImage", label="Enable Image based bokeh")
        self.addCustom('aiBokehPath', self.filenameNew, self.filenameReplace)
        self.addSeparator()
        self.addControl("aiExposureControl", label="Exposure")

templates.registerTranslatorUI(aiZoicTemplate, "camera", "zoic")
