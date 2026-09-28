#!/usr/bin/env python3
"""Generates Resources/Presets/factory.json – 12 channels x 8 presets (spec section 11).

Parameters are written as REAL values by parameter name (e.g. {"CUTOFF": 2500, "TYPE": "BP"});
the plugin converts them through each effect's ParamSpec. Run:  python3 Tools/make_factory.py
"""
import json, math, os

# ---------------------------------------------------------------------------- effect helpers
def fx(name, **params):
    p = {}
    for k, v in params.items():
        p[k.replace('_', ' ').replace('PRE HP', 'PRE-HP').replace('POST LP', 'POST-LP')
              .replace('LOW CUT', 'LOW-CUT').replace('LOW MID FREQ', 'LOW-MID FREQ').replace('LOW MID', 'LOW-MID')
              .replace('HIGH MID FREQ', 'HIGH-MID FREQ').replace('HIGH MID', 'HIGH-MID').replace('PRE DELAY', 'PRE-DELAY')
              .replace('PING PONG', 'PING-PONG')] = v
    return {"fx": name, "params": p}

def cassette(drive=0.3, mode="TAPE", **kw):   return fx("Cassette Plug", MODE=mode, DRIVE=drive, **kw)
def menace(drive=0.3, type="CLIP", **kw):     return fx("Menace", TYPE=type, DRIVE=drive, **kw)
def brainrot(bits=12, **kw):                  return fx("Brainrot", BITS=bits, **kw)
def wall(type="LP24", cutoff=2000, **kw):     return fx("Through The Wall", TYPE=type, CUTOFF=cutoff, **kw)
def bp(lo, hi, **kw):
    return wall("BP", math.sqrt(lo * hi), BP_WIDTH=round(math.log2(hi / lo), 3), **kw)
def tone(**kw):                               return fx("Tone Up", **kw)
def squeeze(**kw):                            return fx("Squeeze", **kw)
def cooked(depth=0.4, **kw):                  return fx("Overcooked", DEPTH=depth, **kw)
def slap(attack=0.4, **kw):                   return fx("Slap", ATTACK=attack, **kw)
def doubles(mix=0.3, **kw):                   return fx("Doubles", MIX=mix, **kw)
def swirl(mix=0.3, rate=0.4, **kw):           return fx("Swirl", MIX=mix, RATE=rate, **kw)
def throw(time="1/4", mix=0.25, **kw):        return fx("Ad-Lib Throw", TIME=time, SYNC="ON", MIX=mix, **kw)
def aura(mix=0.3, type="HALL", **kw):         return fx("Aura Room", TYPE=type, MIX=mix, **kw)
def wide(width=1.3, **kw):                    return fx("Wide Body", WIDTH=width, **kw)
def chopped(mode="PATTERN", rate="1/16", depth=0.5, **kw):
    return fx("Chopped", MODE=mode, RATE=rate, DEPTH=depth, **kw)
def redline(character=0.5, ceiling=-0.3, drive=2.0, **kw):
    return fx("Red Line", CHARACTER=character, CEILING=ceiling, DRIVE=drive, **kw)

SOFT, HARD = 0.0, 1.0
LIGHT = dict(RATIO=2.0, THRESHOLD=-14, ATTACK=15, RELEASE=150, MAKEUP=1.0)

def preset(name, desc, slots, focus="default", source=None, maps=None, todo=None):
    p = {"name": name, "description": desc, "slots": slots}
    if focus != "default": p["focusMacro"] = focus
    if source: p["source"] = source
    if maps: p["maps"] = maps
    if todo: p["tags"] = ["v1.1: " + todo]
    return p

def vmap(macro, slot, param, zero, one):
    return {"macro": macro, "slot": slot, "param": param, "zero": zero, "one": one}

VA, CO, AU, DR, KN = "VILLAIN ARC", "CRASH OUT", "AURA", "DRIP", "KNOCK"

# ---------------------------------------------------------------------------- channels
channels = []
def channel(name, desc, colour, source, presets):
    assert len(presets) == 8, name
    channels.append({"number": len(channels) + 1, "name": name, "description": desc,
                     "colour": colour, "source": source, "presets": presets})

