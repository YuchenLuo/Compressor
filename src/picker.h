/*
 * picker.h
 *
 *  Created on: 2013-04-15
 *      Author: luo
 */

#ifndef PICKER_H_
#define PICKER_H_

#include <bb/cascades/PickerProvider>

namespace bb {
namespace cascades {

class ValuePickerProvider: public bb::cascades::PickerProvider {
public:
	ValuePickerProvider();
    virtual ~ValuePickerProvider();
    VisualNode * createItem(Picker * pickerList, int columnIndex);
    void updateItem(Picker * pickerList, int columnIndex, int rowIndex,
    VisualNode * pickerItem);
    int columnCount() const;
    void range(int column, int * lowerBoundary, int * upperBoundary);
    QVariant value(Picker * picker, const QList<int> & indices) const;

};

}
}

#endif /* PICKER_H_ */
