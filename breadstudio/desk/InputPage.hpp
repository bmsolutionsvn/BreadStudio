#pragma once

#include <obs.hpp>

#include <QWidget>

namespace desk {

/* One page of the Input Select window. A page is made either for a new input, and then
 * create() runs on OK, or for the base of an input that exists, and then apply() runs on OK
 * and revert() on Cancel. */
class InputPage : public QWidget {
	Q_OBJECT

public:
	using QWidget::QWidget;

	/* false while OK must stay greyed out */
	virtual bool ready() const { return true; }

	/* Makes the new input and returns it. Null when it could not be made. */
	virtual OBSSource create() = 0;

	/* for an existing input: keep what was changed, or put back what was there */
	virtual void apply() {}
	virtual void revert() {}

signals:
	void readyChanged();
};

} // namespace desk
