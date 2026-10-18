#!/usr/bin/env python3
# Writes the GT package fixtures GtPackageTest reads. Run:
#   python -I breadstudio/gt/test/fixtures/make-fixtures.py
# The four outputs are committed. Names, bytes and timestamps are fixed, so a
# re-run rewrites identical files and the diff stays empty.
#
# The minimal fixture's document.xml declares encoding="utf-16" but is written
# as UTF-8 bytes (vMix reads it as UTF-8; design 3.1), which is the point of the
# fixture: the reader keeps the UTF-8 bytes despite the declaration.

import base64
import os
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIXED_TIME = (2026, 10, 19, 0, 0, 0)

# A 1x1 opaque PNG: real picture bytes, so the thumbnail and the picture part
# hold a picture and not just filler.
PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="
)

DOCUMENT_XML = (
    '<?xml version="1.0" encoding="utf-16"?>\n'
    '<Composition Width="1920" Height="1080">\n'
    '\t<Layer Name="BG">\n'
    '\t\t<TextBlock Name="Name" Text="Nguyễn" />\n'
    '\t</Layer>\n'
    '</Composition>\n'
)

CONTENT_TYPES = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n'
    '\t<Default Extension="xml" ContentType="application/xml" />\n'
    '\t<Default Extension="png" ContentType="image/png" />\n'
    '</Types>\n'
)

# The resource filename keeps vMix's backslash; the picture part is stored as
# /GUID, which the reader finds from <source guid="GUID">.
GUID = "d3b07384-d9a0-4c9f-9a0e-2f3c4b5d6e7f"
PICTURE_NAME = "abc\\BG.png"

RESOURCES_XML = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<resources>\n'
    '\t<resource filename="abc\\BG.png">'
    '<source guid="d3b07384-d9a0-4c9f-9a0e-2f3c4b5d6e7f">abc\\BG.png</source>'
    '</resource>\n'
    '</resources>\n'
)

# Names a guid whose part is not in the package: a missing picture, not a
# broken title.
MISSING_RESOURCES_XML = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<resources>\n'
    '\t<resource filename="gone.png">'
    '<source guid="00000000-0000-0000-0000-000000000000">gone.png</source>'
    '</resource>\n'
    '</resources>\n'
)


def write_package(path, compression, entries):
    with zipfile.ZipFile(path, "w", compression) as package:
        for name, data in entries:
            info = zipfile.ZipInfo(name, FIXED_TIME)
            info.compress_type = compression
            info.external_attr = 0o600 << 16
            package.writestr(info, data)


def full_entries():
    return [
        ("[Content_Types].xml", CONTENT_TYPES.encode("utf-8")),
        ("document.xml", DOCUMENT_XML.encode("utf-8")),
        ("resources.xml", RESOURCES_XML.encode("utf-8")),
        ("thumbnail.png", PNG),
        ("/" + GUID, PNG),
    ]


def main():
    write_package(os.path.join(HERE, "minimal.gtzip"), zipfile.ZIP_STORED, full_entries())
    write_package(os.path.join(HERE, "deflated.gtzip"), zipfile.ZIP_DEFLATED, full_entries())
    write_package(
        os.path.join(HERE, "missing-part.gtzip"),
        zipfile.ZIP_STORED,
        [
            ("[Content_Types].xml", CONTENT_TYPES.encode("utf-8")),
            ("document.xml", DOCUMENT_XML.encode("utf-8")),
            ("resources.xml", MISSING_RESOURCES_XML.encode("utf-8")),
        ],
    )
    with open(os.path.join(HERE, "not-a-zip.gtzip"), "wb") as not_zip:
        not_zip.write(b"hello")
    print("wrote fixtures to " + HERE)


if __name__ == "__main__":
    main()
