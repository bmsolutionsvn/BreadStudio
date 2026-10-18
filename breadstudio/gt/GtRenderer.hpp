#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "GtDocument.hpp"

/* The Direct3D and Direct2D types the renderer is built on; kept out of this header's includes. */
struct ID3D11Device;
struct ID3D11Texture2D;

namespace gt {

struct GtPackage;

/* The renderer's Direct2D state; defined and used inside the module only. */
struct GtRendererImpl;

/* A warning the renderer raises (design §7); the kinds are the contract's. */
struct GtWarning {
	std::string kind;
	std::string detail;
};

/* The values drawn for one frame: clocks and countdowns are already expanded (contract). */
struct GtFrameState {
	std::map<std::string, std::string> text;    /* "T.Text" -> shown text */
	std::map<std::string, std::string> picture; /* "I.Source" -> resource name or file path; "" = nothing */
	std::map<std::string, uint32_t> colour;     /* "R.Fill.Color" -> argb */
	std::map<std::string, GtStyle> style;       /* "T.Text" -> override */
};

class GtRenderer {
public:
	GtRenderer();
	~GtRenderer();

	/* The device must have BGRA support; false leaves an error naming the failing step. */
	bool init(ID3D11Device *device, std::string &error);

	/* Loads the document and decodes the package's pictures. */
	void load(const GtPackage &package, const GtDocument &document);

	/* A picture field's pixels, already decoded off the graphics thread (§8). */
	void setPicture(const std::string &path, std::vector<uint8_t> bgraPremul, int w, int h);

	/* WIC decode to premultiplied BGRA; any thread, never the graphics thread (Review Focus 4). */
	static bool decodePicture(const std::string &path, std::vector<uint8_t> &bgraPremul, int &w, int &h);

	/* BGRA, document size, premultiplied, cleared to 0. The graphics thread only. */
	bool draw(ID3D11Texture2D *target, const GtFrameState &state);

	/* From load and the last draw, deduplicated (design §7). */
	std::vector<GtWarning> warnings() const;

private:
	GtRendererImpl *impl_;
};

} // namespace gt
