# eg.
# callgraph a.wasm|python3 func_size.py|sort -nr|head -10

import json
import sys


j = json.load(sys.stdin)
funcs = j["funcs"]
for f in funcs:
    if "expr_size" not in f:
        continue
    print(f"{f['expr_size']} {f['name']}")
