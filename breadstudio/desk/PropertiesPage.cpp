#include "PropertiesPage.hpp"
#include "Desk.hpp"
#include "InputModel.hpp"

#include <properties-view.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace desk {

PropertiesPage::PropertiesPage(const char *unversionedId, const char *nameKey_, QWidget *parent)
	: InputPage(parent),
	  typeId(unversionedId),
	  nameKey(nameKey_)
{
	const char *id = obs_get_latest_input_type_id(unversionedId);
	if (id) {
		OBSSourceAutoRelease standIn = obs_source_create_private(id, "bread input select", nullptr);
		source = standIn.Get();
	}
	build();
}

PropertiesPage::PropertiesPage(obs_source_t *existing, QWidget *parent) : InputPage(parent), source(existing)
{
	if (source) {
		OBSDataAutoRelease now = obs_source_get_settings(source);
		OBSDataAutoRelease copy = obs_data_create();
		obs_data_apply(copy, now);
		before = copy.Get();
	}
	build();
}

void PropertiesPage::build()
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	if (!source) {
		layout->addWidget(new QLabel(text("InputSelect.Unavailable"), this), 0, Qt::AlignTop);
		return;
	}

	/* the panel changes the source as the operator changes a setting, as OBS's window does */
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	auto *view =
		new OBSPropertiesView(settings.Get(), source.Get(), (PropertiesReloadCallback)obs_source_properties,
				      (PropertiesUpdateCallback) nullptr, (PropertiesVisualUpdateCb)obs_source_update);
	layout->addWidget(view);
}

OBSSource PropertiesPage::create()
{
	if (!source || before) {
		return OBSSource(); /* nothing to make: no such type, or the page is for a source that exists */
	}

	OBSDataAutoRelease now = obs_source_get_settings(source);
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_apply(settings, now);

	/* The stand-in goes first, and all the way: a capture device can be open in one
	 * source only. OBS destroys sources on another thread, so wait for it. */
	delete findChild<OBSPropertiesView *>();
	source = nullptr;
	obs_wait_for_destroy_queue();

	return createInput(typeId.c_str(), obs_module_text(nameKey.c_str()), settings);
}

/* as OBS's own properties window does on Cancel */
void PropertiesPage::revert()
{
	if (!source || !before) {
		return;
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);
	obs_data_clear(settings);
	obs_data_apply(settings, before);
	obs_source_update(source, before);
}

} // namespace desk
