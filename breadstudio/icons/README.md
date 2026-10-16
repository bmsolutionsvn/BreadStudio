# BreadStudio icons

`make-icons.ps1` draws the mark and writes `breadstudio.ico` (16, 32, 48, 64,
256 px) plus the three PNGs OBS's interface loads from `frontend/forms/images/`
(`obs.png`, `obs_paused.png`, `tray_active.png`). Run it after any change to the
drawing.

The mark is a **placeholder**: a dark rounded square with "BM" in cyan,
reproducing `bm-landing/public/favicon.svg`. It is not the BreadStudio logo.
The owner supplies the real art later; change only the drawing in
`make-icons.ps1` and re-run.
