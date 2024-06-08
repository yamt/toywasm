import json
import sys


def dump_table_type(tableidx, typestr):
    print(
        f'subgraph cluster_table_{tableidx} {{ "table{tableidx}_{typestr}" [label="{typestr}",shape=box]; label=table_{tableidx}; color=purple }}'
    )


j = json.load(sys.stdin)
print("strict digraph {")
funcs = j["funcs"]
for f in funcs:
    imported = f["imported"]
    if imported:
        color = "blue"
    else:
        color = "black"
    label = f["name"]
    print(f"f{f['idx']} [label=\"{label}\",color={color}]")
    if imported:
        continue
    for c in f["calls"]:
        if "callee" in c:
            print(f"f{f['idx']} -> f{c['callee']}")
        else:
            tableidx = c["table"]
            typestr = c["type"]
            dump_table_type(tableidx, typestr)
            print(f"f{f['idx']} -> \"table{tableidx}_{typestr}\" [color=purple]")
for e in j["elements"]:
    tableidx = e["tableidx"]
    funcidx = e["funcidx"]
    typestr = funcs[funcidx]["type"]
    dump_table_type(tableidx, typestr)
    print(f'"table{tableidx}_{typestr}" -> f{funcidx} [color=purple]')
for im in j["imports"]:
    module_name = im["module_name"]
    name = im["name"]
    idx = im["idx"]
    print(
        f'subgraph "cluster_import_{module_name}" {{ import{idx} [label="{name}",shape=rectangle]; label="{module_name}"; color=blue;rank=sink }}'
    )
    print(f"f{idx} -> import{idx} [color=blue]")
for ex in j["exports"]:
    name = ex["name"]
    idx = ex["idx"]
    print(
        f'subgraph cluster_export {{ export{idx} [label="{name}",shape=rectangle]; label=exports; color=red;rank=source }}'
    )
    print(f"export{idx} -> f{idx} [color=red]")
# group the start function together with exports as
# their functionalites are similar for our purpose.
start = j.get("start")
if start is not None:
    print(
        f"subgraph cluster_export {{ start [shape=plaintext]; label=exports; color=red;rank=source }}"
    )
    print(f"start -> f{start} [color=red]")
print("}")
