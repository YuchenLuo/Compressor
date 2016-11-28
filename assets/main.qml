import bb.cascades 1.0
import custom.pickers 1.0

TabbedPane {
    showTabsOnActionBar: false
    Tab {
        title: "Compressor"
        imageSource: "asset:///compressor2.png"
        Page {
            Container {
                id: mainContainer
                objectName: "mainContainer"
                background: Color.create("#ffd55f00")
                Container {
                    topPadding: 20.0
                    Label {
                        text: "Compressor"
                        textStyle.fontSizeValue: 24.0
                        textStyle.fontWeight: FontWeight.Bold
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                    Divider {
                    }
                    Label {
                        text: "Threshold"
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                    Picker {
                        id: picker1
                        objectName: "picker1"
                        title: "Threshold Picker"
                        description: picker1.selectedValue
                        pickerItemProvider: ValuePickerProvider {
                            id: hexValueProvider
                        }
                        preferredRowCount: 2
                        kind: PickerKind.List
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                }
                    Divider {
                    }
                Container {
                    bottomPadding: 20.0
                    Label {
                        text: "Compression strength"
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                    Slider {
                        id: scaleSlider
                        objectName: "scaleSlider"
                        fromValue: 0
                        toValue: 1
                        value: 0.7
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                    /*
                    Label {
                        id: sliderValue
                        text: scaleSlider.value
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                    */
                    Divider {
                    }
                }

                /*
                 * Slider {
                 * id: distortionSlider
                 * objectName: "distSlider"
                 * fromValue: 0
                 * toValue: 10
                 * value: 0
                 * }
                 * Container {
                 * horizontalAlignment: HorizontalAlignment.Center
                 * Label {
                 * text: distortionSlider.value
                 * }
                 * }
                 */
            }
        }
    }
    Tab {
        title: "Distortion"
        Page {
            id: page2
            Container {
                id: page2Container
                background: Color.create("#ffd55f00")

                Container {
                    topPadding: 20.0
                    horizontalAlignment: HorizontalAlignment.Center
                    Label {
                        text: "Distortion"
                        textStyle.fontSizeValue: 24.0
                        textStyle.fontWeight: FontWeight.Bold
                        horizontalAlignment: HorizontalAlignment.Center
                    }
                }
                Divider {
                }
                Label {
                    text: "Samples compression threshold when turned on and sets clipping to occur at that level"
                    horizontalAlignment: HorizontalAlignment.Center
                    multiline: true
                }
                ToggleButton {
                    id: distortionToggle
                    objectName: "distortionToggle"
                }
                Divider {
                }
            }
        }
    }
}