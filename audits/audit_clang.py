import json
import subprocess
import pathlib
import sys

PROJECT_ROOT = pathlib.Path(".")
REPORT = {
    "functions": [],
    "globals": [],
    "includes": [],
    "macros": [],
    "structs": [],
    "typedefs": []
}

def run_clang_ast(file):
    cmd = [
        "clang",
        "-Xclang", "-ast-dump=json",
        "-fsyntax-only",
        str(file)
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return json.loads(result.stdout)

def walk_ast(node, file):
    kind = node.get("kind", "")

    if kind == "FunctionDecl":
        REPORT["functions"].append({
            "file": str(file),
            "name": node.get("name"),
            "type": node.get("type", {}).get("qualType"),
            "line": node.get("loc", {}).get("line"),
            "static": "static" in node.get("storageClass", "")
        })

    elif kind == "VarDecl":
        if node.get("storageClass") != "extern":
            if node.get("isFileVarDecl"):
                REPORT["globals"].append({
                    "file": str(file),
                    "name": node.get("name"),
                    "type": node.get("type", {}).get("qualType"),
                    "line": node.get("loc", {}).get("line")
                })

    elif kind == "RecordDecl":
        if node.get("tagUsed") == "struct":
            REPORT["structs"].append({
                "file": str(file),
                "name": node.get("name"),
                "line": node.get("loc", {}).get("line")
            })

    elif kind == "TypedefDecl":
        REPORT["typedefs"].append({
            "file": str(file),
            "name": node.get("name"),
            "type": node.get("type", {}).get("qualType"),
            "line": node.get("loc", {}).get("line")
        })

    for child in node.get("inner", []):
        walk_ast(child, file)

def extract_includes(file):
    with open(file) as f:
        for line in f:
            if line.strip().startswith("#include"):
                REPORT["includes"].append({
                    "file": str(file),
                    "line": line.strip()
                })

def analyze_file(file):
    print(f"Analyse {file}")
    extract_includes(file)
    ast = run_clang_ast(file)
    if ast:
        walk_ast(ast, file)

def main():
    for file in PROJECT_ROOT.rglob("*.[ch]"):
        analyze_file(file)

    with open("audit_report.json", "w") as f:
        json.dump(REPORT, f, indent=4)

    print("\nAudit terminé → audit_report.json")

if __name__ == "__main__":
    main()
