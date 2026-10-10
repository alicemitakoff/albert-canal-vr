"""Call the five live sources behind the Albert Canal scene and print what the apps show.

No key and no install needed (Python 3.8 or newer):

    python scripts/data/live_data_check.py   (or python tools/live_data_check.py in the web repo)

Every value is printed with the time the source measured it, so you can see how fresh each feed is.
The web page (index.html, refreshLive and refreshAIS) and Unreal (SwapExperience.cpp, RefreshLive) make exactly these calls.
"""
import json, math, time, urllib.request
from datetime import datetime, timezone

METEO = ('https://api.open-meteo.com/v1/forecast?latitude=51.099&longitude=5.105'
         '&current=temperature_2m,wind_speed_10m,wind_direction_10m,cloud_cover,precipitation,is_day'
         '&wind_speed_unit=ms&timezone=Europe%2FBrussels')
ELIA = 'https://opendata.elia.be/api/explore/v2.1/catalog/datasets/'
WIND = ELIA + 'ods086/records?where=realtime%20is%20not%20null&order_by=datetime%20desc&limit=20&select=datetime,realtime'
SOLAR = ELIA + 'ods087/records?where=realtime%20is%20not%20null%20and%20region%3D%22Belgium%22&order_by=datetime%20desc&limit=1&select=datetime,realtime'
LOAD = ELIA + 'ods002/records?where=measured%20is%20not%20null&order_by=datetime%20desc&limit=1&select=datetime,measured'
SHIPS = 'https://raw.githubusercontent.com/alicemitakoff/albert-canal-vr/live-data/live/ships.json'
HUB = (5.10167, 51.09796)                      # swap hub on the BCTN Meerhout quay (lon, lat)


def get(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'AlbertCanalScene/1.0'})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def age(iso):
    t = datetime.fromisoformat(iso.replace('Z', '+00:00'))
    if t.tzinfo is None:
        return ''
    m = (datetime.now(timezone.utc) - t).total_seconds() / 60
    return f'{m:.0f} min ago' if m < 120 else f'{m / 60:.1f} hours ago'


def show(name, fn):
    try:
        fn()
    except Exception as e:
        print(f'{name:<10} NOT AVAILABLE ({e})')


def weather():
    c = get(METEO)['current']
    compass = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'][round(c['wind_direction_10m'] / 45) % 8]
    print(f"{'Weather':<10} {c['temperature_2m']:.0f} C, wind {c['wind_speed_10m']:.1f} m/s {compass}, cloud {c['cloud_cover']}%, "
          f"rain {c['precipitation']} mm  (Open-Meteo, local time {c['time']})")
    print(f"{'':<10} the wind speed also sets how fast the wind turbines turn in both apps")


grid = {}
def wind():
    r = get(WIND)['results']; t0 = r[0]['datetime']
    grid['wind'] = sum(x['realtime'] for x in r if x['datetime'] == t0)
    print(f"{'Wind':<10} {grid['wind']:,.0f} MW in Belgium, sum of {sum(x['datetime'] == t0 for x in r)} areas  (Elia ods086, {age(t0)})")
def solar():
    r = get(SOLAR)['results'][0]; grid['solar'] = r['realtime']
    print(f"{'Solar':<10} {r['realtime']:,.0f} MW in Belgium  (Elia ods087, {age(r['datetime'])})")
def load():
    r = get(LOAD)['results'][0]; grid['load'] = r['measured']
    print(f"{'Demand':<10} {r['measured']:,.0f} MW total load  (Elia ods002, {age(r['datetime'])})")


def ships():
    d = get(SHIPS + '?t=' + str(int(time.time())))
    now = time.time()
    fresh = [s for s in d.get('ships', []) if now - s.get('seen', 0) < 3 * 3600]
    print(f"{'Vessels':<10} {len(fresh)} on the Antwerp to Genk corridor seen in the last 3 hours  (AIS file {age(d['updated'])})")
    km = lambda s: math.hypot((s['lon'] - HUB[0]) * 69.96, (s['lat'] - HUB[1]) * 111.25)
    for s in sorted(fresh, key=km)[:3]:
        print(f"{'':<10} {s.get('name') or 'MMSI ' + s['mmsi']}: {km(s):.0f} km from the hub, "
              f"{(s.get('sog') or 0):.1f} kn, seen {(now - s['seen']) / 60:.0f} min ago")


print('Live data for the Albert Canal battery swap scene\n')
show('Weather', weather)
for n, f in (('Wind', wind), ('Solar', solar), ('Demand', load)):
    show(n, f)
if 'wind' in grid and 'load' in grid:
    ren = grid['wind'] + grid.get('solar', 0)
    print(f"{'':<10} the apps show: Belgian grid {ren / 1000:.1f} GW wind and solar, {100 * ren / grid['load']:.0f}% of demand")
show('Vessels', ships)