channel("MELODIA", "melodie, klávesy", "#8A2BE2", "MELODY", [
    preset("Melodia", "teplá čistá melodie",
           [cassette(0.3), tone(TILT=-1), doubles(0.25), aura(0.25, "HALL")]),
    preset("Dark Keys", "tmavé klávesy",
           [cassette(0.4), wall("LP24", 2500), aura(0.35), wide(1.3)], VA,
           maps=[vmap(VA, 1, "CUTOFF", 6000, 800)]),
    preset("Glass Bells", "jasné bells",
           [tone(HIGH=3), throw("1/8D", 0.3), aura(0.35, "PLATE", DAMPING=0.1)], AU, todo="Heaven Mode"),
    preset("Villain Piano", "temné piano",
           [cassette(0.4, "TUBE"), wall("LP24", 1800), swirl(0.2, 0.12), aura(0.4, "DARK")], VA),
    preset("Wide Dream", "široký dreamy",
           [doubles(0.5), wide(1.8), aura(0.5, "HALL", DECAY=6.0)], DR),
    preset("Detroit Pluck", "tvrdý pluck",
           [slap(0.5), menace(0.3, "CLIP"), tone(LOW_CUT=200), throw("1/16", 0.2)], KN),
    preset("Guitar Soul", "emo kytara",
           [cassette(0.35), doubles(0.3), throw("1/4", 0.25), aura(0.3)], AU),
    preset("Melodia Mid", "nenápadné vylepšení",
           [tone(TILT=1), squeeze(**LIGHT), wide(1.2)]),
])

channel("VOX DEI", "vokály", "#40E0D0", "VOX", [
    preset("Vox Dei", "moderní trap vokál",
           [tone(LOW_CUT=120, HIGH=3), squeeze(RATIO=4, THRESHOLD=-20, ATTACK=8, RELEASE=120, MAKEUP=3),
            cassette(0.2), throw("1/4", 0.15), aura(0.15, "PLATE")]),
    preset("Rage Vox", "agresivní",
           [menace(0.35, "FUZZ"), cooked(0.4), tone(HIGH=4), throw("1/8", 0.2)], CO),
    preset("Phone Ghost", "telefon",
           [bp(500, 3000), menace(0.3, "CLIP"), aura(0.2)]),
    preset("Demon Double", "temný dubl",
           [doubles(0.5), menace(0.2), wall("LP24", 2000), wide(1.4)], VA, todo="Alter Ego -12"),
    preset("Angel Air", "vzdušný",
           [tone(HIGH=5), squeeze(**LIGHT), aura(0.35, "HALL", DAMPING=0.1), wide(1.5)], AU, todo="Heaven Mode"),
    preset("Throw It Back", "ad-lib do prostoru",
           [wall("HP", 300), throw("1/4", 0.45, PING_PONG="ON"), aura(0.4)], AU),
    preset("Crash Out Vox", "přetížený",
           [cooked(0.6), menace(0.5, "CLIP"), redline()], CO),
    preset("Whisper Tape", "lo-fi šepot",
           [brainrot(12), cassette(0.4), wall("LP24", 5000), aura(0.25)]),
])

