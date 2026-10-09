"""Collect live AIS positions on the Albert Canal (Antwerp to Genk) from aisstream.io for 45 seconds
and write live/ships.json. The API key comes from the AISSTREAM_KEY secret and is never written out."""
import asyncio, json, os, time, datetime
import websockets

KEY = os.environ.get('AISSTREAM_KEY', '').strip()
BOX = [[[50.88, 4.25], [51.32, 5.62]]]          # the pilot corridor, Antwerp port to Genk
OUT = 'live/ships.json'
LISTEN = 45
KEEP_H = 3

def load_previous():
    try:
        return {s['mmsi']: s for s in json.load(open(OUT))['ships']}
    except Exception:
        return {}

async def collect(ships):
    async with websockets.connect('wss://stream.aisstream.io/v0/stream', max_size=2**22) as ws:
        await ws.send(json.dumps({'APIKey': KEY, 'BoundingBoxes': BOX,
                                  'FilterMessageTypes': ['PositionReport', 'StandardClassBPositionReport', 'ShipStaticData']}))
        end = time.time() + LISTEN
        while time.time() < end:
            try:
                raw = await asyncio.wait_for(ws.recv(), timeout=max(0.1, end - time.time()))
            except asyncio.TimeoutError:
                break
            m = json.loads(raw)
            if 'error' in m:
                raise RuntimeError(m['error'])
            md = m.get('MetaData', {}); mmsi = str(md.get('MMSI', ''))
            if not mmsi:
                continue
            s = ships.setdefault(mmsi, {'mmsi': mmsi})
            name = (md.get('ShipName') or '').strip()
            if name:
                s['name'] = name
            t = m.get('MessageType')
            if t in ('PositionReport', 'StandardClassBPositionReport'):
                p = m['Message'][t]
                s.update(lat=round(p['Latitude'], 6), lon=round(p['Longitude'], 6), sog=p.get('Sog'), cog=p.get('Cog'),
                         hdg=p.get('TrueHeading'), seen=int(time.time()))
            elif t == 'ShipStaticData':
                p = m['Message'][t]; d = p.get('Dimension') or {}
                s.update(type=p.get('Type'), dest=(p.get('Destination') or '').strip(),
                         len=(d.get('A') or 0) + (d.get('B') or 0), beam=(d.get('C') or 0) + (d.get('D') or 0))

def main():
    if not KEY:
        raise SystemExit('AISSTREAM_KEY is not set')
    ships = load_previous()
    asyncio.run(collect(ships))
    now = int(time.time())
    keep = [s for s in ships.values() if 'lat' in s and now - s.get('seen', 0) < KEEP_H * 3600]
    os.makedirs('live', exist_ok=True)
    json.dump({'updated': datetime.datetime.utcnow().replace(microsecond=0).isoformat() + 'Z', 'source': 'aisstream.io',
               'count': len(keep), 'ships': sorted(keep, key=lambda s: -s['seen'])}, open(OUT, 'w'), separators=(',', ':'))
    print('ships', len(keep))

if __name__ == '__main__':
    main()
