"""Compile the audio-side classes from the Rack wrappers against the test shim."""
import re
import sys
from pathlib import Path

for filename in sys.argv[1:]:
    path = Path(filename)
    source = path.read_text()
    name = path.stem
    start = re.search(rf"^struct {name}Module : Module \{{", source, re.M).start()
    end = source.index("\n};", start) + len("\n};")
    for include in re.findall(r'^#include "modules/[^"]+"', source, re.M):
        print(include)
    print(f"namespace {name}_wrapper {{")
    for declaration in re.findall(r"^using k280::\w+;", source, re.M):
        print(declaration)
    print(f'#line {source.count(chr(10), 0, start) + 1} "{path}"')
    print(source[start:end])
    print("}")
