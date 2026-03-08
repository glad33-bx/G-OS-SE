import os
import re
import subprocess
from pathlib import Path
from collections import defaultdict
from html import escape

# ------------------------
# Configuration
# ------------------------
SRC_EXTENSIONS = [".c", ".h", ".asm"]
KERNEL_ELF = "bin/kernel.bin"
HTML_REPORT = "audit_memory_report.html"
BSS_WARNING_SIZE = 4096  # 4 KB seuil pour buffer volumineux
PAGE_ALIGNMENT = 4096    # 4 KB

# ------------------------
# Fonctions d'analyse
# ------------------------
def scan_source_files(path="."):
    files = []
    for ext in SRC_EXTENSIONS:
        files.extend(Path(path).rglob(f"*{ext}"))
    return files

def parse_c_file(file_path):
    includes, externs, globals_vars, functions = [], [], [], {}
    current_func, brace_level = None, 0
    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line_strip = line.strip()
            if line_strip.startswith("#include"): includes.append(line_strip)
            elif line_strip.startswith("extern"): externs.append(line_strip)
            elif re.match(r"^(int|char|uint32_t|uint8_t|void|struct)\s+\**\w+\s*(=\s*[^;]+)?;", line_strip):
                if current_func is None: globals_vars.append(line_strip)
                else: functions[current_func]["locals"].append(line_strip)
            m = re.match(r"^(static\s+)?(int|void|char|uint32_t|uint8_t|struct)\s+(\w+)\s*\(([^)]*)\)\s*{?", line_strip)
            if m:
                current_func = m.group(3)
                functions[current_func] = {"args": m.group(4), "locals": [], "static": bool(m.group(1)), "calls": []}
                brace_level = 1 if "{" in line_strip else 0
                continue
            if current_func:
                brace_level += line_strip.count("{") - line_strip.count("}")
                call_match = re.findall(r"(\w+)\s*\(", line_strip)
                for c in call_match: functions[current_func]["calls"].append(c)
                if brace_level <= 0:
                    current_func = None
                    brace_level = 0
    return {"includes": includes, "externs": externs, "globals": globals_vars, "functions": functions}

def parse_asm_file(file_path):
    labels, externs, macros, segments = [], [], [], []
    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line_strip = line.strip()
            if line_strip.startswith("extern"): externs.append(line_strip)
            elif line_strip.endswith(":") and not line_strip.startswith("%"): labels.append(line_strip.rstrip(":"))
            elif line_strip.upper().startswith(("%MACRO", "%ENDMACRO")): macros.append(line_strip)
            elif re.match(r"^\.\w+", line_strip): segments.append(line_strip)
    return {"labels": labels, "externs": externs, "macros": macros, "segments": segments}

def nm_symbols(binary):
    symbols = defaultdict(list)
    try:
        out = subprocess.check_output(["nm", "-C", binary], text=True)
    except FileNotFoundError:
        print("Erreur : nm introuvable")
        return symbols
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            typ, name = parts[-2], parts[-1]
            if typ == "T": symbols["functions"].append(name)
            elif typ == "D": symbols["globals"].append(name)
            elif typ == "B": symbols["bss"].append(name)
            elif typ == "R": symbols["readonly"].append(name)
            elif typ == "U": symbols["undefined"].append(name)
    return symbols

def objdump_sections(binary):
    sections = {}
    try:
        out = subprocess.check_output(["objdump", "-h", binary], text=True)
    except FileNotFoundError:
        print("Erreur : objdump introuvable")
        return sections
    for line in out.splitlines():
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)", line)
        if m:
            name = m.group(1)
            size = int(m.group(2), 16)
            addr = int(m.group(3), 16)
            sections[name] = {"addr": addr, "size": size}
    return sections

# ------------------------
# Visualisation mémoire
# ------------------------
def memory_bar_html(sections, symbols):
    html = ["<h2>Visualisation mémoire (.data / .bss)</h2>"]
    html.append("<div style='position:relative;width:90%;height:400px;border:1px solid black;background:#f0f0f0;'>")

    # Sélection des sections pertinentes
    for sec in [".data", ".bss"]:
        info = sections.get(sec)
        if not info: continue
        start = info["addr"]
        size = info["size"]
        scale = 0.0001  # Ajuste l’échelle pour largeur pixels
        left = start * scale
        width = max(size * scale, 2)
        color = "#28a745" if sec==".data" else "#ffc107"
        html.append(f"<div title='{sec} {size} bytes @0x{start:08x}' "
                    f"style='position:absolute;left:{left}px;width:{width}px;height:40px;background-color:{color};border:1px solid black'>{sec}</div>")

    html.append("</div>")
    return "\n".join(html)

