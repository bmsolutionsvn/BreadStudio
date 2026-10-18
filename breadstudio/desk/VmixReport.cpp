#include "VmixReport.hpp"
#include "Desk.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <string>
#include <vector>

namespace desk {

VmixReport::VmixReport(const std::vector<VmixReportRow> &rows, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(text("VmixReport.Title"));

	auto *layout = new QVBoxLayout(this);

	/* what the window is for (spec §5.3): the rows below are differences, named, not
	 * silent changes */
	auto *invite = new QLabel(text("VmixReport.Invite"), this);
	invite->setWordWrap(true);
	layout->addWidget(invite);

	/* the same text the Copy button puts on the clipboard, so what is shown and what
	 * is copied cannot drift: the three headings, then one line per row, or — when
	 * there is nothing to report — the line that says the whole preset came across
	 * (§5.3). The body is the reader's own vmixReportText. */
	const QString body = QString::fromStdString(vmixReportText(rows));
	const QString shown =
		text("VmixReport.Columns") + QLatin1Char('\n') + (body.isEmpty() ? text("VmixReport.None") : body);

	auto *list = new QPlainTextEdit(this);
	list->setReadOnly(true);
	list->setPlainText(shown);
	layout->addWidget(list);

	auto *buttons = new QDialogButtonBox(this);
	QPushButton *copy = buttons->addButton(text("VmixReport.Copy"), QDialogButtonBox::ActionRole);
	QPushButton *close = buttons->addButton(QDialogButtonBox::Close);
	connect(copy, &QPushButton::clicked, this, [shown] { QApplication::clipboard()->setText(shown); });
	connect(close, &QPushButton::clicked, this, &QDialog::accept);
	layout->addWidget(buttons);

	setMinimumSize(600, 360);
}

} // namespace desk
