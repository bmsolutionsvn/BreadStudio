# Product

<!-- impeccable:product-schema 1 -->

BreadStudio is part of the BM Solutions ecosystem. The shared product record is `../../PRODUCT.md`;
this file adds only what is specific to BreadStudio.

## Platform

web

BreadStudio is a Windows desktop program, a fork of OBS Studio (Qt UI, `BreadStudio.exe`). All
BreadStudio code lives under `breadstudio/`; every edit to an OBS file is listed in
`breadstudio/UPSTREAM-EDITS.md`, which `breadstudio/check-edits.ps1` enforces at build time so
upstream merges stay small.

## Users

- **Event production crews and streamers** who want one program that handles inputs, mixing,
  graphics and output without assembling OBS plugins by hand. They run it on the show PC, often
  next to OBS/vMix.

## Product Purpose

A broadcast "desk" built on OBS: an input bar and mixer with bus meters, a multiview, NDI, SRT
and OMT-HX inputs and outputs, and a graphics/titles layer (`gt`). It can run alongside stock
OBS (separate settings paths and single-instance mutex).

Roadmap link: a lighter replay built on the BreadReplay engine is planned for BreadStudio; the
dedicated-operator replay stays the separate BreadReplay product.

## Anti-goals

- Diverging from upstream OBS in files that are not listed in `UPSTREAM-EDITS.md`.
- A desk that needs plugin assembly or global audio-device setup before it is usable.
