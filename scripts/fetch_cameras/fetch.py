import sys
import time
from datetime import datetime, timedelta
import http.client
import urllib.error
import urllib.request
import urllib.parse
import json

USER_AGENT = 'Catchcam-fetch-script[](https://github.com/IvanVnucec/catchcam)'

# San Francisco speed cameras (SFMTA Speed Safety Camera program) from DataSF.
# Most of them are missing from OpenStreetMap. Dataset license: PDDL (public domain).
# The dataset holds daily warning/citation counts, one row per camera per day.
SF_DATASET_ID = "d5uh-bk84"
SF_DATASET_URL = f"https://data.sf.gov/resource/{SF_DATASET_ID}.json"
# A camera counts as active if it reported within this many days of the newest
# record in the dataset. This drops positions of cameras that were relocated.
SF_ACTIVE_WINDOW_DAYS = 60

# Refuse a new list with fewer OSM cameras than this fraction of the previous one
OSM_MIN_RATIO = 0.9

# Attempts rotate through the URLs. The query takes 4-5 min when the server isn't busy.
OVERPASS_URLS = [
    "https://overpass-api.de/api/interpreter",
    "https://overpass.private.coffee/api/interpreter",
]
OVERPASS_QUERY = """[out:json][timeout:600];
node["highway"="speed_camera"];
out geom;
"""
OVERPASS_CLIENT_TIMEOUT_S = 700
OVERPASS_RETRY_DELAYS_MIN = [1, 2, 4, 8]
OVERPASS_RETRYABLE_HTTP = (429, 502, 503, 504)

def query_datasf(params):
    url = SF_DATASET_URL + '?' + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={'User-Agent': USER_AGENT, 'Accept': 'application/json'})
    with urllib.request.urlopen(req, timeout=120) as response:
        return json.loads(response.read())

def get_sf_cameras():
    where = "enforcement_type = 'FIXED SPEED' AND latitude IS NOT NULL AND longitude IS NOT NULL"
    latest = query_datasf({'$select': 'max(date) AS latest', '$where': where})[0].get('latest')
    if not latest:
        raise RuntimeError("DataSF returned no San Francisco speed cameras")
    cutoff = datetime.strptime(latest[:10], '%Y-%m-%d') - timedelta(days=SF_ACTIVE_WINDOW_DAYS)
    fields = 'site_id, location, latitude, longitude, posted_speed'
    rows = query_datasf({
        '$select': f'{fields}, max(date) AS last_seen',
        '$where': f"{where} AND date >= '{cutoff:%Y-%m-%dT%H:%M:%S}'",
        '$group': fields,
        '$limit': 50000,
    })
    # Keep one entry per camera position, using its most recent speed limit
    newest = {}
    for r in rows:
        key = (r['site_id'], r['latitude'], r['longitude'])
        if key not in newest or r['last_seen'] > newest[key]['last_seen']:
            newest[key] = r
    cameras = []
    for r in newest.values():
        location = ' '.join(r.get('location', '').split())
        cameras.append({
            'lat': float(r['latitude']),
            'lon': float(r['longitude']),
            'limit': int(float(r.get('posted_speed') or 0)),
            'ref': f"https://data.sf.gov/d/{SF_DATASET_ID} {r['site_id']} {location}",
        })
    if len(cameras) == 0:
        raise RuntimeError("DataSF returned no San Francisco speed cameras")
    print(f"Found {len(cameras)} San Francisco cameras on DataSF (data up to {latest[:10]})")
    return cameras

class OverpassError(RuntimeError):
    pass

def query_overpass(url):
    data = urllib.parse.urlencode({'data': OVERPASS_QUERY}).encode('utf-8')
    req = urllib.request.Request(
        url,
        data=data,
        method='POST',
        headers={
            'User-Agent': USER_AGENT,
            'Accept': 'application/json',
            'Content-Type': 'application/x-www-form-urlencoded',
        }
    )
    with urllib.request.urlopen(req, timeout=OVERPASS_CLIENT_TIMEOUT_S) as response:
        result = json.loads(response.read())
    # Overpass reports query errors (e.g. its own timeout) as HTTP 200 with a remark
    if 'remark' in result:
        raise OverpassError(result['remark'])
    return result['elements']

