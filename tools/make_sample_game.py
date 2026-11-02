#!/usr/bin/env python3
"""Gera o jogo de exemplo (formato de projeto do GDevelop) em android/app/src/main/assets/game."""
import json, os, struct, zlib

OUT = os.path.join(os.path.dirname(__file__), "..", "android", "app", "src", "main", "assets", "game")
os.makedirs(OUT, exist_ok=True)

def png(path, w, h, pixel):
    raw = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(w)) for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) \
        + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    open(os.path.join(OUT, path), "wb").write(data)

def player(x, y):
    edge = x < 3 or y < 3 or x > 60 or y > 60
    eye = (y in range(18, 28)) and (x in range(16, 26) or x in range(38, 48))
    return (255, 255, 255, 255) if eye else (20, 40, 120, 255) if edge else (60, 120, 240, 255)

def coin(x, y):
    d = ((x - 31.5) ** 2 + (y - 31.5) ** 2) ** 0.5
    if d > 30: return (0, 0, 0, 0)
    return (200, 140, 0, 255) if d > 25 else (255, 210, 40, 255)

png("player.png", 64, 64, player)
png("coin.png", 64, 64, coin)

def cond(i, p, inv=False): return {"type": {"inverted": inv, "value": i}, "parameters": p}
def act(i, p): return {"type": {"inverted": False, "value": i}, "parameters": p}
def std(c, a, sub=None):
    return {"type": "BuiltinCommonInstructions::Standard", "conditions": c, "actions": a, "events": sub or []}
def sprite(name, img):
    return {"name": name, "type": "Sprite", "variables": [],
            "animations": [{"name": "", "directions": [{"sprites": [{"image": img}]}]}]}
def inst(name, x, y, **kw):
    d = {"name": name, "x": x, "y": y, "layer": "", "zOrder": 0, "angle": 0, "customSize": False}
    d.update(kw); return d
cube = {"name": "Spinner", "type": "Scene3D::Cube3DObject", "variables": [],
        "content": {"width": 160, "height": 160, "depth": 160, "frontFaceResourceName": ""}}
layers = [{"name": "", "visibility": True}]

level1 = {
    "name": "Level1", "r": 30, "g": 34, "b": 50, "layers": layers,
    "variables": [{"name": "score", "type": "number", "value": 0}],
    "objects": [sprite("Player", "player.png"), sprite("Coin", "coin.png"), cube],
    "instances": [inst("Player", 100, 300, zOrder=2), inst("Coin", 600, 300), inst("Coin", 900, 200),
                  inst("Spinner", 1000, 80, z=0, customSize=True, width=160, height=160, depth=160,
                       rotationX=35, rotationY=25)],
    "events": [
        std([cond("BuiltinCommonInstructions::Always", [])], [act("Rotate", ["Spinner", "90"])]),
        std([cond("KeyPressed", ["", "Right"])], [act("SetX", ["Player", "+", "400*TimeDelta()"])]),
        std([cond("KeyPressed", ["", "Left"])], [act("SetX", ["Player", "-", "400*TimeDelta()"])]),
        std([cond("KeyPressed", ["", "Up"])], [act("SetY", ["Player", "-", "400*TimeDelta()"])]),
        std([cond("KeyPressed", ["", "Down"])], [act("SetY", ["Player", "+", "400*TimeDelta()"])]),
        std([cond("MouseButtonPressed", ["", "Left"])],
            [act("SetXY", ["Player", "=", "MouseX()-Player.Width()/2", "MouseY()-Player.Height()/2"])]),
        std([cond("CollisionNP", ["Player", "Coin", ""])],
            [act("Delete", ["Coin"]), act("ModVarScene", ["score", "+", "1"]),
             act("ModVarGlobal", ["totalCoins", "+", "1"])]),
        std([cond("VarScene", ["score", ">=", "2"])], [act("Scene", ["", "\"Win\"", ""])]),
    ],
}
win = {
    "name": "Win", "r": 20, "g": 60, "b": 40, "layers": layers, "variables": [],
    "objects": [cube],
    "instances": [inst("Spinner", 560, 280, customSize=True, width=160, height=160, depth=160,
                       rotationX=20, rotationY=40)],
    "events": [
        std([cond("BuiltinCommonInstructions::Always", [])], [act("Rotate", ["Spinner", "120"])]),
        std([cond("Timer", ["", "3", "\"t\""])], [act("Scene", ["", "\"Level1\"", ""])]),
    ],
}
project = {
    "gdVersion": {"major": 5, "minor": 5, "build": 0},
    "properties": {"name": "Demo C++", "windowWidth": 1280, "windowHeight": 720},
    "firstLayout": "Level1",
    "variables": [{"name": "totalCoins", "type": "number", "value": 0}],
    "resources": {"resources": [{"name": "player.png", "file": "player.png", "kind": "image"},
                                {"name": "coin.png", "file": "coin.png", "kind": "image"}]},
    "objects": [], "layouts": [level1, win],
}
json.dump(project, open(os.path.join(OUT, "project.json"), "w"), indent=1)
print("jogo de exemplo gerado em", os.path.abspath(OUT))
