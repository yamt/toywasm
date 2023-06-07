# expected usage:
# python3 test/js_wasm_url.py .js.wasm/data.json mozilla-release

import json
import sys

fp = open(sys.argv[1])
branch = sys.argv[2]
d = json.load(fp)
for e in d:
    if e["branch"] == branch:
        print(e["url"])
        exit(0)

exit(1)
