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
HTML_REPORT = "audit_memory_ultimate.html"
BSS_WARNING_SIZE = 4096
PAGE_ALIGNMENT = 4096
SCALE = 0.0001

# ------------------------
# Scan fichiers source
# ------------------------
def scan_source_files(path="."):
    files = []
    for ext in SRC_EXTENSIONS:
        files.extend(Path(path).rglob(f"*{ext}"))
    return files

# ------------------------
# Parsing C/H avec ligne
# ------------------------
def parse_c_file(file_path):
    includes, externs, globals_vars, functions = [], [], [], {}
    current_func, brace_level = None, 0
    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
        for lineno, line in enumerate(f,1):
            line_strip = line.strip()
            if line_strip.startswith("#include"):
                includes.append({"line": lineno,"text":line_strip})
            elif line_strip.startswith("extern"):
                externs.append({"line": lineno,"text":line_strip})
            elif re.match(r"^(int|char|uint32_t|uint8_t|void|struct)\s+\**\w+\s*(=\s*[^;]+)?;", line_strip):
                var_name = re.sub(r"^(int|char|uint32_t|uint8_t|void|struct)\s+\**","",line_strip).split('=')[0].strip()
                if current_func is None:
                    globals_vars.append({"name":var_name,"line":lineno,"text":line_strip,"file":str(file_path)})
                else:
                    functions[current_func]["locals"].append({"name":var_name,"line":lineno,"text":line_strip})
            m = re.match(r"^(static\s+)?(int|void|char|uint32_t|uint8_t|struct)\s+(\w+)\s*\(([^)]*)\)\s*{?", line_strip)
            if m:
                current_func = m.group(3)
                functions[current_func] = {"args":m.group(4),"locals":[],"static":bool(m.group(1)),"calls":[],"line":lineno,"file":str(file_path)}
                brace_level = 1 if "{" in line_strip else 0
                continue
            if current_func:
                brace_level += line_strip.count("{") - line_strip.count("}")
                call_match = re.findall(r"(\w+)\s*\(", line_strip)
                for c in call_match: functions[current_func]["calls"].append({"name":c,"line":lineno,"file":str(file_path)})
                if brace_level<=0:
                    current_func = None
                    brace_level = 0
    return {"includes":includes,"externs":externs,"globals":globals_vars,"functions":functions}

# ------------------------
# Parsing ASM avec ligne
# ------------------------
def parse_asm_file(file_path):
    labels, externs, macros, segments = [], [], [], []
    with open(file_path,"r",encoding="utf-8",errors="ignore") as f:
        for lineno,line in enumerate(f,1):
            line_strip = line.strip()
            if line_strip.startswith("extern"): externs.append({"line":lineno,"text":line_strip})
            elif line_strip.endswith(":") and not line_strip.startswith("%"): labels.append({"name":line_strip.rstrip(":"),"line":lineno,"file":str(file_path)})
            elif line_strip.upper().startswith(("%MACRO","%ENDMACRO")): macros.append({"line":lineno,"text":line_strip})
            elif re.match(r"^\.\w+",line_strip): segments.append({"line":lineno,"text":line_strip})
    return {"labels":labels,"externs":externs,"macros":macros,"segments":segments}

# ------------------------
# Analyse ELF
# ------------------------
def nm_symbols(binary):
    symbols = defaultdict(list)
    try:
        out = subprocess.check_output(["nm","-C",binary],text=True)
    except FileNotFoundError:
        print("Erreur : nm introuvable")
        return symbols
    for line in out.splitlines():
        parts = line.split()
        if len(parts)>=3:
            typ,name = parts[-2],parts[-1]
            if typ=="T": symbols["functions"].append(name)
            elif typ=="D": symbols["globals"].append(name)
            elif typ=="B": symbols["bss"].append(name)
            elif typ=="R": symbols["readonly"].append(name)
            elif typ=="U": symbols["undefined"].append(name)
    return symbols

def objdump_sections(binary):
    sections = {}
    try:
        out = subprocess.check_output(["objdump","-h",binary],text=True)
    except FileNotFoundError:
        print("Erreur : objdump introuvable")
        return sections
    for line in out.splitlines():
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)",line)
        if m:
            name = m.group(1)
            size = int(m.group(2),16)
            addr = int(m.group(3),16)
            sections[name] = {"addr":addr,"size":size}
    return sections

