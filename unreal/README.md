# Unreal Engine version

C++ module of the Unreal Engine 5.8 project `AlbertCanalSwap` (Cesium for Unreal 2.30, OpenXR, Meta Quest 3).

- `SwapExperience`: the guided battery swap story with 7 checkpoints, the swap crane, the battery bay on deck, the mitre gates of Kwaadmechelen lock, the departure from the quay, the wake, battery colours (red at 20% or less, orange below 60%, green from 60%) and live data (Open-Meteo, Elia, AIS).
- `SwapPawn`: viewer for Quest 3 (A/X or trigger continue, B/Y change place, left stick move, left grip fast, right stick turn and up/down, right grip pause) and laptop (Space, Tab, right mouse to look).
- `SiteTrees`: the 30,428 surveyed trees as instanced meshes.
- `Config/DefaultInput.ini`: the key and button mappings, including the Quest stick axes.

14 October 2026: the code now matches the web version (battery bay on deck, mitre gates, departure path, faster passages, wake, two VR places with smooth movement, story text).

To use it, copy `Source/AlbertCanalSwap/*` into `AlbertCanalSwap/Source/AlbertCanalSwap/` and `Config/DefaultInput.ini` into `AlbertCanalSwap/Config/` of the project from the `downloads` branch, then build. The 3D site and models live in the Blender and Unreal project files on the `downloads` branch.