channel("SVBTERRA", "808 / bas", "#B22222", "808", [
    preset("Svbterra", "808 slyšitelná na mobilu",
           [cassette(0.5, BIAS=0.3, LOW_KEEP=60), tone(LOW_MID=2, LOW_MID_FREQ=150, HIGH_MID=3, HIGH_MID_FREQ=900),
            wide(1.0, MONO_BELOW=120)], CO,
           maps=[vmap(CO, 0, "DRIVE", 0.2, 0.85)]),
    preset("Grave Digger", "tmavá těžká",
           [cassette(0.5, "TUBE", LOW_KEEP=50), wall("LP24", 3000), redline(SOFT)], VA),
    preset("Blown 808", "rage clipped",
           [menace(0.7, "CLIP", LOW_KEEP=50), tone(LOW_CUT=25), redline(HARD)], CO),
    preset("Fuzz Tonka", "fuzz střed",
           [menace(0.5, "FUZZ", MIX=0.5, LOW_KEEP=80), tone(HIGH_MID=4, HIGH_MID_FREQ=800), redline()], CO),
    preset("Spinz Warm", "teplá dlouhá",
           [cassette(0.35, BIAS=0.2), squeeze(RATIO=3, THRESHOLD=-18, ATTACK=30, RELEASE=300, MAKEUP=2), tone(LOW=1.5)]),
    preset("Reese Motion", "pohyb v basu",
           [doubles(0.3, LOW_KEEP=150), swirl(0.2, 0.3), menace(0.2, LOW_KEEP=80), wide(1.0, MONO_BELOW=150)], DR),
    preset("Sub Only", "čistý sub",
           [wall("LP24", 200), squeeze(RATIO=3, THRESHOLD=-18, ATTACK=10, RELEASE=150, MAKEUP=2), wide(0.0)], VA, todo="Big Back"),
    preset("Mid Bite", "agresivní středy",
           [cooked(0.5), menace(0.3, "FOLDBACK", LOW_KEEP=100), tone(HIGH_MID=4, HIGH_MID_FREQ=1000)], CO),
])

channel("DRVM CVLT", "drumy", "#FF6A00", "DRUMS", [
    preset("Drvm Cvlt", "víc punch",
           [slap(0.4), squeeze(RATIO=8, THRESHOLD=-30, ATTACK=3, RELEASE=80, MAKEUP=6, MIX=0.4), redline(SOFT)], KN),
    preset("Knock Box", "tvrdý knock",
           [slap(0.6), menace(0.3, "CLIP"), tone(LOW=3, HIGH_MID=2, HIGH_MID_FREQ=4000)], KN),
    preset("Lo-Fi Kit", "lo-fi drumy",
           [brainrot(10), cassette(0.4), wall("LP24", 7000)], CO),
    preset("Room Smash", "smashed room",
           [squeeze(RATIO=10, THRESHOLD=-30, ATTACK=0.5, RELEASE=60, MAKEUP=8, MIX=0.4), aura(0.2, "ROOM", SIZE=0.2)], AU),
    preset("Hat Shine", "lesklé hajtky",
           [tone(HIGH=4), wide(1.5), slap(0.3)]),
    preset("Snare Crack", "praskavý snare",
           [slap(0.5), menace(0.25, "CLIP"), aura(0.15, "PLATE", DECAY=0.8, SIZE=0.3)], KN),
    preset("Memphis Tape", "kazetové drumy",
           [cassette(0.6, WOW=0.3), brainrot(12), bp(80, 9000)]),
    preset("Clipper Cult", "hlasité drumy",
           [cooked(0.3), redline(HARD, ceiling=-0.3, drive=4.0)], CO),
])

channel("RAGE ENGINE", "rage leady, synthy", "#FF0000", "MELODY", [
    preset("Rage Engine", "klasický rage lead",
           [menace(0.5, "FUZZ"), cooked(0.5), doubles(0.3), wide(1.7), aura(0.2)], CO),
    preset("Hellfire Saw", "supersaw drive",
           [cassette(0.6, "TUBE"), menace(0.4, "CLIP"), tone(HIGH=3), throw("1/8", 0.2)], CO),
    preset("Siren Lead", "pumpující",
           [chopped("PUMP", "1/4", 0.6), menace(0.4), aura(0.3)], DR),
    preset("Molten Pad", "rozžhavený pad",
           [cassette(0.7), swirl(0.4, 0.25), aura(0.5)], AU),
    preset("Crushed Arp", "crushed arp",
           [brainrot(8), wall("LP24", 3000, LFO_RATE="1/8", LFO_DEPTH=0.3), throw("1/16", 0.3)], DR),
    preset("Screamer", "foldback",
           [menace(0.6, "FOLDBACK"), wall("HP", 300), wide(1.6)], CO),
    preset("Stadium", "velký hall",
           [cooked(0.4), wide(1.9), aura(0.5, "HALL", SIZE=0.85, DECAY=4.5)], AU),
    preset("Burn It Down", "extrém",
           [menace(0.8, "FUZZ"), brainrot(10), redline(HARD)], CO),
])

