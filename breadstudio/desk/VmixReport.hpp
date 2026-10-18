#pragma once

#include "VmixPreset.hpp"

#include <QDialog>

#include <vector>

namespace desk {

/* The import report window (spec §5.3): every row the read and the build put on the
 * report — each naming the vMix input, what vMix had and why it was left out — shown
 * read-only, with a button that copies the same list to the clipboard as text so an
 * operator can keep it with the Project. It is the same window whether one input was
 * dropped or fifty, and an empty report says the whole preset came across. */
class VmixReport : public QDialog {
	Q_OBJECT

public:
	explicit VmixReport(const std::vector<VmixReportRow> &rows, QWidget *parent);
};

} // namespace desk