# ------------------------
# Génération HTML complet
# ------------------------
def generate_html_report(file_data, symbols, sections):
    html = ["<html><head><style>",
            "body{font-family:monospace;} table{border-collapse:collapse;} td,th{border:1px solid black;padding:4px;}",
            ".text{background-color:#cce5ff;}.data{background-color:#d4edda;}.bss{background-color:#fff3cd;}",
            ".function{background-color:#f8d7da;}.global{background-color:#e2e3e5;} .alert{color:red;font-weight:bold;}","</style></head><body>"]
    html.append("<h1>Audit Mémoire Kernel x86 32 bits</h1>")

    html.append("<h2>Sections ELF</h2><table><tr><th>Section</th><th>Adresse</th><th>Taille</th></tr>")
    for sec, info in sections.items():
        cls = ".text" if sec.startswith(".text") else ".data" if sec.startswith(".data") else ".bss" if sec.startswith(".bss") else ""
        html.append(f"<tr class='{cls}'><td>{sec}</td><td>0x{info['addr']:08x}</td><td>{info['size']} bytes</td></tr>")
    html.append("</table>")

    html.append(memory_bar_html(sections, symbols))

    # Fichiers source
    html.append("<h2>Fichiers source</h2>")
    for f, data in file_data.items():
        html.append(f"<h3>{escape(f)}</h3><table><tr><th>Type</th><th>Contenu</th></tr>")
        for inc in data.get("includes", []): html.append(f"<tr><td>Include</td><td>{escape(inc)}</td></tr>")
        for ext in data.get("externs", []): html.append(f"<tr><td>Extern</td><td>{escape(ext)}</td></tr>")
        for glob in data.get("globals", []): html.append(f"<tr class='global'><td>Global</td><td>{escape(glob)}</td></tr>")
        for func, info in data.get("functions", {}).items():
            html.append(f"<tr class='function'><td>Function</td><td>{escape(func)}({escape(info.get('args',''))})</td></tr>")
            for local in info.get("locals", []): html.append(f"<tr><td>Local</td><td>{escape(local)}</td></tr>")
            for call in info.get("calls", []): html.append(f"<tr><td>Call</td><td>{escape(call)}</td></tr>")
        for label in data.get("labels", []): html.append(f"<tr><td>ASM Label</td><td>{escape(label)}</td></tr>")
        for macro in data.get("macros", []): html.append(f"<tr><td>ASM Macro</td><td>{escape(macro)}</td></tr>")
        for seg in data.get("segments", []): html.append(f"<tr><td>ASM Segment</td><td>{escape(seg)}</td></tr>")
        html.append("</table>")

    # Alertes
    html.append("<h2>Alertes</h2><ul>")
    unused_globals = [g for f in file_data.values() for g in f.get("globals", []) if g not in symbols.get("globals", []) and g not in symbols.get("bss", [])]
    candidates_static = [f for f in symbols.get("functions", []) if f not in symbols.get("undefined", [])]
    for sec in [".bss", ".data"]:
        if sections.get(sec, {}).get("size",0) >= BSS_WARNING_SIZE: html.append(f"<li class='alert'>Buffer volumineux: {sec} {sections[sec]['size']} bytes</li>")
    alignment_warn = [f"{s} @0x{info['addr']:08x} non aligné" for s,info in sections.items() if info["addr"] % PAGE_ALIGNMENT != 0]

    if unused_globals: html.append(f"<li class='alert'>Variables globales inutilisées: {', '.join(unused_globals)}</li>")
    if candidates_static: html.append(f"<li class='alert'>Fonctions candidates static: {', '.join(candidates_static)}</li>")
    for w in alignment_warn: html.append(f"<li class='alert'>Alignment warning: {w}</li>")
    html.append("</ul></body></html>")
    return "\n".join(html)

# ------------------------
# Main
# ------------------------
def main():
    files = scan_source_files(".")
    file_data = {}

    for f in files:
        if f.suffix in [".c", ".h"]: file_data[str(f)] = parse_c_file(f)
        elif f.suffix == ".asm": file_data[str(f)] = parse_asm_file(f)

    symbols = nm_symbols(KERNEL_ELF)
    sections = objdump_sections(KERNEL_ELF)
    html_content = generate_html_report(file_data, symbols, sections)
    with open(HTML_REPORT, "w", encoding="utf-8") as f: f.write(html_content)
    print(f"Audit mémoire visuel généré : {HTML_REPORT}")

if __name__ == "__main__":
    main()
