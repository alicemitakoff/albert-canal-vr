# Albert Canal battery swap VR: full project download

Everything needed to recreate the CMA CGM e-barge battery swap at BCTN Meerhout and the Kwaadmechelen lock: the Unreal Engine 5.8 project, the Blender files and data scripts, the survey data and the ready Meta Quest 3 app.

1. **Download** this branch: [one click zip](https://github.com/alicemitakoff/albert-canal-vr/archive/refs/heads/downloads.zip) (about 1 GB), or
   `git clone --depth 1 --branch downloads --single-branch https://github.com/alicemitakoff/albert-canal-vr AlbertCanal_download`
2. **Unpack**: on Windows double click `Unpack_Windows.bat`, on a Mac double click `Unpack_Mac.command`. Everything lands in a folder called `AlbertCanal`.
3. Follow the recreation guide.

| Inside `AlbertCanal` | What it is |
| --- | --- |
| `AlbertCanalSwap/` | Unreal 5.8 project (Content, Config, Source). Add Cesium for Unreal 2.30.0 to `Plugins/` |
| `Cesium_Mac_patch_only/` | three Cesium source files with compiler fixes, only needed on a Mac |
| `*.blend`, `exports/` | Blender site, barge, hub and lock, plus the FBX, GLB and tree files sent to Unreal |
| `scripts/` | data pipeline and export scripts |
| `data/` | raw survey downloads: orthophoto tiles, terrain, OpenStreetMap extracts, canal centreline |
| `AlbertCanal_Quest3_app/` | ready app for Meta Quest 3 with installers for Windows and Mac |

The web version and the live data job are on the `main` branch.

Survey data: Orthofoto 2024 and DHM Vlaanderen II © Digitaal Vlaanderen; map data © OpenStreetMap contributors.
