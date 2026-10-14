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

	/* one page and no list, for the base of an input that exists */
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
	QListWidget *list = nullptr;
	QStackedWidget *pages = nullptr;
	QPushButton *ok = nullptr;
	std::vector<InputPage *> pageOf; /* by kind; made when the kind is first shown */
};

} // namespace desk
