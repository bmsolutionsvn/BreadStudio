#include "Project.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace desk {

namespace {

/* the base folder OBS keeps its profiles and scene collections under, from
 * global.ini; empty when the frontend does not answer */
std::string locationBase(const char *key)
{
	config_t *app = obs_frontend_get_app_config();
	if (!app) {
		return {};
	}
	const char *value = config_get_string(app, "Locations", key);
	if (!value) {
		value = config_get_default_string(app, "Locations", key);
	}
	return value ? std::string(value) : std::string();
}

/* "<base>/obs-studio/basic/scenes", where OBS keeps one .json per collection */
std::string scenesFolder()
{
	return locationBase("SceneCollections") + "/obs-studio/basic/scenes";
}

/* the live profile's folder; empty when the frontend is not ready */
std::string currentProfileDir()
{
	try {
		char *raw = obs_frontend_get_current_profile_path();
		const std::string folder = raw ? raw : "";
		bfree(raw);
		return folder;
	} catch (const std::exception &) {
		/* obs_frontend_get_current_profile_path has no catch of its own
		 * (OBSStudioAPI.cpp:184): with no valid profile it throws rather than
		 * returning null, so a not-ready frontend is answered with "" */
		return {};
	}
}

/* the live collection's file, from the user config's file name and the scenes folder */
std::string currentCollectionFile()
{
	config_t *user = obs_frontend_get_user_config();
	const char *fileName = user ? config_get_string(user, "Basic", "SceneCollectionFile") : nullptr;
	if (!fileName || !*fileName) {
		return {};
	}
	return scenesFolder() + "/" + fileName;
}

/* the live profile's name; empty when the frontend is not ready */
std::string currentProfileName()
{
	try {
		char *raw = obs_frontend_get_current_profile();
		const std::string name = raw ? raw : "";
		bfree(raw);
		return name;
	} catch (const std::exception &) {
		/* obs_frontend_get_current_profile has no catch of its own
		 * (OBSStudioAPI.cpp:178); a frontend with no valid profile throws */
		return {};
	}
}

/* the live collection's name; empty when the frontend is not ready */
std::string currentCollectionName()
{
	char *raw = obs_frontend_get_current_scene_collection();
	const std::string name = raw ? raw : "";
	bfree(raw);
	return name;
}

std::vector<uint8_t> readBytes(const std::string &path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return {};
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool writeBytes(const std::string &path, const std::vector<uint8_t> &bytes, std::string &error)
{
	std::error_code code;
	const std::filesystem::path target(path);
	if (target.has_parent_path()) {
		std::filesystem::create_directories(target.parent_path(), code);
	}
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		error = "cannot write " + path;
		return false;
	}
	if (!bytes.empty()) {
		file.write((const char *)bytes.data(), (std::streamsize)bytes.size());
	}
	if (!file) {
		error = "cannot write " + path;
		return false;
	}
	return true;
}

std::vector<uint8_t> asBytes(const std::string &text)
{
	return std::vector<uint8_t>(text.begin(), text.end());
}

bool startsWith(const std::string &text, const std::string &prefix)
{
	return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string pathFileName(const std::string &path)
{
	const size_t at = path.find_last_of("/\\");
	return at == std::string::npos ? path : path.substr(at + 1);
}

std::string pathStem(const std::string &path)
{
	std::string file = pathFileName(path);
	const size_t dot = file.find_last_of('.');
	if (dot != std::string::npos) {
		file = file.substr(0, dot);
	}
	return file.empty() ? "Untitled" : file;
}

/* every file under `dir`, as container paths "profile/<relative>" */
void collectProfileFiles(const std::string &dir, std::vector<ProjectEntry> &entries)
{
	std::error_code code;
	if (!std::filesystem::is_directory(dir, code)) {
		return;
	}
	for (const std::filesystem::directory_entry &item : std::filesystem::recursive_directory_iterator(dir, code)) {
		if (code) {
			break;
		}
		if (!item.is_regular_file(code)) {
			continue;
		}
		const std::filesystem::path relative = std::filesystem::relative(item.path(), dir, code);
		if (code) {
			continue;
		}
		entries.push_back(
			ProjectEntry{"profile/" + relative.generic_string(), readBytes(item.path().string())});
	}
}

/* the string values of OBS's file settings in a scene collection, in the order they
 * appear: what a Media or Image input was pointed at */
std::vector<std::string> mediaPaths(const std::string &json)
{
	std::vector<std::string> paths;
	for (const char *key : {"local_file", "file"}) {
		const std::string needle = "\"" + std::string(key) + "\"";
		size_t at = 0;
		while ((at = json.find(needle, at)) != std::string::npos) {
			size_t cursor = at + needle.size();
			while (cursor < json.size() && std::isspace((unsigned char)json[cursor]) != 0) {
				cursor++;
			}
			if (cursor >= json.size() || json[cursor] != ':') {
				at += needle.size();
				continue;
			}
			cursor++;
			while (cursor < json.size() && std::isspace((unsigned char)json[cursor]) != 0) {
				cursor++;
			}
			if (cursor >= json.size() || json[cursor] != '"') {
				at += needle.size();
				continue;
			}
			cursor++;
			std::string value;
			while (cursor < json.size() && json[cursor] != '"') {
				if (json[cursor] == '\\' && cursor + 1 < json.size() &&
				    (json[cursor + 1] == '\\' || json[cursor + 1] == '"')) {
					value.push_back(json[cursor + 1]);
					cursor += 2;
				} else {
					value.push_back(json[cursor++]);
				}
			}
			if (!value.empty()) {
				paths.push_back(value);
			}
			at = cursor + 1;
		}
	}
	return paths;
}

/* a profile's service.json with the provider, the server and the key blanked, so a
 * copy that leaves out the stream key carries no stream details (P7). OBS writes
 * "service", "server" and "key" in the settings object of that file. */
std::string withoutStreamKey(const std::string &json)
{
	std::string out = json;
	for (const char *key : {"service", "server", "key"}) {
		const std::string needle = "\"" + std::string(key) + "\"";
		size_t at = 0;
		while ((at = out.find(needle, at)) != std::string::npos) {
			size_t cursor = at + needle.size();
			while (cursor < out.size() && std::isspace((unsigned char)out[cursor]) != 0) {
				cursor++;
			}
			if (cursor >= out.size() || out[cursor] != ':') {
				at += needle.size();
				continue;
			}
			cursor++;
			while (cursor < out.size() && std::isspace((unsigned char)out[cursor]) != 0) {
				cursor++;
			}
			if (cursor >= out.size() || out[cursor] != '"') {
				at += needle.size();
				continue;
			}
			const size_t open = cursor;
			size_t close = open + 1;
			while (close < out.size() && out[close] != '"') {
				if (out[close] == '\\') {
					close++;
				}
				close++;
			}
			if (close >= out.size()) {
				break;
			}
			out.erase(open + 1, close - open - 1);
			at = open + 2;
		}
	}
	return out;
}

std::vector<std::string> stringList(char **list)
{
	std::vector<std::string> out;
	if (!list) {
		return out;
	}
	for (char **item = list; *item; item++) {
		out.emplace_back(*item);
	}
	bfree(list);
	return out;
}

/* `wanted`, or the first "wanted 2", "wanted 3" not already in `taken`, so an open
 * never overwrites a profile or a collection the machine already holds (spec §8) */
std::string uniqueName(const std::string &wanted, const std::vector<std::string> &taken)
{
	const std::string base = wanted.empty() ? "Project" : wanted;
	std::string candidate = base;
	for (int n = 2; std::find(taken.begin(), taken.end(), candidate) != taken.end(); n++) {
		candidate = base + " " + std::to_string(n);
	}
	return candidate;
}

/* a name for the packed media that is unique inside the container */
std::string uniqueMediaName(const std::string &file, std::vector<std::string> &used)
{
	std::string candidate = file.empty() ? "media" : file;
	std::string stem = candidate;
	std::string extension;
	const size_t dot = candidate.find_last_of('.');
	if (dot != std::string::npos) {
		stem = candidate.substr(0, dot);
		extension = candidate.substr(dot);
	}
	for (int n = 2; std::find(used.begin(), used.end(), candidate) != used.end(); n++) {
		candidate = stem + " " + std::to_string(n) + extension;
	}
	used.push_back(candidate);
	return candidate;
}

} // namespace

std::string Project::path() const
{
	return path_;
}

std::string Project::name() const
{
	if (path_.empty()) {
		return "Untitled";
	}
	std::string file = pathFileName(path_);
	const size_t dot = file.find_last_of('.');
	if (dot != std::string::npos) {
		file = file.substr(0, dot);
	}
	return file.empty() ? "Untitled" : file;
}

bool Project::changed() const
{
	return changed_;
}

void Project::markChanged()
{
	changed_ = true;
}

void Project::noteObSave(bool saving)
{
	if (!saving) {
		/* the same callback fires on a load; a load is not a change */
		return;
	}
	/* obs_frontend_save() is queued, so the callback for a save the desk asked for
	 * arrives after the desk cleared the flag. Count those and absorb them, so a
	 * desk write does not look like an operator edit. A genuine edit sets the flag
	 * through its own save; the only way to lose one is an edit and a desk save
	 * coalesced into the single write OBS makes for a turn, which the next edit
	 * re-marks. */
	if (selfSaves_ > 0) {
		selfSaves_--;
		return;
	}
	markChanged();
}

void Project::flushToObs()
{
	/* the count is raised before the queued save so its callback, whenever it runs,
	 * is taken for the desk's */
	selfSaves_++;
	obs_frontend_save();
}

void Project::createNew()
{
	/* the outgoing pair is written as OBS switches off it (ActivateProfile saves the
	 * active profile at OBSBasic_Profiles.cpp:690, ActivateSceneCollection saves the
	 * active collection at OBSBasic_SceneCollections.cpp:776), so creating the new
	 * profile and collection cannot drop work the operator has not saved */
	const std::string previousProfile = currentProfileName();
	const std::string profileName = uniqueName("Untitled", stringList(obs_frontend_get_profiles()));
	const std::string collectionName = uniqueName("Untitled", stringList(obs_frontend_get_scene_collections()));

	obs_frontend_create_profile(profileName.c_str());
	if (currentProfileName() != profileName) {
		/* nothing was created; the live profile is untouched */
		return;
	}
	if (!obs_frontend_add_scene_collection(collectionName.c_str()) || currentCollectionName() != collectionName) {
		/* the profile did change; put the live pair back before giving up */
		if (!previousProfile.empty()) {
			obs_frontend_set_current_profile(previousProfile.c_str());
		}
		return;
	}

	path_.clear();
	changed_ = false;
}

bool Project::open(const std::string &path, std::string &error)
{
	/* the container is read and its manifest parsed before anything is switched, so
	 * a file that is not a Project leaves the live profile and collection untouched
	 * (spec §8) */
	std::vector<ProjectEntry> entries;
	if (!readProjectFile(path, entries, error)) {
		return false;
	}

	std::string manifestText;
	std::vector<ProjectEntry> profileEntries;
	std::vector<uint8_t> scenes;
	bool hasScenes = false;
	for (const ProjectEntry &entry : entries) {
		if (entry.path == ManifestPath) {
			manifestText.assign(entry.bytes.begin(), entry.bytes.end());
		} else if (startsWith(entry.path, "profile/")) {
			profileEntries.push_back(entry);
		} else if (startsWith(entry.path, "scenes/") && !hasScenes) {
			scenes = entry.bytes;
			hasScenes = true;
		} else if (startsWith(entry.path, "media/")) {
			/* packed media is a copy for the operator to place by hand; nothing is
			 * unpacked and nothing replaces an input's own file */
			blog(LOG_INFO, "[bread-desk] the project carries media '%s'", entry.path.c_str());
		}
	}

	ProjectManifest manifest;
	if (!manifestFromJson(manifestText, manifest, error)) {
		return false;
	}
	if (!hasScenes) {
		error = path + " holds no scene collection";
		return false;
	}

	const std::string previousProfile = currentProfileName();
	const std::string previousCollection = currentCollectionName();
	const std::string profileName = uniqueName(manifest.project, stringList(obs_frontend_get_profiles()));
	const std::string collectionName =
		uniqueName(manifest.project, stringList(obs_frontend_get_scene_collections()));

	/* Neither create is trusted: obs_frontend_create_profile swallows its own failure
	 * (OBSBasic_Profiles.cpp:196) and obs_frontend_add_scene_collection returns a bool.
	 * If a create did not land, the live name still points at the operator's own
	 * profile or collection, and unpacking over it would destroy it - so each switch
	 * is proven before a single byte is written. Both creates are done before either
	 * is written, and neither is deferred. */
	obs_frontend_create_profile(profileName.c_str());
	if (currentProfileName() != profileName) {
		error = "could not create the profile '" + profileName + "'";
		return false;
	}
	if (!obs_frontend_add_scene_collection(collectionName.c_str()) || currentCollectionName() != collectionName) {
		/* the profile did change; put the live pair back as it was */
		if (!previousProfile.empty()) {
			obs_frontend_set_current_profile(previousProfile.c_str());
		}
		error = "could not create the scene collection '" + collectionName + "'";
		return false;
	}

	/* Both switches have landed and nothing of the container is written yet, so the
	 * folder OBS made and the collection file it owns are only now safe to write. The
	 * collection file is read while the new collection is the current one. */
	const std::string profileDir = currentProfileDir();
	const std::string expectedProfileDir = profileDirFor(locationBase("Profiles"), profileName);
	if (!expectedProfileDir.empty() && !profileDir.empty() && profileDir != expectedProfileDir) {
		blog(LOG_WARNING, "[bread-desk] OBS keeps the opened profile at '%s', not '%s'", profileDir.c_str(),
		     expectedProfileDir.c_str());
	}
	const std::string collectionFile = currentCollectionFile();
	const std::string expectedCollectionFile = sceneFileFor(locationBase("SceneCollections"), collectionName);
	if (!expectedCollectionFile.empty() && !collectionFile.empty() && collectionFile != expectedCollectionFile) {
		blog(LOG_WARNING, "[bread-desk] OBS keeps the opened collection at '%s', not '%s'",
		     collectionFile.c_str(), expectedCollectionFile.c_str());
	}
	if (profileDir.empty() || collectionFile.empty()) {
		error = "OBS did not name the profile or the scene collection it made";
		return false;
	}

	/* Both halves are unpacked with the same ordering: create, switch OFF the new one,
	 * write its bytes while it is inactive, switch back so OBS loads them.
	 *
	 * Profile: ActivateProfile saves the active, in-memory profile over its own
	 * basic.ini (OBSBasic_Profiles.cpp:690), ungated by defer_save, so writing while
	 * the new profile is active would be overwritten by OBS's empty defaults on the way
	 * out. Collection: ActivateSceneCollection saves the active collection on the way
	 * out too (SaveProjectNow, OBSBasic_SceneCollections.cpp:776), so writing the new
	 * collection's file while it is active would be overwritten by OBS's empty default
	 * scene. Writing between the away switch and the back switch puts the bytes where
	 * the next Load will read them, so after this call the live profile's files and the
	 * live collection's file on disk are the container's bytes.
	 *
	 * No defer_save is needed: that window suppresses OBS_FRONTEND_EVENT_SCENE_
	 * COLLECTION_CHANGED (OBSStudioAPI.cpp:747-750) while CLEANUP is still delivered,
	 * which would leave the desk unloaded after open; and it is what stopped the away
	 * switch from writing the outgoing collection. With the write moved to the inactive
	 * side, the away switch's own save persists the outgoing collection instead. */
	if (!previousProfile.empty() && previousProfile != profileName) {
		obs_frontend_set_current_profile(previousProfile.c_str());
	}
	for (const ProjectEntry &entry : profileEntries) {
		const std::string relative = entry.path.substr(std::string("profile/").size());
		if (!writeBytes(profileDir + "/" + relative, entry.bytes, error)) {
			return false;
		}
	}
	if (!previousProfile.empty() && previousProfile != profileName) {
		obs_frontend_set_current_profile(profileName.c_str());
	}

	if (!previousCollection.empty() && previousCollection != collectionName) {
		obs_frontend_set_current_scene_collection(previousCollection.c_str());
	}
	if (!writeBytes(collectionFile, scenes, error)) {
		return false;
	}
	if (!previousCollection.empty() && previousCollection != collectionName) {
		obs_frontend_set_current_scene_collection(collectionName.c_str());
	}

	path_ = path;
	changed_ = false;
	remember(path);
	return true;
}

bool Project::save(std::string &error)
{
	if (path_.empty()) {
		error = "the project has no file yet";
		return false;
	}
	return saveAs(path_, false, true, error);
}

bool Project::saveAs(const std::string &path, bool withMedia, bool withStreamKey, std::string &error)
{
	/* OBS writes the profile and the collection to its own files first, so the
	 * container holds what is on screen, not a stale copy */
	flushToObs();

	const std::string name = pathStem(path);
	std::vector<ProjectEntry> entries;
	entries.push_back(ProjectEntry{ManifestPath, asBytes(manifestJson(manifestFor(name)))});

	collectProfileFiles(currentProfileDir(), entries);

	const std::string collectionFile = currentCollectionFile();
	if (collectionFile.empty()) {
		error = "OBS did not name the current scene collection";
		return false;
	}
	entries.push_back(ProjectEntry{"scenes/" + pathFileName(collectionFile), readBytes(collectionFile)});

	if (withMedia) {
		const std::vector<uint8_t> sceneBytes = readBytes(collectionFile);
		const std::string sceneText(sceneBytes.begin(), sceneBytes.end());
		std::vector<std::string> used;
		for (const std::string &media : mediaPaths(sceneText)) {
			std::error_code code;
			if (!std::filesystem::is_regular_file(media, code)) {
				continue;
			}
			const std::string packed = uniqueMediaName(pathFileName(media), used);
			entries.push_back(ProjectEntry{"media/" + packed, readBytes(media)});
		}
	}

	if (!withStreamKey) {
		for (ProjectEntry &entry : entries) {
			if (entry.path == "profile/service.json" || entry.path == "profile/service.json.bak") {
				const std::string text(entry.bytes.begin(), entry.bytes.end());
				entry.bytes = asBytes(withoutStreamKey(text));
			}
		}
	}

	const std::string written = writeProjectFile(path, entries);
	if (!written.empty()) {
		error = written;
		return false;
	}

	path_ = path;
	changed_ = false;
	remember(path);
	return true;
}

std::vector<std::string> Project::recent() const
{
	std::vector<std::string> out;
	config_t *config = obs_frontend_get_user_config();
	const char *joined = config ? config_get_string(config, "BreadDesk", "RecentProjects") : nullptr;
	if (!joined) {
		return out;
	}
	const std::string text(joined);
	size_t at = 0;
	while (at < text.size() && out.size() < 10) {
		size_t end = text.find('\n', at);
		if (end == std::string::npos) {
			end = text.size();
		}
		if (end > at) {
			out.push_back(text.substr(at, end - at));
		}
		at = end + 1;
	}
	return out;
}

void Project::remember(const std::string &path)
{
	config_t *config = obs_frontend_get_user_config();
	if (!config) {
		return;
	}
	std::vector<std::string> list = recent();
	list.erase(std::remove(list.begin(), list.end(), path), list.end());
	list.insert(list.begin(), path);
	if (list.size() > 10) {
		list.resize(10);
	}
	std::string joined;
	for (size_t i = 0; i < list.size(); i++) {
		if (i != 0) {
			joined.push_back('\n');
		}
		joined += list[i];
	}
	config_set_string(config, "BreadDesk", "RecentProjects", joined.c_str());
	config_save_safe(config, "tmp", nullptr);
}

} // namespace desk
