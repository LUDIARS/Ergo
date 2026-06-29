# Third-Party Notices

Ergo bundles (vendors) the following third-party components under `third_party/`.
Each is distributed under its own permissive license, reproduced or referenced below.

## earcut.hpp

- Source: https://github.com/mapbox/earcut.hpp
- Copyright (c) 2015, Mapbox
- License: **ISC**
- Full text: [`third_party/earcut/LICENSE`](third_party/earcut/LICENSE)

## nanosvg

- Source: https://github.com/memononen/nanosvg
- Copyright (c) 2013-2014 Mikko Mononen (memon@inside.org)
- License: **zlib**

## GoogleTest (gtest)

- Source: https://github.com/google/googletest
- Copyright Google Inc.
- License: **BSD-3-Clause**

---

These notices are provided to satisfy the attribution requirements of the above
licenses. The vendored copies are header-only / source-included and are compiled
statically into Ergo's libraries and tests.

## Managed (fetched) dependencies

The following are **not** vendored in this tree. They are fetched on demand from
pinned upstream revisions by the third-party dependency manager
(`third_party/dependencies.cmake`; see `spec/setup/third-party-deps.md`) and are
built only when their `ERGO_WITH_<NAME>` switch is enabled.

### kazmath

- Source: https://github.com/Kazade/kazmath
- Pin: commit `48dbc191da47880ea6708b0a7b3c7b69b6352cad`
- License: **BSD-2-Clause**
- Used via: `ERGO_WITH_KAZMATH=ON` → target `kazmath`

### curl / libcurl

- Source: https://github.com/curl/curl (https://curl.se)
- Pin: tag `curl-8_19_0`
- License: **curl** (MIT/X-derivative)
- Used via: `ERGO_WITH_CURL=ON` → target `CURL::libcurl`
