import mtoa.ui.ae.templates as templates
class aiZenoCameraTemplate(templates.AttributeTemplate):
    def setup(self):
        self.addControl("aiSensorWidth", label="Sensor Width")
        self.addControl("aiSensorHeight")
        self.addControl("aiFocalLength")
        self.addSeparator()
        self.addControl("aiFStop")
        self.addControl("aiFocalDistance")
        self.addControl("aiUseDof")
        self.addControl("aiOpticalVignet", label="Optical Vignetting")
        self.addControl("aiIso", label="Iso Rating")
        self.addControl("aiFilterMap", label="Filter Map")
templates.registerTranslatorUI(aiZenoCameraTemplate, "camera", "zenoCamera")
