# Albert Canal battery swap VR

The CMA CGM e-barge battery swap at BCTN Meerhout and the Kwaadmechelen lock on the Albert Canal, built on open survey data with live weather, grid and vessel data.

## The Unreal project is NOT in this branch

This branch holds only the **web version** (`index.html`, `assets/`), the **live ships job** (`tools/`, `.github/workflows/`) and a **copy of the Unreal C++ module** (`unreal/Source/`).

The full **Unreal Engine 5.8 project** (Content, Config, Source, the level), the **Blender files**, the **survey data** and the **ready Meta Quest 3 app** are on the `downloads` branch, split into zip parts:

1. Download **https://github.com/alicemitakoff/albert-canal-vr/archive/refs/heads/downloads.zip** (about 860 MB) and unzip it.
   Or: `git clone --depth 1 --branch downloads --single-branch https://github.com/alicemitakoff/albert-canal-vr AlbertCanal_download`
2. Double click **Unpack_Windows.bat** (Mac: **Unpack_Mac.command**). Everything lands in a folder called `AlbertCanal`.
3. The Unreal project is `AlbertCanal/AlbertCanalSwap/AlbertCanalSwap.uproject`. Add Cesium for Unreal 2.30.0 for UE 5.8 into `AlbertCanalSwap/Plugins/` (download `CesiumForUnreal-58-v2.30.0.zip` from https://github.com/CesiumGS/cesium-unreal/releases/tag/v2.30.0).
4. Right click the `.uproject`, generate project files, then open it and let it build.

The step by step guide (data pipeline, live APIs, Unreal build, Quest install) is the Claude Doc "Albert Canal battery swap VR: how to rebuild it".

## Links

### Frozen version (15 October 2026), do not change

| | |
| --- | --- |
| Laptop | https://alicemitakoff.github.io/albert-canal-vr/demo/ |
| Cinema (flat on the Quest) | https://alicemitakoff.github.io/albert-canal-vr/flat/ |
| Immersive (VR on Meta Quest 3) | https://alicemitakoff.github.io/albert-canal-vr/vr/ |

These three links and the files they load (`index.html`, `assets`, `demo`, `flat`, `vr` at the top of this repo) are frozen at the tag `frozen-2026-10-15`. The **Frozen links check** in the Actions tab turns red if any of them changes. `FROZEN.sha256` lists the fingerprint of every frozen file.

### New version (v2), work here

| | |
| --- | --- |
| Laptop | https://alicemitakoff.github.io/albert-canal-vr/v2/demo/ |
| Cinema (flat on the Quest) | https://alicemitakoff.github.io/albert-canal-vr/v2/flat/ |
| Immersive (VR on Meta Quest 3) | https://alicemitakoff.github.io/albert-canal-vr/v2/vr/ |

All new changes go in the `v2` folder only (`v2/index.html`, `v2/assets`).

## Run the web version locally

```bash
python -m http.server 8080
```

Then open http://localhost:8080 in Chrome.

## Live data

Open-Meteo (weather at the lock), Elia open data (Belgian wind, solar and demand) and AIS vessel positions via aisstream.io, collected every 10 minutes by `tools/ais_fetch.py` into the `live-data` branch. `tools/live_data_check.py` prints all five live values.

Orthofoto 2024 and DHM Vlaanderen II © Digitaal Vlaanderen (open data). Map data © OpenStreetMap contributors (ODbL).