channel("DREAMCORE", "plugg, dreamy", "#FFB6C1", "MELODY", [
    preset("Dreamcore", "plugg dream",
           [doubles(0.35), wall("LP24", 7000), throw("1/4D", 0.2), aura(0.45, "HALL")], AU, todo="Heaven Mode"),
    preset("Cloud Bells", "měkké bells",
           [tone(TILT=-1), throw("1/8D", 0.35, PING_PONG="ON"), aura(0.4)], AU),
    preset("Pluggnb Glow", "teplá záře",
           [cassette(0.3), doubles(0.3), aura(0.35), wide(1.5)]),
    preset("Heaven Gate", "andělský",
           [tone(LOW_CUT=200), doubles(0.3), aura(0.6, "HALL", DAMPING=0.1, DECAY=5.0)], AU, todo="Heaven Mode"),
    preset("Soft Focus", "rozmazaný",
           [wall("LP24", 3500), doubles(0.5), aura(0.5)], VA),
    preset("Pink Tape", "jemná páska",
           [cassette(0.4, WOW=0.4), wall("LP24", 8000), aura(0.3)], DR),
    preset("Main Character", "široký",
           [wide(1.9), throw("1/4", 0.3), aura(0.4)], AU),
    preset("Floating", "plovoucí",
           [swirl(0.3, 0.12), throw("1/2", 0.3), aura(0.6, DECAY=5.0)], DR),
])

channel("VHS", "lo-fi", "#C8A165", "MELODY", [
    preset("VHS", "kazeta",
           [cassette(0.5, WOW=0.5, HISS=0.2), brainrot(12), wall("LP24", 6000)]),
    preset("Basement", "sklep",
           [wall("LP24", 1200), aura(0.3, "ROOM")], VA),
    preset("Payphone", "telefon",
           [bp(400, 3000), menace(0.3, "CLIP")]),
    preset("8-Bit Soul", "8-bit",
           [brainrot(6, RATE=11025)], CO),
    preset("Pirate Radio", "rádio",
           [bp(600, 4000), brainrot(10, NOISE=0.4)]),
    preset("Warped Vinyl", "vinyl",
           [cassette(0.3, WOW=0.7, HISS=0.3), brainrot(16, CRACKLE=0.3), wall("LP24", 7000)], DR),
    preset("Dusty", "prašný",
           [tone(TILT=-3), brainrot(14, CRACKLE=0.2), squeeze(**LIGHT)]),
    preset("Memphis Mist", "Memphis",
           [cassette(0.6), brainrot(10, RATE=22050), wall("LP24", 4000), aura(0.2)]),
])

channel("MVTANT", "FX, sound design", "#7CFC00", "FX", [
    preset("Mvtant", "zmutovaný zvuk",
           [menace(0.4, "FOLDBACK"), wall("COMB", 800, LFO_DEPTH=0.5, RESO=0.5), throw("1/16", 0.4)], DR, todo="Jet Lag"),
    preset("Toxic Comb", "kovový comb",
           [wall("COMB", 600, RESO=0.6), menace(0.3), aura(0.3)]),
    preset("Radioactive", "vibrující",
           [swirl(0.6, 3.0), brainrot(8), wide(1.8)], DR),
    preset("Robot NPC", "robotický",
           [wall("COMB", 220, RESO=0.85), brainrot(8), chopped("PATTERN", "1/16", 0.3)], todo="Alter Ego formant, NPC Glitch"),
    preset("Underwater", "pod vodou",
           [wall("LP24", 700, LFO_DEPTH=0.3, LFO_RATE="1 BAR"), doubles(0.5), aura(0.5)], VA),
    preset("Portal", "vír",
           [wall("COMB", 500, LFO_RATE="4 BAR", LFO_DEPTH=0.7, RESO=0.5), swirl(0.4, 0.2), aura(0.6)], AU, todo="Heaven Mode -12"),
    preset("Glitch Aura", "glitchový prostor",
           [brainrot(12, JITTER=0.5), chopped("PATTERN", "1/32", 0.4), throw("1/32", 0.3, FEEDBACK=0.6), aura(0.4)], DR),
    preset("Black Hole", "nekonečný",
           [aura(0.7, "HALL", SIZE=1.0, DECAY=20.0), wall("LP24", 2000), wide(2.0)], AU),
])

