"""Serve firmware images for the boards' OTA pull (step 11d, FWOTA; src/app/FirmwareOta.h).

Copies the built images (PlatformIO env -> <name>.bin) into a folder and serves it over HTTP on this PC's LAN
address, then prints the command each board needs. The boards join the WiFi in include/wifi_secrets.h and fetch
the image themselves:

  on a USB Node:     NET * OTA url=http://<this PC>:<port>/node.bin      (every other Node, by radio)
                     OTA url=http://<this PC>:<port>/node.bin            (the USB Node itself)
  on the Analyzer:   OTA url=http://<this PC>:<port>/analyzer.bin
  on the Emitter:    OTA url=http://<this PC>:<port>/emitter.bin         (its USB port)

  python tools/bench/otaserve.py [--build-dir <PLATFORMIO_BUILD_DIR or .pio/build>] [--port 8070]
      [--env node=esp32dev --env analyzer=esp32dev-analyzer --env emitter=esp32dev-emitter]

Windows asks once whether Python may accept connections on the private network; allow it.
Each request is logged (which board fetched what), Ctrl+C stops the server.
"""
import argparse
import functools
import http.server
import os
import shutil
import socket
import tempfile

ap = argparse.ArgumentParser()
ap.add_argument("--build-dir", default=os.environ.get("PLATFORMIO_BUILD_DIR", ".pio/build"))
ap.add_argument("--port", type=int, default=8070)
ap.add_argument("--env", action="append", default=[],
                help="name=pio_env (default: node=esp32dev analyzer=esp32dev-analyzer emitter=esp32dev-emitter)")
ap.add_argument("--dir", help="serve this folder instead (copies nothing)")
a = ap.parse_args()

envs = a.env or ["node=esp32dev", "analyzer=esp32dev-analyzer", "emitter=esp32dev-emitter"]
serve_dir = a.dir or tempfile.mkdtemp(prefix="otaserve_")
if not a.dir:
    for item in envs:
        name, env = item.split("=")
        src = os.path.join(a.build_dir, env, "firmware.bin")
        if not os.path.exists(src):
            raise SystemExit("otaserve: %s not built (%s missing)" % (env, src))
        shutil.copy(src, os.path.join(serve_dir, name + ".bin"))
        print("%-9s <- %s (%d bytes)" % (name + ".bin", src, os.path.getsize(src)))

probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
probe.connect(("192.0.2.1", 80))  # no packet is sent; picks the LAN interface's address
host = probe.getsockname()[0]
probe.close()
for f in sorted(os.listdir(serve_dir)):
    print("OTA url=http://%s:%d/%s" % (host, a.port, f))

handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=serve_dir)
server = http.server.ThreadingHTTPServer(("0.0.0.0", a.port), handler)
print("serving %s on port %d (Ctrl+C to stop)" % (serve_dir, a.port), flush=True)
try:
    server.serve_forever()
except KeyboardInterrupt:
    pass
