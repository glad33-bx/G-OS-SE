import re
import pathlib

for file in pathlib.Path(".").rglob("*.asm"):
    for line in open(file):
        if ".global" in line:
            print("ASM EXPORT:", file, line.strip())