channel("NO FILTER", "raw, suché, clipnuté", "#F2F2F2", "VOX", [
    preset("No Filter", "syrový suchý vokál",
           [tone(LOW_CUT=100), squeeze(RATIO=6, THRESHOLD=-24, ATTACK=1, RELEASE=60, MAKEUP=4), menace(0.2, "CLIP"), redline(SOFT)], CO),
    preset("Crash Out", "totální přetížení",
           [cooked(0.7), menace(0.4, "FUZZ"), redline(HARD)], CO),
    preset("Opp Pack", "tmavý a tvrdý",
           [cassette(0.5, "TUBE"), wall("LP24", 2000), slap(0.4), redline()], VA),
    preset("Lock In", "čistý suchý vokál vepředu",
           [squeeze(RATIO=4, THRESHOLD=-20, ATTACK=8, RELEASE=120, MAKEUP=3), tone(HIGH=2), redline(SOFT)], KN),
    preset("Red Line Rage", "hlasitý rage",
           [menace(0.6, "CLIP"), cooked(0.4), redline(HARD, ceiling=-0.1)], CO),
    preset("Dry Menace", "suchá distorze",
           [menace(0.4, "FOLDBACK", LOW_KEEP=120), tone(HIGH_MID=3, HIGH_MID_FREQ=1500)], CO),
    preset("Big Back 808", "tlustá syrová 808",
           [cassette(0.6, LOW_KEEP=70), menace(0.3, "CLIP", LOW_KEEP=90), wide(1.0, MONO_BELOW=150), redline()], CO, source="808"),
    preset("3AM Studio", "suchý, intimní",
           [cassette(0.3), squeeze(RATIO=3, THRESHOLD=-18, ATTACK=10, RELEASE=150, MAKEUP=2), tone(TILT=-1),
            chopped("NOISE GATE", "1/16", 1.0, THRESHOLD=-50, SMOOTH=0.3)], VA),
])

channel("DRIFT", "phonk, cowbell, Memphis", "#6A5ACD", "MELODY", [
    preset("Drift", "phonk cowbell",
           [cassette(0.4), menace(0.5, "CLIP"), wall("LP24", 6000), redline()], CO),
    preset("Cowbell Curse", "přetížený zvon",
           [menace(0.6, "FUZZ"), tone(HIGH_MID=5, HIGH_MID_FREQ=900), redline(HARD)], CO),
    preset("Night Drive", "noční jízda",
           [cassette(0.4, WOW=0.2), wall("LP24", 4000), throw("1/4D", 0.25), aura(0.3)], AU),
    preset("Smoke Tape", "zakouřená kazeta",
           [brainrot(12), cassette(0.5, HISS=0.3), bp(100, 6000)], VA),
    preset("Tokyo Ghost", "éterický pohyb",
           [doubles(0.4), swirl(0.3, 0.3), aura(0.4), wide(1.6)], DR),
    preset("Burnout", "sekaný drive",
           [cooked(0.6), menace(0.5, "CLIP"), chopped("PATTERN", "1/8", 0.3)], KN),
    preset("Low Rider", "phonk bas",
           [cassette(0.5, "TUBE", LOW_KEEP=60), tone(LOW=3), squeeze(RATIO=3, THRESHOLD=-18, ATTACK=20, RELEASE=200, MAKEUP=2),
            redline(SOFT)], CO, source="808"),
    preset("Midnight Pressure", "pumpující temnota",
           [chopped("PUMP", "1/4", 0.5), menace(0.4), wall("LP24", 3000), aura(0.25)], DR),
])

