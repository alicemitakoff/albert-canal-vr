#!/bin/sh
# Unpacks the Albert Canal project into a folder called AlbertCanal next to this file.
cd "$(dirname "$0")" || exit 1
echo "Unpacking the Albert Canal project. This takes a few minutes..."
for z in parts/*.zip; do echo "$z"; unzip -oq "$z" -d AlbertCanal || { echo "Something went wrong with $z"; exit 1; }; done
A=AlbertCanal/AlbertCanal_Quest3_app
cat parts/AlbertCanalSwap-arm64.apk.0* > "$A/AlbertCanalSwap-arm64.apk"
cat parts/main.1.com.aimovation.albertcanalswap.obb.0* > "$A/main.1.com.aimovation.albertcanalswap.obb"
chmod +x "$A"/*.command "$A"/osx-x64/* 2>/dev/null
echo "Done. Everything is in the AlbertCanal folder next to this file."
