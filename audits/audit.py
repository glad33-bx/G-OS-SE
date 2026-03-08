import json
import subprocess
import pathlib

REPORT = {
    "exported_functions": [],
    "static_functions": [],
    "globals": [],
    "const_globals": [],
    "structs": [],
    "interrupt_handlers": []
}

def run_clang_ast(file, args):
    cmd = ["clang", "-Xclang", "-ast-dump=json", "-fsyntax-only"]
    cmd += args
    cmd.append(file)

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return json.loads(result.stdout)

def walk(node, file):
    kind = node.get("kind")

    if kind == "FunctionDecl":
        name = node.get("name")
        storage = node.get("storageClass")

        if storage == "static":
            REPORT["static_functions"].append((file, name))
        else:
            REPORT["exported_functions"].append((file, name))

        # Détection ISR basique
        if name and ("irq" in name.lower() or "isr" in name.lower()):
            REPORT["interrupt_handlers"].append((file, name))

    elif kind == "VarDecl":
        if node.get("isFileVarDecl"):
            name = node.get("name")
            qtype = node.get("type", {}).get("qualType", "")

            if "const" in qtype:
                REPORT["const_globals"].append((file, name))
            else:
                REPORT["globals"].append((file, name))

    elif kind == "RecordDecl":
        if node.get("tagUsed") == "struct":
            REPORT["structs"].append((file, node.get("name")))

    for child in node.get("inner", []):
        walk(child, file)

def main():
    compile_db = json.load(open("compile_commands.json"))

    for entry in compile_db:
        file = entry["file"]
        args = entry["arguments"][1:-1]  # enlève clang et fichier

        print("Analyse:", file)

        ast = run_clang_ast(file, args)
        if ast:
            walk(ast, file)

    with open("kernel_audit.json", "w") as f:
        json.dump(REPORT, f, indent=4)

    print("Audit terminé → kernel_audit.json")

if __name__ == "__main__":
    main()