# ------------------------
# Carte mémoire détaillée
# ------------------------
def memory_bar_html(sections,file_data):
    html = ["<h2>Visualisation mémoire détaillée (.data / .bss)</h2>",
            "<div style='position:relative;width:95%;height:400px;border:1px solid black;background:#f0f0f0;'>"]
    for sec,color in [(".data","#28a745"),(".bss","#ffc107")]:
        s = sections.get(sec)
        if not s: continue
        addr = s["addr"]
        for f,d in file_data.items():
            for g in d.get("globals",[]):
                left = addr*SCALE
                width = max(1, g.get("size",4)) # approx
                html.append(f"<div title='{g['name']} ({g['line']}) in {f}' style='position:absolute;left:{left}px;width:{width}px;height:40px;background-color:{color};border:1px solid black'></div>")
                addr += width
    html.append("</div>")
    return "\n".join(html)

# ------------------------
# Tableau synthèse utilisation
# ------------------------
def usage_summary_html(file_data):
    html = ["<h2>Tableau synthèse utilisation</h2>",
            "<table><tr><th>Nom</th><th>Type</th><th>Fichier</th><th>Ligne</th><th>Appels / Utilisation</th></tr>"]
    for f,d in file_data.items():
        for g in d.get("globals",[]):
            html.append(f"<tr><td>{g['name']}</td><td>global</td><td>{f}</td><td>{g['line']}</td><td>-</td></tr>")
        for func,info in d.get("functions",{}).items():
            calls_str = ", ".join([f"{c['name']}@{c['file']}:{c['line']}" for c in info.get("calls",[])])
            html.append(f"<tr><td>{func}</td><td>function</td><td>{f}</td><td>{info['line']}</td><td>{calls_str}</td></tr>")
    html.append("</table>")
    return "\n".join(html)

# ------------------------
# Génération HTML complet
# ------------------------
def generate_html_report(file_data,symbols,sections):
    html = ["<html><head><style>",
            "body{font-family:monospace;} table{border-collapse:collapse;} td,th{border:1px solid black;padding:4px;}",
            ".text{background-color:#cce5ff;}.data{background-color:#d4edda;}.bss{background-color:#fff3cd;}",
            ".function{background-color:#f8d7da;}.global{background-color:#e2e3e5;} .alert{color:red;font-weight:bold;}","</style></head><body>"]
    html.append("<h1>Audit mémoire Kernel x86 32 bits – version ultime</h1>")

    html.append("<h2>Sections ELF</h2><table><tr><th>Section</th><th>Adresse</th><th>Taille</th></tr>")
    for sec,info in sections.items():
        cls = ".text" if sec.startswith(".text") else ".data" if sec.startswith(".data") else ".bss" if sec.startswith(".bss") else ""
        html.append(f"<tr class='{cls}'><td>{sec}</td><td>0x{info['addr']:08x}</td><td>{info['size']} bytes</td></tr>")
    html.append("</table>")

    html.append(memory_bar_html(sections,file_data))
    html.append(usage_summary_html(file_data))

    # Alertes
    html.append("<h2>Alertes</h2><ul>")
    unused_globals = [g['name'] for f in file_data.values() for g in f.get("globals",[]) if g['name'] not in symbols.get("globals",[]) and g['name'] not in symbols.get("bss",[])]
    candidates_static = [f for f in symbols.get("functions",[]) if f not in symbols.get("undefined",[])]
    for sec in [".bss",".data"]:
        if sections.get(sec,{}).get("size",0)>=BSS_WARNING_SIZE: html.append(f"<li class='alert'>Buffer volumineux: {sec} {sections[sec]['size']} bytes</li>")
    alignment_warn = [f"{s} @0x{info['addr']:08x} non aligné" for s,info in sections.items() if info["addr"]%PAGE_ALIGNMENT!=0]
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
        if f.suffix in [".c",".h"]: file_data[str(f)] = parse_c_file(f)
        elif f.suffix==".asm": file_data[str(f)] = parse_asm_file(f)
    symbols = nm_symbols(KERNEL_ELF)
    sections = objdump_sections(KERNEL_ELF)
    html_content = generate_html_report(file_data,symbols,sections)
    with open(HTML_REPORT,"w",encoding="utf-8") as f: f.write(html_content)
    print(f"Audit mémoire ultime généré : {HTML_REPORT}")

if __name__=="__main__":
    main()
