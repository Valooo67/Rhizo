#!/usr/bin/env python3
import pathlib
import struct
import sys

out = pathlib.Path(sys.argv[1])
names = ("vertex", "fragment", "instanced_vertex", "composite_vertex", "composite_fragment", "shadow_vertex", "skybox_vertex", "skybox_fragment")
lines = ["#pragma once", "#include <cstdint>", "namespace rhizo::shaders {"]
if len(sys.argv) != len(names) + 2:
    raise SystemExit("expected output header and eight SPIR-V inputs")
for name, source in zip(names, sys.argv[2:]):
    path = pathlib.Path(source)
    data = path.read_bytes()
    if len(data) % 4:
        raise SystemExit(f"invalid SPIR-V: {path}")
    words = struct.unpack("<" + "I" * (len(data) // 4), data)
    lines.append(f"inline constexpr uint32_t {name}[] = {{")
    for index in range(0, len(words), 8):
        lines.append(" " + ", ".join(f"0x{word:08x}u" for word in words[index:index + 8]) + ",")
    lines.append("};")
    lines.append(f"inline constexpr unsigned {name}_bytes = {len(data)}u;")
lines.append("}")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("\n".join(lines) + "\n")
