"""Radio range: one gateway on a laptop's USB, one Node carried away from it, a measurement at each distance on the
operator's go. Needs only Python and pyserial on the laptop (this file alone is enough).

  python rangetest.py --port COM12 --node R1 [--pings 40] [--out range_R1.csv]

At each point the operator types the distance in metres and a note (line of sight, a person between, the node's
side turned away, on the floor ...), then:

  1. NET <node> NET PING once: the node's radio counters before (rx, tx_failed).
  2. --pings x NET <node> NET PING, one at a time: a reply within 1.5 s or a miss, and the round trip in ms.
  3. --big x NET <node> RB STATUS: a reply of ~9 frames; the parts that came of the parts it had.
  4. NET PING again: the node's counters after.

What the columns mean, for ESP-NOW as this firmware uses it (src/net/NodeNet): a command is broadcast 3 times (no
acknowledgement, no retry), the node's reply is unicast (the radio acknowledges and retries it). So
  reply_pct       a command came and its answer came back: the link as the bench uses it
  bcast_pct       the node's received frames over the frames sent (3 per command): the raw broadcast delivery,
                  the first number to fall as the range runs out (the repeats still hide it in reply_pct)
  node_tx_failed  unicast replies the node gave up on after the radio's retries
  big_whole       long replies that arrived whole; big_missing_parts the parts the gateway saw missing, big_no_end
                  the ones whose last part (or command) never came
The firmware reports no signal strength (RSSI); the margin is read from bcast_pct and from the variants at the far end.

Other radios of this network (NET_ID) must be off: their frames land in the node's rx count. The node is powered at
the far end (a power bank); the script mutes it first (live). Rows go to --out after every point.
"""
import argparse
import csv
import datetime
import os
import re
import statistics
import sys
import time

import serial

KV = re.compile(r"(\w+)=(\S*)")


class Gateway:
    def __init__(self, port):
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = port, 115200, 0.02
        self.s.dtr = False
        self.s.rts = False
        self.s.open()
        self.buf = b""
        time.sleep(0.5)
        self.s.reset_input_buffer()

    def lines(self):
        self.buf += self.s.read(4096)
        *done, self.buf = self.buf.split(b"\n")
        return [x.decode(errors="replace").strip() for x in done]

    def command(self, target, text, timeout):
        """Sends NET <target> <text>; returns (reply lines, the parts the node sent and the gateway's missing count from
        the done line, both None when no done line came, and the ms to it). The gateway joins the parts into one line
        and reports gaps on the done line (src/net/NetGateway)."""
        self.lines()
        self.s.write(("NET %s %s\n" % (target, text)).encode())
        t0 = time.time()
        seq, parts, missing, done_ms, reply = None, None, None, None, []
        while time.time() - t0 < timeout:
            for line in self.lines():
                if seq is None and line.startswith("NET sent seq="):
                    seq = line.split("seq=")[1].split()[0]
                    continue
                if seq is None or not line.startswith("NET reply") or (" seq=%s " % seq) not in line + " ":
                    continue
                kv = dict(KV.findall(line))
                if " done parts=" in line:
                    parts = int(kv.get("parts", "0"))
                    missing = int(kv.get("missing", "0"))
                    done_ms = (time.time() - t0) * 1000.0
                else:
                    reply.append(line)
            if done_ms is not None:
                break
        return reply, parts, missing, done_ms


def counters(gw, node):
    reply, _, _, _ = gw.command(node, "NET PING", 2.0)
    for line in reply:
        if "rx=" in line and "tx_failed=" in line:
            kv = dict(KV.findall(line.split(" part=", 1)[1]))
            return int(kv["rx"]), int(kv["tx_failed"])
    return None


