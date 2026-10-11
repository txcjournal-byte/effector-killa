#!/usr/bin/env python3
"""Generates Resources/Presets/tapes.json - the "TV menu" of every cassette (effect):
8 ready-made programmes per effect, so nobody has to tweak P1-P8 by hand.
Values are REAL parameter values by name (converted by the plugin's ParamSpec).
Run:  python3 Tools/make_tapes.py
"""
import json, os, math

def t(name, desc, **params):
    p = {}
    for k, v in params.items():
        p[k.replace('__', '-').replace('_', ' ')] = v
    return {"name": name, "description": desc, "params": p}

TAPES = {
 "Cassette Plug": [
    t("Warm Tape", "gentle tape glue for anything", MODE="TAPE", DRIVE=0.25, BIAS=0.1, TONE=0.5),
    t("Dusty Deck", "old walkman, wobbly and soft", MODE="TAPE", DRIVE=0.35, TONE=0.35, WOW=0.4, HISS=0.15),
    t("Hot Tube", "thick tube harmonics", MODE="TUBE", DRIVE=0.55, BIAS=0.35, TONE=0.55),
    t("808 Grit", "808 cuts through, sub stays clean", MODE="TUBE", DRIVE=0.6, BIAS=0.3, LOW_KEEP=70),
    t("Bus Glue", "barely there, just warmth", MODE="TAPE", DRIVE=0.15, MIX=0.6),
    t("Memphis Dub", "dark, saturated tape copy", MODE="TAPE", DRIVE=0.6, TONE=0.3, WOW=0.2, HISS=0.25),
    t("Bright Crunch", "crispy saturated top end", MODE="TUBE", DRIVE=0.5, TONE=0.75),
    t("Melted", "seasick wow, lo-fi melody", MODE="TAPE", DRIVE=0.4, TONE=0.4, WOW=0.75, HISS=0.2),
 ],
 "Menace": [
    t("Clip It", "hard clipped edge", TYPE="CLIP", DRIVE=0.4),
    t("Rage Fuzz", "buzzing rage lead fuzz", TYPE="FUZZ", DRIVE=0.6, TONE=0.6),
    t("Fold Freak", "metallic wavefolding", TYPE="FOLDBACK", DRIVE=0.45),
    t("Octave Rectify", "rectified, octave-up grit", TYPE="RECTIFY", DRIVE=0.5, MIX=0.6),
    t("808 Bite", "distorted 808, clean sub", TYPE="CLIP", DRIVE=0.65, LOW_KEEP=80),
    t("Parallel Smash", "half dry, half destroyed", TYPE="FUZZ", DRIVE=0.8, MIX=0.4),
    t("Phone Distort", "thin, crushed mids", TYPE="CLIP", DRIVE=0.55, PRE__HP=400, POST__LP=4500),
    t("Dark Menace", "heavy distortion, dark tone", TYPE="FUZZ", DRIVE=0.5, TONE=0.25, POST__LP=5000),
 ],
 "Brainrot": [
    t("12-Bit Sampler", "classic sampler grit", BITS=12, RATE=26000, TONE=0.6),
    t("8-Bit Game", "console bleeps", BITS=8, RATE=16000, TONE=0.7),
    t("Vinyl Dust", "crackle and soft noise", BITS=16, CRACKLE=0.35, NOISE=0.1, TONE=0.45),
    t("Radio Rot", "noisy AM radio", BITS=10, RATE=11000, NOISE=0.35, TONE=0.35),
    t("Glitch Jitter", "unstable, stuttering rate", BITS=10, RATE=12000, JITTER=0.6),
    t("Crushed Blend", "crush on the side, body stays", BITS=6, RATE=8000, MIX=0.35),
    t("Lo-Fi Cloud", "soft dark lo-fi", BITS=13, RATE=22050, TONE=0.3, CRACKLE=0.15),
    t("Total Rot", "everything broken", BITS=5, RATE=4000, JITTER=0.4, NOISE=0.2),
 ],
 "Through The Wall": [
    t("Next Room", "muffled, party next door", TYPE="LP24", CUTOFF=700, RESO=0.15),
    t("Dark Lowpass", "takes the edge off", TYPE="LP12", CUTOFF=3500, RESO=0.1),
    t("Phone Call", "telephone band", TYPE="BP", CUTOFF=1225, BP_WIDTH=2.6, RESO=0.1),
    t("Thin Out", "high-pass, leaves room for 808", TYPE="HP", CUTOFF=300, RESO=0.1),
    t("Wah Pump", "filter moving in tempo", TYPE="LP24", CUTOFF=1500, RESO=0.45, LFO_RATE="1/4", LFO_DEPTH=0.45),
    t("Slow Sweep", "4-bar filter rise and fall", TYPE="LP24", CUTOFF=1200, RESO=0.3, LFO_RATE="4 BAR", LFO_DEPTH=0.6),
    t("Flanger", "comb filter jet sweep", TYPE="COMB", CUTOFF=600, RESO=0.6, LFO_RATE="2 BAR", LFO_DEPTH=0.5),
    t("Auto Wah", "opens with your playing", TYPE="BP", CUTOFF=600, BP_WIDTH=1.2, RESO=0.4, ENV=0.6),
 ],
 "Tone Up": [
    t("Air", "bright and expensive", HIGH=4, LOW__CUT=30),
    t("Warmth", "fuller low mids, softer top", LOW=2, TILT=-1.5),
    t("Vocal Clean", "cut mud, add presence", LOW__CUT=100, LOW__MID=-3, LOW__MID_FREQ=300, HIGH__MID=2, HIGH__MID_FREQ=3500, HIGH=2),
    t("808 Phone", "808 audible on phones", LOW__MID=2, LOW__MID_FREQ=150, HIGH__MID=3, HIGH__MID_FREQ=900),
    t("Scoop", "smiley curve", LOW=3, LOW__MID=-3, LOW__MID_FREQ=500, HIGH=3),
    t("Dark", "tilt to the dark side", TILT=-4, HIGH=-2),
    t("Mid Push", "in your face mids", HIGH__MID=4, HIGH__MID_FREQ=1200),
    t("Sub Focus", "clean low end, no rumble", LOW__CUT=30, LOW=2.5, LOW__MID=-2, LOW__MID_FREQ=250),
 ],
 "Squeeze": [
    t("Gentle Glue", "light, transparent", THRESHOLD=-14, RATIO=2, ATTACK=20, RELEASE=180, MAKEUP=1),
    t("Vocal Lock", "steady vocal level", THRESHOLD=-22, RATIO=4, ATTACK=6, RELEASE=110, MAKEUP=4),
    t("Drum Smash", "parallel punch", THRESHOLD=-30, RATIO=10, ATTACK=1, RELEASE=70, MAKEUP=8, MIX=0.4),
    t("808 Hold", "even 808 sustain", THRESHOLD=-20, RATIO=3, ATTACK=25, RELEASE=250, MAKEUP=3),
    t("Bus Glue", "slow mix bus glue", THRESHOLD=-16, RATIO=2, ATTACK=30, RELEASE=300, MAKEUP=1),
    t("Punchy", "slow attack lets transients through", THRESHOLD=-20, RATIO=4, ATTACK=40, RELEASE=120, MAKEUP=3),
    t("Brickwall", "flat and loud", THRESHOLD=-28, RATIO=20, ATTACK=0.5, RELEASE=60, MAKEUP=10),
    t("Breathing", "pumping release", THRESHOLD=-26, RATIO=6, ATTACK=3, RELEASE=400, MAKEUP=6),
 ],
 "Overcooked": [
    t("Light Cook", "a little bit of everything", DEPTH=0.25),
    t("Well Done", "loud, dense, bright", DEPTH=0.5),
    t("Burnt", "maximum crunch", DEPTH=0.85, TIME=0.3),
    t("Upward Only", "brings up the quiet details", DEPTH=0.6, UPWARD=0.9, DOWNWARD=0.1),
    t("Tamed", "downward only, controlled", DEPTH=0.5, UPWARD=0.1, DOWNWARD=0.8),
    t("Air Fryer", "crispy top end", DEPTH=0.45, HIGH=3, LOW=-1),
    t("Heavy Bottom", "big low end", DEPTH=0.4, LOW=3, HIGH=-1),
    t("Half Baked", "parallel blend", DEPTH=0.7, MIX=0.45),
 ],
 "Slap": [
    t("Snap", "more attack", ATTACK=0.5),
    t("Knock Harder", "big attack, slight clip", ATTACK=0.8, CLIP_OUT=0.3),
    t("Tight", "shorter tails", ATTACK=0.3, SUSTAIN=-0.5),
    t("Roomy", "longer sustain", SUSTAIN=0.6),
    t("Soft Hit", "softer transients", ATTACK=-0.5),
    t("Clip Knock", "clipped punch", ATTACK=0.6, CLIP_OUT=0.6),
    t("Gated", "attack only, tail removed", ATTACK=0.4, SUSTAIN=-0.9),
    t("Blend", "subtle parallel punch", ATTACK=0.7, MIX=0.5),
 ],
 "Doubles": [
    t("Wide Double", "stereo double track", RATE=0.5, DEPTH=0.35, VOICES=2, SPREAD=0.9, MIX=0.4),
    t("Dream Chorus", "lush 4-voice chorus", RATE=0.7, DEPTH=0.55, VOICES=4, SPREAD=1.0, MIX=0.5),
    t("Subtle", "just a touch of width", RATE=0.3, DEPTH=0.2, VOICES=2, SPREAD=0.7, MIX=0.25),
    t("Seasick", "deep slow warble", RATE=0.25, DEPTH=0.9, VOICES=2, MIX=0.6),
    t("Fast Shimmer", "quick vibrato shimmer", RATE=3.0, DEPTH=0.3, VOICES=3, MIX=0.4),
    t("808 Safe", "chorus above, mono bass", RATE=0.5, DEPTH=0.35, VOICES=2, LOW_KEEP=160, MIX=0.4),
    t("Vintage", "single voice, classic", RATE=0.8, DEPTH=0.45, VOICES=1, SPREAD=0.5, MIX=0.5),
    t("Choir", "thick 4-voice ensemble", RATE=0.4, DEPTH=0.6, VOICES=4, SPREAD=1.0, MIX=0.7),
 ],
 "Swirl": [
    t("Slow Swirl", "classic slow phaser", RATE=0.15, STAGES=6, FEEDBACK=0.3, DEPTH=0.7, MIX=0.5),
    t("Jet", "intense, resonant", RATE=0.3, STAGES=12, FEEDBACK=0.7, DEPTH=0.9, MIX=0.6),
    t("Bar Sync", "one sweep per bar", SYNC="ON", SYNC_RATE="1 BAR", STAGES=8, FEEDBACK=0.4, DEPTH=0.8, MIX=0.5),
    t("Subtle Move", "light motion", RATE=0.2, STAGES=4, FEEDBACK=0.2, DEPTH=0.5, MIX=0.3),
    t("Fast Vibe", "fast wobble", RATE=4.0, STAGES=4, FEEDBACK=0.3, DEPTH=0.6, MIX=0.5),
    t("Dreamy", "deep and wide", RATE=0.1, STAGES=10, FEEDBACK=0.5, DEPTH=1.0, MIX=0.45),
    t("Half Bar", "synced 1/2", SYNC="ON", SYNC_RATE="1/2", STAGES=6, FEEDBACK=0.5, DEPTH=0.8, MIX=0.5),
    t("Underwater", "dark slow phase", RATE=0.08, STAGES=12, FEEDBACK=0.6, DEPTH=0.6, MIX=0.7),
 ],
 "Ad-Lib Throw": [
    t("Quarter Throw", "classic 1/4 echo", TIME="1/4", FEEDBACK=0.3, MIX=0.25),
    t("Dotted Eighth", "bouncy 1/8D", TIME="1/8D", FEEDBACK=0.35, MIX=0.25),
    t("Ping-Pong", "left / right echoes", TIME="1/8", FEEDBACK=0.4, PING__PONG="ON", MIX=0.3),
    t("Slapback", "short single slap", TIME="1/16", FEEDBACK=0.1, MIX=0.2),
    t("Ad-Lib Throw", "long ducked throw", TIME="1/4D", FEEDBACK=0.5, DUCKING=0.6, MIX=0.4),
    t("Dark Tape Echo", "filtered repeats", TIME="1/4", FEEDBACK=0.55, LP=2500, HP=300, MIX=0.3),
    t("Endless", "long feedback trail", TIME="1/2", FEEDBACK=0.8, LP=4000, DUCKING=0.4, MIX=0.3),
    t("Clean Vocal", "ducked, sits behind the vocal", TIME="1/8", FEEDBACK=0.3, HP=400, DUCKING=0.8, MIX=0.3),
 ],
 "Aura Room": [
    t("Small Room", "tight natural room", TYPE="ROOM", SIZE=0.3, DECAY=0.8, MIX=0.2),
    t("Vocal Plate", "bright plate", TYPE="PLATE", SIZE=0.5, DECAY=1.6, DAMPING=0.2, PRE__DELAY=20, MIX=0.2),
    t("Big Hall", "wide and long", TYPE="HALL", SIZE=0.8, DECAY=4.0, MIX=0.35),
    t("Dark Cave", "dark, creepy space", TYPE="DARK", SIZE=0.7, DECAY=3.5, DAMPING=0.6, MIX=0.35),
    t("Ducked Hall", "space only in the gaps", TYPE="HALL", SIZE=0.7, DECAY=3.0, DUCKING=0.7, MIX=0.4),
    t("Drum Room", "short room for drums", TYPE="ROOM", SIZE=0.2, DECAY=0.6, MIX=0.18),
    t("Dream Wash", "huge pad wash", TYPE="HALL", SIZE=1.0, DECAY=8.0, DAMPING=0.2, MIX=0.55),
    t("808 Space", "tiny bit of air for bass", TYPE="ROOM", SIZE=0.25, DECAY=0.7, WIDTH=0.6, MIX=0.1),
 ],
 "Wide Body": [
    t("Wide", "wider stereo", WIDTH=1.5),
    t("Ultra Wide", "very wide", WIDTH=1.9, MONO_BELOW=120),
    t("Mono", "collapse to mono", WIDTH=0.0),
    t("Bass Mono", "mono lows, wide top", WIDTH=1.3, MONO_BELOW=150),
    t("Haas Spread", "short delay stereo", WIDTH=1.2, HAAS=12),
    t("Club Safe", "tight mono bass", WIDTH=1.0, MONO_BELOW=200),
    t("Narrow", "a bit narrower", WIDTH=0.7),
    t("Master Width", "subtle master widening", WIDTH=1.15, MONO_BELOW=120),
 ],
 "Chopped": [
    t("Sidechain 1/4", "pumps on every beat", MODE="PUMP", RATE="1/4", DEPTH=0.6),
    t("Pump 1/8", "faster pump", MODE="PUMP", RATE="1/8", DEPTH=0.5),
    t("Trance Gate", "16th gate", MODE="PATTERN", PATTERN=0, RATE="1/16", DEPTH=0.8),
    t("Stutter", "broken pattern", MODE="PATTERN", PATTERN=5, RATE="1/16", DEPTH=0.7),
    t("Half Chop", "subtle chop", MODE="PATTERN", PATTERN=1, RATE="1/16", DEPTH=0.35),
    t("Glitch 32", "fast glitch gate", MODE="PATTERN", PATTERN=8, RATE="1/32", DEPTH=0.6),
    t("Noise Gate", "cuts the noise floor", MODE="NOISE GATE", THRESHOLD=-50, DEPTH=1.0, SMOOTH=0.3),
    t("Soft Pump", "gentle bus pump", MODE="PUMP", RATE="1/4", DEPTH=0.25, SMOOTH=0.6),
 ],
 "Red Line": [
    t("Safe Ceiling", "transparent safety limiter", CEILING=-1.0, DRIVE=0, CHARACTER=0.0),
    t("Loud", "louder, soft clipped", CEILING=-0.3, DRIVE=4, CHARACTER=0.3),
    t("Hard Clip", "aggressive clipping", CEILING=-0.3, DRIVE=6, CHARACTER=1.0),
    t("808 Clip", "clipped 808 punch", CEILING=-0.5, DRIVE=8, CHARACTER=0.8),
    t("Master", "-1 dB master limiter", CEILING=-1.0, DRIVE=2, CHARACTER=0.2),
    t("Rage", "slammed and loud", CEILING=-0.1, DRIVE=12, CHARACTER=0.9),
    t("Gentle", "just catching peaks", CEILING=-0.5, DRIVE=1, CHARACTER=0.0),
    t("Drum Bus", "snappy drum clipping", CEILING=-0.3, DRIVE=5, CHARACTER=0.7),
 ],
}

out = {"version": 1, "tapes": [{"fx": fx, "programmes": progs} for fx, progs in TAPES.items()]}
for fx, progs in TAPES.items():
    assert len(progs) == 8, fx
path = os.path.join(os.path.dirname(__file__), "..", "Resources", "Presets", "tapes.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, ensure_ascii=False, indent=1)
print("wrote", sum(len(p) for p in TAPES.values()), "tape programmes to", os.path.normpath(path))
