// Default empty project template
#include "applicationui.hpp"
#include "audioproc.h"

#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>

#include <bb/cascades/Slider>
#include <bb/cascades/ToggleButton>
#include <bb/cascades/Page>

using namespace bb::cascades;

ApplicationUI::ApplicationUI(bb::cascades::Application *app)
: QObject(app)
{
    // create scene document from main.qml asset
    // set parent to created document to ensure it exists for the whole application lifetime
    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);

    // create root object for the UI
    AbstractPane *root = qml->createRootObject<AbstractPane>();
    qml->setContextProperty("injection", this);

    // Connect listener slots for QML objects
    QObject *scaleSlider = root->findChild<QObject*>("scaleSlider");
    if (scaleSlider){
    	bool res = QObject::connect( scaleSlider, SIGNAL(immediateValueChanged(float)),
    	                            this, SLOT(handleScaleSliderChange(float)));
    	Q_ASSERT(res);
    	Q_UNUSED(res);
    }else {
    	fprintf( stderr, "%s: Slider scaleSlider not found \n", __func__ );
    }
    /*
    QObject *distSlider = root->findChild<QObject*>("distSlider");
    if (distSlider){
    	bool res = QObject::connect( distSlider, SIGNAL(immediateValueChanged(float)),
    	                            this, SLOT(handleDistSliderChange(float)));
    	Q_ASSERT(res);
    	Q_UNUSED(res);
    }else {
    	fprintf( stderr, "%s: Slider distSlider not found \n", __func__ );
    }
    */
    QObject *distToggle = root->findChild<QObject*>("distortionToggle");
    if (distToggle){
    	bool res = QObject::connect( distToggle, SIGNAL(checkedChanged(bool)),
    	                            this, SLOT(handleDistToggleChange(bool)));
    	Q_ASSERT(res);
    	Q_UNUSED(res);
    }else {
    	fprintf( stderr, "%s: ToggleButton distToggle not found \n", __func__ );
    }


    // set created root object as a scene
    app->setScene(root);
}
void ApplicationUI::handleScaleSliderChange(float scale){
	ap->setScale( (double)scale );
}
void ApplicationUI::handleDistSliderChange(float dist){
	ap->setDistortion( (double)dist );
}
void ApplicationUI::handleDistToggleChange(bool  dist){
	ap->setDistortion( dist ? 10:0 );
}