channel("HYPER", "hyperpop, glitch", "#00E5FF", "VOX", [
    preset("Hyper", "hyperpop vokál",
           [squeeze(RATIO=6, THRESHOLD=-24, ATTACK=3, RELEASE=80, MAKEUP=4), tone(HIGH=5), menace(0.3, "CLIP"), doubles(0.3), wide(1.6)], CO),
    preset("Brain Melt", "rozpadlý glitch",
           [brainrot(8, JITTER=0.4), chopped("PATTERN", "1/16", 0.4), throw("1/16", 0.3)], DR),
    preset("Delulu", "snový, přeslazený",
           [doubles(0.5), swirl(0.4, 0.3), aura(0.4, DAMPING=0.1), wide(1.8)], AU),
    preset("Screen Time", "digitální drť",
           [brainrot(10), wall("HP", 400), menace(0.3, "CLIP"), redline()], CO),
    preset("Glitch Princess", "sekaný lesk",
           [chopped("PATTERN", "1/32", 0.5), tone(HIGH=4), throw("1/8", 0.3, PING_PONG="ON")], DR),
    preset("Sugar Rush", "přesycený, jasný",
           [tone(LOW_CUT=250, HIGH=6), cooked(0.5), menace(0.3, "FUZZ"), redline()], CO, todo="Alter Ego +12"),
    preset("Plot Twist", "pohyblivý filtr",
           [wall("HP", 400, LFO_RATE="1/2", LFO_DEPTH=0.6), menace(0.3), aura(0.35)], DR),
    preset("Lore Drop", "velký glitch prostor",
           [chopped("PATTERN", "1/16", 0.5), brainrot(12), aura(0.5, "HALL"), wide(1.7)], AU),
])

channel("FINAL BOSS", "bus, master", "#C0C0C0", "BUS", [
    preset("Final Boss", "master glue + hlasitost",
           [squeeze(RATIO=2, THRESHOLD=-16, ATTACK=30, RELEASE=300, MAKEUP=1), tone(TILT=0.5), wide(1.15, MONO_BELOW=120),
            redline(SOFT, ceiling=-1.0)], KN),
    preset("No Cap Master", "čistý master",
           [tone(LOW_CUT=25), squeeze(RATIO=1.5, THRESHOLD=-14, ATTACK=20, RELEASE=200, MAKEUP=1), redline(SOFT, ceiling=-1.0)]),
    preset("Glue Gang", "lepidlo na bus",
           [squeeze(RATIO=4, THRESHOLD=-22, ATTACK=30, RELEASE=150, MAKEUP=3, MIX=0.5), cassette(0.15), redline(SOFT)], KN),
    preset("Car Test", "basy do auta",
           [tone(LOW=2, LOW_MID=-2, LOW_MID_FREQ=300), cooked(0.2), redline()], KN),
    preset("Phone Speaker", "slyšitelné z mobilu",
           [cassette(0.3, LOW_KEEP=80), tone(HIGH_MID=3, HIGH_MID_FREQ=1000), wide(1.0, MONO_BELOW=150), redline()]),
    preset("Big Stage", "široký bus",
           [tone(HIGH=1.5), wide(1.4, MONO_BELOW=150), redline(SOFT)], AU),
    preset("Loud Money", "maximální hlasitost",
           [cooked(0.3), menace(0.1, "CLIP", MIX=0.3, LOW_KEEP=150), redline(HARD, ceiling=-0.3, drive=4.0)], CO),
    preset("Side Quest", "jemné pumpování busu",
           [squeeze(RATIO=8, THRESHOLD=-30, ATTACK=3, RELEASE=80, MAKEUP=6, MIX=0.3), chopped("PUMP", "1/4", 0.2), redline(SOFT)], DR),
])

out = {"version": 1, "product": "Effector Killa", "channels": channels}
path = os.path.join(os.path.dirname(__file__), "..", "Resources", "Presets", "factory.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, ensure_ascii=False, indent=1)
print("wrote", sum(len(c["presets"]) for c in channels), "presets to", os.path.normpath(path))
