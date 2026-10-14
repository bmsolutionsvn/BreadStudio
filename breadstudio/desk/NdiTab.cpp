#include "NdiTab.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"
#include "SourceList.hpp"
#include "SourceTiles.hpp"

#include <callback/calldata.h>
#include <callback/proc.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <string>

namespace desk {

/* the NDI plugin's values (DistroAV's) */
static const int BandwidthHighest = 0;
static const int BandwidthLowest = 1;
static const int BandwidthAudioOnly = 2;
static const int LatencyBuffered = 0;
static const int LatencyUnbuffered = 2;

/* "Also search" lives in the NDI plugin, which its finder uses (spec §4.1) */
static QString extraIps()
{
	calldata_t data;
	calldata_init(&data);
	QString ips;
	if (proc_handler_call(obs_get_proc_handler(), "bread_ndi_get_extra_ips", &data)) {
		const char *value = calldata_string(&data, "ips");
		ips = QString::fromUtf8(value ? value : "");
	}
	calldata_free(&data);
	return ips;
}

static void setExtraIps(const QString &ips)
{
	calldata_t data;
	calldata_init(&data);
	calldata_set_string(&data, "ips", ips.trimmed().toUtf8().constData());
	proc_handler_call(obs_get_proc_handler(), "bread_ndi_set_extra_ips", &data);
	calldata_free(&data);
}

NdiTab::NdiTab(QWidget *parent) : InputPage(parent)
{
	build();
	refresh();
}

NdiTab::NdiTab(obs_source_t *base_, QWidget *parent) : InputPage(parent), base(base_)
{
	build();

	/* obs_data_get_int gives the plugin's default for a value never saved, as a DistroAV collection may have */
	OBSDataAutoRelease now = obs_source_get_settings(base);
	tiles->choose(obs_data_get_string(now, "ndi_source_name"));
	const long long bandwidth = obs_data_get_int(now, "ndi_bw_mode");
	lowBandwidth->setChecked(bandwidth == BandwidthLowest);
	audioOnly->setChecked(bandwidth == BandwidthAudioOnly);
	bigBuffer->setChecked(obs_data_get_int(now, "latency") != LatencyUnbuffered);

	refresh();
}

void NdiTab::build()
{
	auto *layout = new QVBoxLayout(this);

	/* why NDI is off (spec §10); hidden while it is on */
	status = new QLabel(this);
	status->setWordWrap(true);
	status->setVisible(false);
	layout->addWidget(status);

	tiles = new SourceTiles(text("Ndi.None"), this);
	layout->addWidget(tiles, 1);
	connect(tiles, &SourceTiles::pickedChanged, this, &InputPage::readyChanged);

	auto *boxes = new QHBoxLayout();
	lowBandwidth = new QCheckBox(text("Ndi.LowBandwidth"), this);
	audioOnly = new QCheckBox(text("Ndi.AudioOnly"), this);
	bigBuffer = new QCheckBox(text("Ndi.BigBuffer"), this);
	boxes->addWidget(lowBandwidth);
	boxes->addWidget(audioOnly);
	boxes->addWidget(bigBuffer);
	boxes->addStretch(1);
	layout->addLayout(boxes);

	/* for senders on another subnet: NDI's finder asks these addresses directly (spec §4.1) */
	auto *row = new QHBoxLayout();
	row->addWidget(new QLabel(text("Ndi.AlsoSearch"), this));
	alsoSearch = new QLineEdit(extraIps(), this);
	alsoSearch->setPlaceholderText(text("Ndi.AlsoSearch.Hint"));
	row->addWidget(alsoSearch, 1);
	layout->addLayout(row);
	connect(alsoSearch, &QLineEdit::editingFinished, this, [this] { setExtraIps(alsoSearch->text()); });

	/* NDI's licence asks for its trademark line and a link near where NDI is chosen (spec §7.2) */
	auto *credit = new QLabel(text("Ndi.Credit"), this);
	credit->setWordWrap(true);
	credit->setOpenExternalLinks(true);
	layout->addWidget(credit);

	/* once a second while the window is open (spec §4.1) */
	connect(&timer, &QTimer::timeout, this, &NdiTab::refresh);
	timer.start(1000);
}

void NdiTab::refresh()
{
	tiles->setGroups(groupBySender(announced(TypeId, "ndi_source_name", nullptr)));

	const QString why = typeNote(TypeId, "status");
	status->setText(why);
	status->setVisible(!why.isEmpty());
}

OBSDataAutoRelease NdiTab::settings() const
{
	OBSDataAutoRelease result = obs_data_create();
	obs_data_set_string(result, "ndi_source_name", tiles->chosen().c_str());
	/* Audio Only wins over Low Bandwidth: NDI has one bandwidth setting */
	obs_data_set_int(result, "ndi_bw_mode",
			 audioOnly->isChecked()      ? BandwidthAudioOnly
			 : lowBandwidth->isChecked() ? BandwidthLowest
						     : BandwidthHighest);
	obs_data_set_int(result, "latency", bigBuffer->isChecked() ? LatencyBuffered : LatencyUnbuffered);
	return result;
}

bool NdiTab::ready() const
{
	return !tiles->chosen().empty();
}

OBSSource NdiTab::create()
{
	if (base || tiles->chosen().empty()) {
		return OBSSource(); /* nothing to make: the page is for a source that exists, or nothing is picked */
	}
	/* as vMix does, the input is named after its source */
	return createInput(TypeId, sourceName(tiles->chosen()).c_str(), settings());
}

/* only when something changed: a new source or bandwidth restarts the receiver */
void NdiTab::apply()
{
	if (!base || tiles->chosen().empty()) {
		return;
	}

	OBSDataAutoRelease now = obs_source_get_settings(base);
	OBSDataAutoRelease wanted = settings();
	if (std::string(obs_data_get_string(now, "ndi_source_name")) ==
		    obs_data_get_string(wanted, "ndi_source_name") &&
	    obs_data_get_int(now, "ndi_bw_mode") == obs_data_get_int(wanted, "ndi_bw_mode") &&
	    obs_data_get_int(now, "latency") == obs_data_get_int(wanted, "latency")) {
		return;
	}
	obs_source_update(base, wanted);
}

} // namespace desk
