import subprocess
from collections import defaultdict

binary = "bin/kernel.bin"

out = subprocess.check_output(["nm", "-C", binary], text=True)

functions = []
for line in out.splitlines():
    parts = line.split()
    if len(parts) >= 3:
        typ = parts[-2]
        name = parts[-1]
        if typ == "T":
            functions.append(name)

# Chercher références croisées
out = subprocess.check_output(["nm", "-C", binary], text=True)

usage_count = defaultdict(int)

for line in out.splitlines():
    for f in functions:
        if f in line:
            usage_count[f] += 1

print("=== CANDIDATS STATIC ===")
for f in functions:
    if usage_count[f] <= 1:
        print(f)
