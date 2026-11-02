#!/usr/bin/env python3
"""Exportador (v1): empacota um projeto GDevelop (project.json + recursos) no template do APK.

Uso: export_game.py <project.json> <pasta_de_recursos> [--out android/app/src/main/assets/game]
Depois rode o build do APK (Gradle / GitHub Actions) para gerar o jogo exportado.
"""
import argparse, json, os, shutil, sys

ap = argparse.ArgumentParser()
ap.add_argument("project"); ap.add_argument("resources")
ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "android", "app", "src", "main", "assets", "game"))
a = ap.parse_args()

proj = json.load(open(a.project, encoding="utf-8"))
if os.path.isdir(a.out): shutil.rmtree(a.out)
os.makedirs(a.out)
shutil.copy(a.project, os.path.join(a.out, "project.json"))
missing = []
for r in proj.get("resources", {}).get("resources", []):
    src = os.path.join(a.resources, r["file"])
    if os.path.isfile(src):
        dst = os.path.join(a.out, r["file"]); os.makedirs(os.path.dirname(dst), exist_ok=True); shutil.copy(src, dst)
    else:
        missing.append(r["file"])
print("exportado para", os.path.abspath(a.out))
if missing:
    print("AVISO: recursos não encontrados:", ", ".join(missing)); sys.exit(1)
