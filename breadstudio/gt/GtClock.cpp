#include "GtClock.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace gt {

namespace {

/* English invariant names, so a title reads the same on every PC (§4.5). */
const char *const WeekdayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *const WeekdayAbbrev[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *const MonthNames[] = {"January", "February", "March",     "April",   "May",      "June",
				  "July",    "August",   "September", "October", "November", "December"};
const char *const MonthAbbrev[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* The index a clock field reads, clamped so a malformed time cannot read out of bounds. */
int nameIndex(int value, int count)
{
	return value >= 0 && value < count ? value : 0;
}

std::string padded(int value, int digits)
{
	char buffer[16];
	std::snprintf(buffer, sizeof(buffer), "%0*d", digits, value);
	return buffer;
}

/* A run of decimal digits, so a span cannot hide a sign or trailing text. */
bool parseDigits(const std::string &text, long long &out)
{
	if (text.empty() || text.size() > 9) {
		return false;
	}
	long long value = 0;
	for (const char c : text) {
		if (c < '0' || c > '9') {
			return false;
		}
		value = value * 10 + (c - '0');
	}
	out = value;
	return true;
}

bool isSpecifier(char c)
{
	return c == 'd' || c == 'f' || c == 'F' || c == 'h' || c == 'H' || c == 'm' || c == 'M' || c == 's' || c == 't' ||
	       c == 'y';
}

/* One run of a repeated specifier character. Sets ok false when the run length
 * is not one .NET defines. */
std::string specifier(char c, int run, const GtClockTime &t, bool &ok, bool &fractions)
{
	switch (c) {
	case 'd':
		if (run == 1) {
			return std::to_string(t.day);
		}
		if (run == 2) {
			return padded(t.day, 2);
		}
		if (run == 3) {
			return WeekdayAbbrev[nameIndex(t.weekday, 7)];
		}
		if (run == 4) {
			return WeekdayNames[nameIndex(t.weekday, 7)];
		}
		break;
	case 'f':
	case 'F': {
		if (run > 7) {
			break;
		}
		fractions = true;
		/* millis as a three-digit fraction, then cut to `run` digits or padded with zeros. */
		char digits[4];
		std::snprintf(digits, sizeof(digits), "%03d", t.millis % 1000);
		std::string value = run <= 3 ? std::string(digits, run) : std::string(digits) + std::string(run - 3, '0');
		if (c == 'F') {
			while (!value.empty() && value.back() == '0') {
				value.pop_back();
			}
		}
		return value;
	}
	case 'h': {
		int hour = t.hour % 12;
		if (hour == 0) {
			hour = 12;
		}
		if (run == 1) {
			return std::to_string(hour);
		}
		if (run == 2) {
			return padded(hour, 2);
		}
		break;
	}
	case 'H':
		if (run == 1) {
			return std::to_string(t.hour);
		}
		if (run == 2) {
			return padded(t.hour, 2);
		}
		break;
	case 'm':
		if (run == 1) {
			return std::to_string(t.minute);
		}
		if (run == 2) {
			return padded(t.minute, 2);
		}
		break;
	case 'M':
		if (run == 1) {
			return std::to_string(t.month);
		}
		if (run == 2) {
			return padded(t.month, 2);
		}
		if (run == 3) {
			return MonthAbbrev[nameIndex(t.month - 1, 12)];
		}
		if (run == 4) {
			return MonthNames[nameIndex(t.month - 1, 12)];
		}
		break;
	case 's':
		if (run == 1) {
			return std::to_string(t.second);
		}
		if (run == 2) {
			return padded(t.second, 2);
		}
		break;
	case 't':
		if (run == 1) {
			return t.hour < 12 ? "A" : "P";
		}
		if (run == 2) {
			return t.hour < 12 ? "AM" : "PM";
		}
		break;
	case 'y':
		if (run == 1) {
			return std::to_string(t.year % 100);
		}
		if (run == 2) {
			return padded(t.year % 100, 2);
		}
		if (run == 3) {
			return padded(t.year % 100, 3);
		}
		if (run == 4) {
			return padded(t.year, 4);
		}
		break;
	}
	ok = false;
	return {};
}

/* Formats a multi-character .NET custom format. Reports whether it shows fractions. */
bool formatCustom(const std::string &format, const GtClockTime &t, std::string &out, bool &fractions)
{
	/* A one-letter format is a .NET standard format, which the builder does not support (§4.5). */
	if (format.size() == 1) {
		return false;
	}
	out.clear();
	bool ok = true;
	for (std::size_t i = 0; i < format.size() && ok;) {
		const char c = format[i];
		if (c == '\'' || c == '"') {
			/* A quoted literal. An unterminated quote is a format error. */
			const std::size_t end = format.find(c, i + 1);
			if (end == std::string::npos) {
				ok = false;
				break;
			}
			out.append(format, i + 1, end - i - 1);
			i = end + 1;
		} else if (c == '\\') {
			/* \x is the next character as a literal. A trailing backslash is an error. */
			if (i + 1 >= format.size()) {
				ok = false;
				break;
			}
			out.push_back(format[i + 1]);
			i += 2;
		} else if (isSpecifier(c)) {
			std::size_t run = 1;
			while (i + run < format.size() && format[i + run] == c) {
				run++;
			}
			out += specifier(c, static_cast<int>(run), t, ok, fractions);
			i += run;
		} else {
			/* : and / are literal, and so is anything .NET does not recognise. */
			out.push_back(c);
			i++;
		}
	}
	return ok;
}

} // namespace

GtClockTime gtNow()
{
	const auto now = std::chrono::system_clock::now();
	const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	const std::time_t seconds = static_cast<std::time_t>(millis / 1000);
	std::tm local{};
#if defined(_WIN32)
	localtime_s(&local, &seconds);
#else
	localtime_r(&seconds, &local);
#endif
	GtClockTime t{};
	t.year = local.tm_year + 1900;
	t.month = local.tm_mon + 1;
	t.day = local.tm_mday;
	t.hour = local.tm_hour;
	t.minute = local.tm_min;
	t.second = local.tm_sec;
	t.millis = static_cast<int>(millis % 1000);
	t.weekday = local.tm_wday; /* 0 = Sunday */
	return t;
}

bool gtFormatDate(const std::string &format, const GtClockTime &t, std::string &out)
{
	bool fractions = false;
	return formatCustom(format, t, out, fractions);
}

GtClockResult gtExpandClock(const std::string &text, const GtClockTime &now)
{
	GtClockResult result;
	result.text = text;

	std::string out;
	bool ticking = false;
	bool fractions = false;
	bool ok = true;
	for (std::size_t i = 0; i < text.size();) {
		const char c = text[i];
		if (c == '{') {
			if (i + 1 < text.size() && text[i + 1] == '{') {
				out.push_back('{'); /* {{ is an escape */
				i += 2;
				continue;
			}
			if (text.compare(i, 3, "{0:") != 0) {
				ok = false; /* any other {…} is not a clock */
				break;
			}
			/* Find the closing brace, honouring '…', "…" and \x inside the format. */
			std::size_t j = i + 3;
			char quote = 0;
			while (j < text.size()) {
				const char q = text[j];
				if (quote != 0) {
					if (q == quote) {
						quote = 0;
					}
				} else if (q == '\'' || q == '"') {
					quote = q;
				} else if (q == '\\') {
					j++;
				} else if (q == '}') {
					break;
				}
				j++;
			}
			if (j >= text.size()) {
				ok = false; /* an unmatched { */
				break;
			}
			std::string formatted;
			bool formatFractions = false;
			if (!formatCustom(text.substr(i + 3, j - i - 3), now, formatted, formatFractions)) {
				ok = false;
				break;
			}
			out += formatted;
			ticking = true;
			fractions = fractions || formatFractions;
			i = j + 1;
		} else if (c == '}') {
			if (i + 1 < text.size() && text[i + 1] == '}') {
				out.push_back('}'); /* }} is an escape */
				i += 2;
				continue;
			}
			ok = false; /* an unmatched } */
			break;
		} else {
			out.push_back(c);
			i++;
		}
	}

	if (!ok) {
		/* The whole text comes back as written, never thrown or blanked (Review Focus 3). */
		result.text = text;
		result.ticking = false;
		result.fractions = false;
		return result;
	}
	result.text = out;
	result.ticking = ticking;
	result.fractions = fractions;
	return result;
}

bool gtTimeSpanMs(const std::string &text, int64_t &ms)
{
	/* Split on ':' so "mm:ss" has two parts and "hh:mm:ss[.fff]" three. */
	std::vector<std::string> parts;
	std::size_t begin = 0;
	for (;;) {
		const std::size_t colon = text.find(':', begin);
		if (colon == std::string::npos) {
			parts.push_back(text.substr(begin));
			break;
		}
		parts.push_back(text.substr(begin, colon - begin));
		begin = colon + 1;
	}
	if (parts.size() != 2 && parts.size() != 3) {
		return false;
	}

	long long hours = 0, minutes = 0;
	std::size_t index = 0;
	if (parts.size() == 3) {
		if (!parseDigits(parts[0], hours)) {
			return false;
		}
		index = 1;
	}
	if (!parseDigits(parts[index], minutes)) {
		return false;
	}

	long long seconds = 0, millis = 0;
	const std::string &last = parts.back();
	const std::size_t dot = last.find('.');
	if (dot == std::string::npos) {
		if (!parseDigits(last, seconds)) {
			return false;
		}
	} else {
		if (!parseDigits(last.substr(0, dot), seconds)) {
			return false;
		}
		std::string fraction = last.substr(dot + 1);
		if (fraction.empty() || fraction.size() > 3) {
			return false;
		}
		while (fraction.size() < 3) {
			fraction.push_back('0');
		}
		if (!parseDigits(fraction, millis)) {
			return false;
		}
	}

	ms = ((hours * 60 + minutes) * 60 + seconds) * 1000 + millis;
	return true;
}

GtCountdown::GtCountdown(const GtCountdownSettings &settings) : settings_(settings)
{
	int64_t value = 0;
	startMs_ = gtTimeSpanMs(settings_.start, value) ? value : 0;
	durationMs_ = gtTimeSpanMs(settings_.duration, value) ? value : 0;
	if (startMs_ < 0) {
		startMs_ = 0;
	}
	if (durationMs_ < 0) {
		durationMs_ = 0;
	}
}

bool GtCountdown::active() const
{
	return durationMs_ > 0;
}

bool GtCountdown::running() const
{
	return state_ == State::Running;
}

int64_t GtCountdown::elapsedAt(int64_t nowMs) const
{
	int64_t elapsed = elapsedMs_;
	if (state_ == State::Running) {
		elapsed += nowMs - startedAtMs_;
	}
	return elapsed < 0 ? 0 : elapsed;
}

void GtCountdown::start(int64_t nowMs)
{
	if (state_ == State::Running) {
		return;
	}
	/* A pause resumes from where it held; a stop or a reset begins a fresh run. */
	if (state_ == State::Stopped || state_ == State::Idle) {
		elapsedMs_ = 0;
	}
	state_ = State::Running;
	startedAtMs_ = nowMs;
}

void GtCountdown::pause(int64_t nowMs)
{
	if (state_ == State::Running) {
		elapsedMs_ = elapsedAt(nowMs);
	}
	state_ = State::Paused;
}

void GtCountdown::stop(int64_t nowMs)
{
	if (state_ == State::Running) {
		elapsedMs_ = elapsedAt(nowMs);
	}
	state_ = State::Stopped;
}

void GtCountdown::reset()
{
	elapsedMs_ = 0;
	startedAtMs_ = 0;
	state_ = State::Idle;
}

std::string GtCountdown::text(int64_t nowMs) const
{
	const int64_t elapsed = elapsedAt(nowMs);
	const int64_t low = std::min(startMs_, durationMs_);
	const int64_t high = std::max(startMs_, durationMs_);

	/* Counting down reads from Duration to Start; Reverse reads from Start to Duration. */
	int64_t value = settings_.reverse ? startMs_ + elapsed : durationMs_ - elapsed;
	value = std::clamp(value, low, high);
	if (settings_.reverseDisplay) {
		value = low + high - value;
	}

	/* As a span, vMix formats it as a time of day (§4.5). */
	GtClockTime span{};
	const int64_t totalSeconds = value / 1000;
	span.millis = static_cast<int>(value % 1000);
	span.second = static_cast<int>(totalSeconds % 60);
	span.minute = static_cast<int>((totalSeconds / 60) % 60);
	span.hour = static_cast<int>((totalSeconds / 3600) % 24);

	std::string out;
	/* ponytail: a one-letter format gtFormatDate refuses falls back to HH:mm:ss, so the
	 * field always shows a reading; upgrade if vMix is seen to do something else. */
	if (!gtFormatDate(settings_.format, span, out)) {
		gtFormatDate("HH:mm:ss", span, out);
	}
	return out;
}

} // namespace gt
