# Third-party notices

Namaz would not exist without the work of many other projects and people.

## Salaat Time 3.0

Namaz was inspired by **Salaat Time 3.0**, the original Windows prayer-time
application. We thank its authors for their work.  The decision to write a
free implementation was driven by two things: the original is closed-source,
and its prayer-time calculation and daily-use paths had critical bugs.

No source code, executables, audio recordings, city databases, icons or
screenshots from Salaat Time are distributed in this repository.  The public
build uses only the free assets listed below.

> Namaz is an independent project and is not affiliated with, endorsed by, or
> supported by the authors of Salaat Time.

May Allah reward the authors of Salaat Time for their intentions and work.

## Islamic Times Library (ITL)

`crates/prayer-core` is a pure-Rust port of the ITL prayer-time engine.
The original C sources are kept in `third_party/itl` for differential tests
and attribution.

- Copyright (c) 2003–2006, 2009–2010, Arabeyes, Thamer Mahmoud
- Licence: GNU Lesser General Public License 2.1 or later
- Upstream: https://www.arabeyes.org/

## GeoNames

`assets/cities.json` is generated from the GeoNames `cities5000` dump.

- Licence: Creative Commons Attribution 4.0 International (CC BY 4.0)
- Upstream: https://www.geonames.org/
- See `assets/cities.LICENSE`.

## Bundled audio

`assets/audio/*.ogg` are third-party recordings:

- `ADHAN.ogg` — adhan (call to prayer) by **Ahmad bin Abdul-Aziz Al-Nufais**
- `BISMILLAH.ogg`, `ALIHLAS.ogg` — Quran recitation by **Mishary Rashid
  Alafasy**

The recordings are freely available and may be used by anyone, but the
reciters did not state a licence.  See `assets/audio/audio.LICENSE`.

## Qt and KDE

Namaz is built with Qt 6 and follows the KDE Plasma human interface
guidelines.  Qt and the KDE Frameworks are used under their respective
licences (LGPL-3.0 / GPL-2.0+ / GPL-3.0).  They are not vendored in this
repository.

## cpal

`third_party/cpal` is a patched copy of cpal 0.15.3.  It is used through
`[patch.crates-io]` because rodio 0.20 still depends on the cpal 0.15 API.

- Copyright: the cpal contributors
- Licence: Apache-2.0 (see `third_party/cpal/LICENSE`)
- Upstream: https://github.com/RustAudio/cpal
- Local changes: see `third_party/cpal/PATCHES.md`

The patch stops the ALSA output worker from retrying a failed write forever,
so a lost device cannot spin at 100 % CPU or make `Stream::drop()` hang.

## Rust crates

The Rust dependencies are listed in `Cargo.lock`.  Their licences are
available from their respective authors; the majority are MIT or
Apache-2.0.  `packaging/cargo-sources.json` pins the exact crate versions and
checksums used by the Flatpak build.
