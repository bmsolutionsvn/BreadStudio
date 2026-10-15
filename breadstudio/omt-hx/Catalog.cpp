#include "Catalog.hpp"
#include "Describe.hpp"

#include <obs-module.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

#include "omthx/omthx.h"

namespace omt {

namespace {

using Clock = std::chrono::steady_clock;

/* how long the check goes on after the last question */
constexpr auto Idle = std::chrono::seconds(5);
/* a source that sent nothing is asked again after this */
constexpr auto Retry = std::chrono::seconds(5);
/* "no signal": no picture within this (spec §4.2) */
constexpr int ProbeMs = 2000;

enum class Check { Waiting, Found, NoSignal };

struct Entry {
	Check check = Check::Waiting;
	omthx::SourceInfo info;
	Clock::time_point checkedAt;
};

std::mutex mutex;
std::condition_variable wake;
std::map<std::string, Entry> table; /* by address */
Clock::time_point asked;
std::thread worker;
bool stopping = false;

bool due(const Entry &entry, Clock::time_point now)
{
	return entry.check == Check::Waiting || (entry.check == Check::NoSignal && now - entry.checkedAt > Retry);
}

/* one probe at a time, never under the lock */
void work()
{
	std::unique_lock lock(mutex);
	while (!stopping) {
		if (Clock::now() - asked > Idle) {
			table.clear();
			wake.wait(lock, [] { return stopping || Clock::now() - asked <= Idle; });
			continue;
		}

		const Clock::time_point now = Clock::now();
		auto next = std::find_if(table.begin(), table.end(),
					 [now](const auto &entry) { return due(entry.second, now); });
		if (next == table.end()) {
			wake.wait_for(lock, std::chrono::milliseconds(250));
			continue;
		}

		const std::string address = next->first;
		lock.unlock();
		omthx::SourceInfo info;
		const bool found = omthx::probe(address, ProbeMs, info);
		lock.lock();

		auto entry = table.find(address);
		if (entry != table.end()) {
			entry->second.check = found ? Check::Found : Check::NoSignal;
			entry->second.info = info;
			entry->second.checkedAt = Clock::now();
		}
	}
}

std::string describe(const Entry &entry)
{
	switch (entry.check) {
	case Check::Found:
		return formatLine(entry.info);
	case Check::NoSignal:
		return obs_module_text("Detail.NoSignal");
	case Check::Waiting:
		break;
	}
	return obs_module_text("Detail.Checking");
}

} // namespace

std::vector<Seen> seen()
{
	std::vector<Seen> result;
	std::lock_guard lock(mutex);
	if (stopping) {
		return result;
	}

	const std::vector<std::string> addresses = omthx::discover();

	asked = Clock::now();
	if (!worker.joinable()) {
		worker = std::thread(work);
	}
	for (const std::string &address : addresses) {
		result.push_back({address, describe(table[address])});
	}
	wake.notify_one();
	return result;
}

void stopCatalog()
{
	{
		std::lock_guard lock(mutex);
		stopping = true;
	}
	wake.notify_one();
	if (worker.joinable()) {
		worker.join();
	}
}

} // namespace omt
