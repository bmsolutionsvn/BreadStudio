#pragma once

#include "ProjectFile.hpp"

#include <string>
#include <vector>

namespace desk {

/* The Project the desk has open. One at a time (P2). */
class Project {
public:
	/* the open Project's path, empty for one never saved */
	std::string path() const;
	/* the name shown in the title bar: the file's stem, or "Untitled" */
	std::string name() const;
	/* true while it has changes not written (P9) */
	bool changed() const;

	/* a fresh, empty Project (P3). False, with `error` set, when OBS would not
	 * create the profile or the scene collection it needs: the live pair is left
	 * untouched, so a caller that would build into the Project must not. */
	bool createNew(std::string &error);
	/* reads `path` and switches the profile and the scene collection to what it holds */
	bool open(const std::string &path, std::string &error);
	/* writes to the open path, or asks for one when there is none */
	bool save(std::string &error);
	/* writes a copy at `path`, packing or leaving out the media per `withMedia`, and
	 * leaving the stream key out per `withStreamKey` (P6, P7) */
	bool saveAs(const std::string &path, bool withMedia, bool withStreamKey, std::string &error);

	/* the recent list, most recent first, at most ten (P8) */
	std::vector<std::string> recent() const;

	/* OBS's save callback calls this when the scene collection is written: the show
	 * has moved on since the .bstd was last written (P9). A save the desk asked for
	 * itself is absorbed, so the desk's own write does not bring the dot back. */
	void noteObSave(bool saving);

	/* the desk's own edits mark the Project changed */
	void markChanged();

private:
	/* asks OBS to write its own profile and collection, counting the write so its
	 * callback is taken for the desk's, not the operator's */
	void flushToObs();
	/* the write path, when the desk has one, is put at the head of the recent list */
	void remember(const std::string &path);

	std::string path_;
	bool changed_ = false;
	int selfSaves_ = 0;
};

/* Pure, and tested: the manifest for a Project whose name is `name`. */
ProjectManifest manifestFor(const std::string &name);
/* Pure, and tested: where a Project's profile and collection are unpacked inside
 * the app's own config, given the manifest (P4). */
std::string profileDirFor(const std::string &appData, const std::string &name);
std::string sceneFileFor(const std::string &appData, const std::string &name);

} // namespace desk