def measure(gw, node, pings, big, gap):
    before = counters(gw, node)
    ok, rtts = 0, []
    for i in range(pings):
        _, parts, missing, ms = gw.command(node, "NET PING", 1.5)
        if ms is not None and missing == 0:
            ok += 1
            rtts.append(ms)
        print("\r  ping %d / %d, %d answered" % (i + 1, pings, ok), end="", flush=True)
        time.sleep(gap)
    print()
    big_whole = big_missing = big_no_end = 0
    for i in range(big):
        _, parts, missing, ms = gw.command(node, "RB STATUS", 4.0)
        if ms is None:
            big_no_end += 1  # the last part (or the command) lost: how much came is unknown
        else:
            big_whole += 1 if missing == 0 else 0
            big_missing += missing
        print("\r  long reply %d / %d" % (i + 1, big), end="", flush=True)
    print()
    after = counters(gw, node)
    row = {"sent": pings, "replies": ok, "reply_pct": "%.1f" % (100.0 * ok / pings if pings else 0.0),
           "rtt_med_ms": "%.0f" % statistics.median(rtts) if rtts else "",
           "rtt_max_ms": "%.0f" % max(rtts) if rtts else "",
           "big_whole": "%d/%d" % (big_whole, big) if big else "", "big_missing_parts": big_missing,
           "big_no_end": big_no_end}
    if before and after:
        # Frames the node received between the two counter reads: the pings, the long replies' commands and the
        # second NET PING, 3 frames each.
        commands = pings + big + 1
        row["bcast_pct"] = "%.1f" % (100.0 * (after[0] - before[0]) / (3 * commands))
        row["node_tx_failed"] = after[1] - before[1]
    else:
        row["bcast_pct"] = ""
        row["node_tx_failed"] = ""
    return row


FIELDS = ["time", "node", "distance_m", "note", "sent", "replies", "reply_pct", "bcast_pct", "rtt_med_ms",
          "rtt_max_ms", "big_whole", "big_missing_parts", "big_no_end", "node_tx_failed"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="the gateway's port")
    ap.add_argument("--node", required=True, help="the node's name (R1, E2, ...)")
    ap.add_argument("--pings", type=int, default=40)
    ap.add_argument("--big", type=int, default=3, help="long replies (RB STATUS) per point")
    ap.add_argument("--gap", type=float, default=0.2, help="seconds between pings")
    ap.add_argument("--out", help="the CSV (default range_<node>_<date>.csv here)")
    A = ap.parse_args()

    out = A.out or "range_%s_%s.csv" % (A.node, datetime.date.today().isoformat())
    gw = Gateway(A.port)
    print("gateway on %s; the node %s should answer at arm's length first:" % (A.port, A.node))
    # A node powered up on a power bank boots unmuted (the mute is live, not stored).
    reply, _, _, _ = gw.command(A.node, "PARAM SET output.silent 1", 2.0)
    print("  muted" if any("status=ok" in x for x in reply) else "  (no answer to the mute)")
    c = counters(gw, A.node)
    print("  %s" % ("answers (rx=%d tx_failed=%d)" % c if c else "NO ANSWER: check the node, its name, its power"))

    new = not os.path.exists(out)
    with open(out, "a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if new:
            w.writeheader()
        while True:
            print()
            d = input("distance in m (Enter = stop) > ").strip().replace(",", ".")
            if not d:
                break
            try:
                float(d)
            except ValueError:
                print("  a number, e.g. 7.5")
                continue
            note = input("note (line of sight, person between, turned away, ...) > ").strip()
            row = measure(gw, A.node, A.pings, A.big, A.gap)
            row.update({"time": datetime.datetime.now().isoformat(timespec="seconds"), "node": A.node,
                        "distance_m": d, "note": note})
            w.writerow(row)
            f.flush()
            print("  %s m: replies %s %%, broadcast %s %%, round trip %s ms (max %s), long replies whole %s "
                  "(parts missing %s, no end %s), node tx_failed %s"
                  % (d, row["reply_pct"], row["bcast_pct"] or "?", row["rtt_med_ms"] or "-", row["rtt_max_ms"] or "-",
                     row["big_whole"] or "-", row["big_missing_parts"], row["big_no_end"], row["node_tx_failed"]))
    print("rows in %s" % out)


if __name__ == "__main__":
    main()
