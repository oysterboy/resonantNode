"""The presets page: an HTML reference built from tools/bench/presets/*.preset, each preset marked against glide.

  python tools/bench/presetpage.py <out.html>     (from the repo root)

The page is published as an Artifact (claude.ai/artifact/2onDTSwSpJxUq8wQTzrczC); rebuild and republish after a
preset changes. The files are the source, the page a view of them.
"""
import html
import os
import sys

DIR = "tools/bench/presets"
OUT = sys.argv[1]
ORDER = ["glide", "cricket", "fieldcricket", "trill", "fieldtrill", "trillrise", "bb-all", "bb-trill", "lowwindow", "wide", "low", "deep", "calm", "quiet"]


def read(name):
    what, notes, sets, other = "", [], [], []
    for line in open(os.path.join(DIR, name + ".preset"), encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        if line.startswith("#"):
            t = line.lstrip("# ")
            if not what:
                what = t
            else:
                notes.append(t)
            continue
        p = line.split()
        if len(p) >= 4 and p[0] == "PARAM" and p[1] == "SET":
            sets.append((p[2], " ".join(p[3:])))
        else:
            other.append(line)
    return what, notes, sets, other


names = [n for n in ORDER if os.path.exists(os.path.join(DIR, n + ".preset"))]
names += sorted(f[:-7] for f in os.listdir(DIR) if f.endswith(".preset") and f[:-7] not in names)
data = {n: read(n) for n in names}
base = dict(data["glide"][2]) if "glide" in data else {}

e = html.escape
cards = []
for n in names:
    what, notes, sets, other = data[n]
    full = n == "glide"
    rows = []
    for path, value in sets:
        mod = path.split(".")[0]
        if full:
            mark = ""
        elif path not in base:
            mark = '<span class="tag new">new param</span>'
        elif base[path] == value:
            mark = '<span class="tag same">as glide</span>'
        else:
            mark = '<span class="tag diff">glide %s</span>' % e(base[path])
        rows.append('<tr class="m-%s"><td class="path">%s</td><td class="val">%s</td><td>%s</td></tr>'
                    % (mod, e(path), e(value), mark))
    for c in other:
        rows.append('<tr><td class="path" colspan="3">%s</td></tr>' % e(c))
    note = "".join("<p class=\"note\">%s</p>" % e(x) for x in notes)
    if not full:
        det = [(path, value) for path, value in sets if path.startswith("detection.") and base.get(path) != value]
        if det:
            note += '<p class="det"><strong>Detection changes against glide:</strong> %s</p>' % ", ".join(
                "<code>%s</code> %s (glide %s)" % (e(pth), e(v), e(base.get(pth, "unset"))) for pth, v in det)
        else:
            note += '<p class="det none">No detection changes against glide.</p>'
    body = ('<details%s><summary>%d settings</summary><div class="tw"><table>%s</table></div></details>'
            % ("" if full else " open", len(sets) + len(other), "".join(rows)))
    cards.append("""<section class="preset" id="%s">
  <header><h2>%s</h2>%s</header>
  <p class="what">%s</p>%s
  <pre class="cmd">python tools/bench/nodepreset.py %s --gateway E0=COM12 --targets R1,E2,E3</pre>
  %s
</section>""" % (e(n), e(n), '<span class="badge">baseline</span>' if full else "", e(what), note, e(n), body))

page = """<title>Node Presets</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;600&family=IBM+Plex+Mono:wght@400;500&family=Space+Grotesk:wght@500;700&display=swap">
<style>
/* Layout: one reading column; each preset a block, its settings a table marked against the glide baseline. */
:root {
  --bg: #f5f7f6; --surface: #ffffff; --fg: #17201d; --muted: #5b6863; --line: #d6dedb;
  --accent: #1f7a5c; --diff: #a24b12; --diffbg: #fbeee4; --new: #3d4fb0; --newbg: #e9ecfb;
  --codebg: #eef2f0;
  --display: "Space Grotesk", system-ui, sans-serif; --body: "IBM Plex Sans", system-ui, sans-serif;
  --mono: "IBM Plex Mono", ui-monospace, Consolas, monospace;
}
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
  --bg: #111816; --surface: #18211e; --fg: #e3ebe7; --muted: #94a39d; --line: #2b3733;
  --accent: #5cc79f; --diff: #f0a36b; --diffbg: #3a2416; --new: #a3b0f5; --newbg: #23284a; --codebg: #1f2a26;
  color-scheme: dark; } }
:root[data-theme="dark"] {
  --bg: #111816; --surface: #18211e; --fg: #e3ebe7; --muted: #94a39d; --line: #2b3733;
  --accent: #5cc79f; --diff: #f0a36b; --diffbg: #3a2416; --new: #a3b0f5; --newbg: #23284a; --codebg: #1f2a26;
  color-scheme: dark; }
body { background: var(--bg); color: var(--fg); font: 15px/1.55 var(--body); }
.wrap { max-width: 860px; margin: 0 auto; padding-block: 32px 64px;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px));
  display: grid; gap: 22px; }
h1 { font: 700 30px/1.15 var(--display); margin: 0; text-wrap: balance; }
h2 { font: 600 20px/1.2 var(--display); margin: 0; }
.lede { color: var(--muted); max-width: 65ch; margin: 0; }
.how { background: var(--surface); border: 1px solid var(--line); border-radius: 8px; padding: 16px 18px; display: grid; gap: 8px; }
.how h2 { font-size: 15px; letter-spacing: .06em; text-transform: uppercase; color: var(--muted); }
.how ul { margin: 0; padding-left: 18px; display: grid; gap: 6px; }
code, pre, .path, .val { font-family: var(--mono); font-size: 13px; }
code { background: var(--codebg); padding: 1px 5px; border-radius: 4px; }
nav { display: flex; flex-wrap: wrap; gap: 8px; }
nav a { font: 500 13px var(--mono); color: var(--accent); text-decoration: none; border: 1px solid var(--line);
  border-radius: 999px; padding: 3px 10px; background: var(--surface); }
nav a:hover, nav a:focus-visible { border-color: var(--accent); outline: none; }
.preset { border-top: 2px solid var(--line); padding-top: 16px; display: grid; gap: 10px; min-width: 0; }
.preset header { display: flex; align-items: baseline; gap: 10px; flex-wrap: wrap; }
.badge { font: 600 11px var(--body); letter-spacing: .08em; text-transform: uppercase; color: var(--accent);
  border: 1px solid var(--accent); border-radius: 4px; padding: 1px 6px; }
.what { margin: 0; max-width: 70ch; }
.note { margin: 0; color: var(--muted); font-size: 14px; max-width: 70ch; }
.det { margin: 0; padding: 6px 10px; border-left: 3px solid var(--diff); background: var(--diffbg); max-width: 70ch; font-size: 14px; }
.det.none { border-left-color: var(--line); background: transparent; color: var(--muted); }
.cmd { margin: 0; background: var(--codebg); border-radius: 6px; padding: 8px 12px; overflow-x: auto; }
summary { cursor: pointer; color: var(--muted); font-size: 14px; }
summary:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.tw { overflow-x: auto; margin-top: 8px; }
table { border-collapse: collapse; width: 100%; background: var(--surface); border: 1px solid var(--line); }
td { padding: 4px 10px; border-bottom: 1px solid var(--line); vertical-align: top; }
td.val { font-variant-numeric: tabular-nums; white-space: nowrap; }
.tag { font: 500 12px var(--mono); padding: 1px 6px; border-radius: 4px; white-space: nowrap; }
.tag.same { color: var(--muted); }
.tag.diff { color: var(--diff); background: var(--diffbg); }
.tag.new { color: var(--new); background: var(--newbg); }
footer { color: var(--muted); font-size: 13px; }
</style>
<div class="wrap">
  <h1>Node Presets</h1>
  <p class="lede">Named setups for the ResonantNode bench, one file each in <code>tools/bench/presets/</code>. Applying one
  changes the boards live; nothing is saved, so a reboot or <code>PARAM LOAD</code> returns every board to its stored setup.
  Each preset is marked against <a href="#glide">glide</a>, the running field as captured from R1 on 2026-11-16.</p>
  <div class="how">
    <h2>How to use</h2>
    <ul>
      <li>List: <code>python tools/bench/nodepreset.py --list</code></li>
      <li>Apply by radio: <code>nodepreset.py &lt;name&gt; --gateway E0=COM12 --targets R1,E2,E3</code>, or by USB with <code>--boards R1=COM3,...</code>. It reports per board which setting did not answer ok.</li>
      <li>Save the running setup before trying another: <code>nodepreset.py --capture &lt;name&gt; --gateway E0=COM12 --from R1 --what "one line"</code>. It writes every detection, behavior and output value except the node's own rotation and silent switch.</li>
      <li>A preset that needs a newer firmware image says so in its description.</li>
    </ul>
  </div>
  <nav>@@NAV@@</nav>
  @@CARDS@@
  <footer>Generated from the preset files at commit time; the files are the source.</footer>
</div>
""".replace("@@NAV@@", "".join('<a href="#%s">%s</a>' % (e(n), e(n)) for n in names)).replace(
    "@@CARDS@@", "\n".join(cards))
open(OUT, "w", encoding="utf-8").write(page)
print(OUT, len(names))
