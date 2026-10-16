# BreadStudio icons

`make-icons.ps1` is the one art source. It draws the mark and writes four files:

- `breadstudio/icons/breadstudio.ico` — the program icon (16, 32, 48, 64, 256 px)
- `frontend/forms/images/obs.png` — the window, taskbar and About picture
- `frontend/forms/images/obs_paused.png` — the tray icon while recording is paused
- `frontend/forms/images/tray_active.png` — the tray icon while streaming or recording

The mark is a **placeholder**: a dark rounded square with "BM" in cyan,
reproducing `bm-landing/public/favicon.svg`. It is not the BreadStudio logo.
The owner supplies the real art later; change the drawing in `make-icons.ps1`
and re-run it, and all four files above are regenerated.
