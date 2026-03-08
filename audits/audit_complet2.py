import os
import re
import subprocess
from pathlib import Path
from collections import defaultdict

# ------------------------
# Configuration
# ------------------------
SRC_EXTENSIONS = [".c", ".h", ".asm"]
KERNEL_ELF = "bin/kernel.bin"

# ------------------------
# Helper functions
# ------------------------
def scan_source_files(path="."):
    files = []
    for ext in SRC_EXTENSIONS:
        files.extend(Path(path).rglob(f"*{ext}"))
    return files

def parse_c_file(file_path):
    """Retourne includes, extern, globals, fonctions et variables locales"""
    includes = []
    externs = []
    globals_vars = []
    functions = {}
    current_func = None
    brace_level = 0

    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    for line in lines:
        line = line.strip()
        if line.startswith("#include"):
            includes.append(line)
        elif line.startswith("extern"):
            externs.append(line)
        # Variables globales simples
        elif re.match(r"^(int|char|uint32_t|uint8_t|void|struct)\s+\**\w+\s*(=\s*[^;]+)?;", line):
            if current_func is None:
                globals_vars.append(line)
            else:
                functions[current_func]["locals"].append(line)
        # Fonction
        m = re.match(r"^(static\s+)?(int|void|char|uint32_t|uint8_t|struct)\s+(\w+)\s*\(([^)]*)\)\s*{?", line)
        if m:
            current_func = m.group(3)
            functions[current_func] = {
                "args": m.group(4),
                "locals": [],
                "static": bool(m.group(1))
            }
            # Vérifier si { est sur la même ligne
            if "{" in line:
                brace_level = 1
            else:
                brace_level = 0
            continue
        # Suivi des accolades pour savoir si on quitte la fonction
        if current_func:
            brace_level += line.count("{") - line.count("}")
            if brace_level <= 0:
                current_func = None
                brace_level = 0
    return {
        "includes": includes,
        "externs": externs,
        "globals": globals_vars,
        "functions": functions
    }

def parse_asm_file(file_path):
    """Retourne labels et externs"""
    labels = []
    externs = []
    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if line.startswith("extern"):
                externs.append(line)
            elif re.match(r"^\w+:", line):
                labels.append(line.rstrip(":"))
    return {"labels": labels, "externs": externs}

def nm_symbols(binary):
    """Analyse ELF avec nm"""
    try:
        out = subprocess.check_output(["nm", "-C", binary], text=True)
    except FileNotFoundError:
        print("Erreur : nm introuvable")
        return {}
    symbols = defaultdict(list)
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            typ = parts[-2]
            name = parts[-1]
            if typ == "T":
                symbols["functions"].append(name)
            elif typ == "D":
                symbols["globals"].append(name)
            elif typ == "B":
                symbols["bss"].append(name)
            elif typ == "R":
                symbols["readonly"].append(name)
            elif typ == "U":
                symbols["undefined"].append(name)
    return symbols

def detect_unused_globals(all_globals, symbols):
    """Renvoie la liste de globals non référencés"""
    unused = []
    for g in all_globals:
        # heuristique simple : si le nom exact n'apparaît pas dans nm
        if g not in symbols.get("globals", []) and g not in symbols.get("bss", []):
            unused.append(g)
    return unused

def detect_candidates_static(all_functions, symbols):
    """Fonctions globales définies mais jamais appelées ailleurs"""
    candidates = []
    for f in symbols.get("functions", []):
        # si fonction globale n'est pas utilisée ailleurs
        if f not in symbols.get("undefined", []):
            candidates.append(f)
    return candidates

# ------------------------
# Analyse principale
# ------------------------
def main():
    print("=== AUDIT DES FICHIERS SOURCE ===")
    files = scan_source_files(".")
    all_globals = []
    all_functions = []

    for f in files:
        print(f"\n--- {f} ---")
        if f.suffix in [".c", ".h"]:
            data = parse_c_file(f)
            print("Includes:", data["includes"])
            print("Externs:", data["externs"])
            print("Globals:", data["globals"])
            all_globals.extend([re.sub(r"^(int|char|uint32_t|uint8_t|void|struct)\s+\**", "", v).split('=')[0].strip() for v in data["globals"]])
            print("Functions:", list(data["functions"].keys()))
            all_functions.extend(data["functions"].keys())
            for func, info in data["functions"].items():
                if info["locals"]:
                    print(f"  Locals in {func}: {info['locals']}")
        elif f.suffix == ".asm":
            data = parse_asm_file(f)
            print("ASM Labels:", data["labels"])
            print("ASM Externs:", data["externs"])

    print("\n=== AUDIT ELF SYMBOLS ===")
    symbols = nm_symbols(KERNEL_ELF)
    for k, v in symbols.items():
        print(f"{k.upper()} ({len(v)}): {v[:10]}{'...' if len(v) > 10 else ''}")

    # Détecter fonctions globales non-static candidates
    candidates_static = detect_candidates_static(all_functions, symbols)
    print("\n=== FONCTIONS CANDIDATES STATIC ===")
    print(candidates_static[:20], "..." if len(candidates_static) > 20 else "")

    # Détecter variables globales inutilisées
    unused_globals = detect_unused_globals(all_globals, symbols)
    print("\n=== VARIABLES GLOBALES INUTILISEES ===")
    print(unused_globals[:20], "..." if len(unused_globals) > 20 else "")

if __name__ == "__main__":
    main()
