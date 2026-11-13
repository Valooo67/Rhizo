#!/usr/bin/env python3
import pathlib, struct, sys
out=pathlib.Path(sys.argv[1]); lines=["#pragma once","#include <cstdint>","namespace rhizo::shaders {"]
for name,path in (("vertex",pathlib.Path(sys.argv[2])),("fragment",pathlib.Path(sys.argv[3])),("shadow_vertex",pathlib.Path(sys.argv[4]))):
 data=path.read_bytes()
 if len(data)%4: raise SystemExit(f"invalid SPIR-V: {path}")
 words=struct.unpack("<"+"I"*(len(data)//4),data)
 lines.append(f"inline constexpr uint32_t {name}[] = {{")
 for i in range(0,len(words),8): lines.append(" "+", ".join(f"0x{x:08x}u" for x in words[i:i+8])+",")
 lines.append("};"); lines.append(f"inline constexpr unsigned {name}_bytes = {len(data)}u;")
lines.append("}"); out.parent.mkdir(parents=True,exist_ok=True); out.write_text("\n".join(lines)+"\n")
