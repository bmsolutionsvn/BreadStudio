#pragma once

#include <obs.hpp>

#include <QDialog>

#include <vector>

class QListWidget;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace desk {

class InputPage;

/* The Input Select window: the kinds of input down the left, one page per kind (spec §3). */
class InputSelect : public QDialog {
	Q_OBJECT

public:
	/* the whole window, to make a new input */
	explicit InputSelect(QWidget *parent);

	/* the whole window, to change the source of an input that exists (vMix's Change): its own
	 * source first, edited in place, then every kind, whose OK puts a new source in place of the
	 * old base (BaseReplacement) */
	InputSelect(obs_source_t *input, QWidget *parent);

	/* the input this window is for; null when it makes a new one */
	obs_source_t *input() const { return existing; }

	void accept() override;
	void reject() override;

signals:
	void inputMade(obs_source_t *input);

private:
	void addButtons(QVBoxLayout *outer);
	void showKind();
	void syncOk();
	InputPage *page() const;

	OBSSource existing;
	InputPage *own = nullptr; /* the existing input's own source, edited in place */
	QListWidget *list = nullptr;
	QStackedWidget *pages = nullptr;
	QPushButton *ok = nullptr;
	std::vector<InputPage *> pageOf; /* by kind; made when the kind is first shown */
};

} // namespace desk
