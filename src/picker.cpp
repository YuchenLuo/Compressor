/*
 * picker.cpp
 *
 *  Created on: 2013-04-15
 *      Author: luo
 */

#include <bb/cascades/Container>
#include <bb/cascades/Label>
#include <bb/cascades/Color>
#include "picker.h"
#include "audioproc.h"

#define PICKER_COLUMNS 8

namespace bb {
namespace cascades {

ValuePickerProvider::ValuePickerProvider() {
}

ValuePickerProvider::~ValuePickerProvider() {
}

// Pass in pickerList when ValuePickerProvider is called
VisualNode * ValuePickerProvider::createItem(Picker * pickerList, int columnIndex) {
    Container * returnItem = Container::create();
    Label * desc = Label::create().objectName("desc");
    returnItem->add(desc);
    return returnItem;
}

// Update pickerList for each entry in the picker
void ValuePickerProvider::updateItem(Picker * pickerList,
    int columnIndex, int rowIndex, VisualNode * pickerItem) {
    Container * container = (Container *)pickerItem;
    Label * desc = pickerItem->findChild<Label *>("desc");
    desc->setText( QString::number(rowIndex,16) );
    desc->textStyle()->setColor(Color::Black);
}

// Use columnCount() to return the number of columns
// in the picker
int ValuePickerProvider::columnCount() const {
    return PICKER_COLUMNS;
}

// Specify the range of the picker
void ValuePickerProvider::range(int column, int * lowerBoundary, int * upperBoundary) {
    *lowerBoundary = 0;
    *upperBoundary = 15;
}

// Customize the value of the picker based on selection
QVariant ValuePickerProvider::value(Picker * picker, const QList<int> & indices) const {
	QString stringValue = "";
	unsigned int value = 0;
	for( int i = 0; i < PICKER_COLUMNS; i++ )
	{
		stringValue = stringValue + QString::number(indices.value(i), 16);
		value = value | ( indices.value(i)&0xF ) << (28 - i*4);
	}
	ap->setCompression( (double)value );

	return QVariant(stringValue);
}

}
}