def get_speed_cameras_from_overpass_api():
    for attempt, delay in enumerate(OVERPASS_RETRY_DELAYS_MIN + [None]):
        url = OVERPASS_URLS[attempt % len(OVERPASS_URLS)]
        try:
            return query_overpass(url)
        except (OSError, http.client.HTTPException, json.JSONDecodeError, OverpassError) as e:
            if isinstance(e, urllib.error.HTTPError) and e.code not in OVERPASS_RETRYABLE_HTTP:
                raise
            if delay is None:
                raise
            print(f"Overpass API {url} failed ({e}), retrying in {delay} min")
            time.sleep(delay * 60)

def get_osm_cameras():
    overpass = get_speed_cameras_from_overpass_api()
    cameras = []
    invalid_maxspeed_cnt = 0
    for node in overpass:
        lat, lon = float(node['lat']), float(node['lon'])
        maxspeed = node.get('tags', {}).get('maxspeed', '0')
        try:
            limit = int(maxspeed.split()[0])
        except:
            invalid_maxspeed_cnt += 1
            limit = 0
        ref = f"https://www.openstreetmap.org/node/{node['id']}"
        cameras.append({'lat': lat, 'lon': lon, 'limit': limit, 'ref': ref})
    print(f"Found {len(cameras)} cameras on OpenStreetMap")
    print(f"Found {invalid_maxspeed_cnt} cameras with invalid maxspeed value")
    return cameras

def generate_c_file(output_file, data_list):
    with open(output_file, 'w') as f:
        current_date = datetime.now().strftime("%d-%m-%Y %H:%M:%S")
        cams_list = ""
        for c in data_list:
            lat = c["lat"]
            lon = c["lon"]
            limit = c["limit"]
            ref = c["ref"]
            gmaps_url = f"https://maps.google.com/maps?q=&layer=c&cbll={lat},{lon}"
            line = f'    {{ {lat:.7f}f, {lon:.7f}f, {limit} }}, // {ref} // {gmaps_url}\n'
            cams_list += line

        f.write(f"""\
// This file was generated by a script on {current_date}
// Do not edit this file manually

#pragma once

#include <pico/platform/sections.h>
#include "cams.h"

static const struct cams_camera_info __in_flash("cams_list") cams_list[] = {{
{cams_list}
}};

static const size_t cams_list_length = sizeof(cams_list) / sizeof(cams_list[0]);
""")

def generate_js_file(output_file, data_list):
    with open(output_file, 'w') as f:
        current_date = datetime.now().strftime("%d-%m-%Y %H:%M:%S")
        contents = f'// This file was generated by a script on {current_date}\n'
        contents += '// Do not edit this file manually\n\n'
        contents += 'const cams_list = [\n'
        for c in data_list:
            lat = c["lat"]
            lon = c["lon"]
            contents += f'    {{ lat: {lat:.7f}, lng: {lon:.7f} }},\n'
        contents += '];\n\n'
        contents += 'window.cams_list = cams_list;\n'
        f.write(contents)

def count_osm_cameras(cams_file):
    try:
        with open(cams_file) as f:
            return sum('openstreetmap.org/node/' in line for line in f)
    except FileNotFoundError:
        return 0

def main(output_file_h, output_file_js):
    print("Fetching cameras...")
    # Any failed source raises and aborts the script, so a list with
    # missing cameras is never generated (the previous list stays in place)
    sf_cameras = get_sf_cameras()
    osm_cameras = get_osm_cameras()
    prev_osm_cnt = count_osm_cameras(output_file_h)
    if len(osm_cameras) < prev_osm_cnt * OSM_MIN_RATIO:
        raise RuntimeError(f"Got only {len(osm_cameras)} cameras from OpenStreetMap, previous list had {prev_osm_cnt}")
    cameras = sf_cameras + osm_cameras
    if len(cameras) == 0:
        print("No cameras found")
        sys.exit(1)
    cameras_sorted = sorted(cameras, key=lambda x: (x['lat'], x['lon']))
    print(f"Found total {len(cameras_sorted)} cameras")
    print(f"Generating C header file: {output_file_h}")
    generate_c_file(output_file_h, cameras_sorted)
    print(f"Generating JS file: {output_file_js}")
    generate_js_file(output_file_js, cameras_sorted)
    print("Done")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 fetch.py filename.h filename.js")
        sys.exit(1)

    output_file_h = sys.argv[1]
    output_file_js = sys.argv[2]
    main(output_file_h, output_file_js)
