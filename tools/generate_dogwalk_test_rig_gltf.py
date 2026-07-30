#!/usr/bin/env python3
"""Generate minimal skinned glTF (simple_skin layout) with idle + walk clips."""

import base64
import json
import math
import struct
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
OUT_PATH = REPO_ROOT / "engine/Resources/Fixtures/dogwalk_test_rig/dogwalk_test_rig.gltf"


def f32_le(values):
    return struct.pack(f"<{len(values)}f", *values)


def quat_z(degrees):
    half = math.radians(degrees) * 0.5
    return [0.0, 0.0, math.sin(half), math.cos(half)]


def data_uri(blob: bytes) -> str:
    return "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")


def main():
    mesh_blob = base64.b64decode(
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAgADAAUAAgAFAAQABAAFAAcABAAHAAYABgAHAAkABgAJAAgAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAD8AAAAAAACAPwAAAD8AAAAAAAAAAAAAgD8AAAAAAACAPwAAgD8AAAAAAAAAAAAAwD8AAAAAAACAPwAAwD8AAAAAAAAAAAAAAEAAAAAAAACAPwAAAEAAAAAA"
    )
    skin_blob = base64.b64decode(
        "AAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAABAPwAAgD4AAAAAAAAAAAAAQD8AAIA+AAAAAAAAAAAAAAA/AAAAPwAAAAAAAAAAAAAAPwAAAD8AAAAAAAAAAAAAgD4AAEA/AAAAAAAAAAAAAIA+AABAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAA="
    )
    ibm_blob = base64.b64decode(
        "AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAvwAAgL8AAAAAAACAPwAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAL8AAIC/AAAAAAAAgD8="
    )
    idle_blob = f32_le([0.0, 1.0]) + f32_le(quat_z(0.0) + quat_z(10.0))
    walk_blob = f32_le([0.0, 0.5]) + f32_le(quat_z(0.0) + quat_z(40.0))

    gltf = {
        "asset": {"version": "2.0", "generator": "dogwalk_test_rig generator"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": "Character", "skin": 0, "mesh": 0, "children": [1]},
            {"name": "Hips", "children": [2], "translation": [0.0, 1.0, 0.0]},
            {"name": "Leg", "rotation": [0.0, 0.0, 0.0, 1.0]},
        ],
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 1,
                            "JOINTS_0": 2,
                            "WEIGHTS_0": 3,
                        },
                        "indices": 0,
                    }
                ]
            }
        ],
        "skins": [{"inverseBindMatrices": 4, "joints": [1, 2]}],
        "animations": [
            {
                "name": "idle",
                "channels": [
                    {
                        "sampler": 0,
                        "target": {"node": 2, "path": "rotation"},
                    }
                ],
                "samplers": [
                    {
                        "input": 5,
                        "interpolation": "STEP",
                        "output": 6,
                    }
                ],
            },
            {
                "name": "walk",
                "channels": [
                    {
                        "sampler": 0,
                        "target": {"node": 2, "path": "rotation"},
                    }
                ],
                "samplers": [
                    {
                        "input": 7,
                        "interpolation": "LINEAR",
                        "output": 8,
                    }
                ],
            },
        ],
        "buffers": [
            {"byteLength": len(mesh_blob), "uri": data_uri(mesh_blob)},
            {"byteLength": len(skin_blob), "uri": data_uri(skin_blob)},
            {"byteLength": len(ibm_blob), "uri": data_uri(ibm_blob)},
            {"byteLength": len(idle_blob), "uri": data_uri(idle_blob)},
            {"byteLength": len(walk_blob), "uri": data_uri(walk_blob)},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 48, "target": 34963},
            {"buffer": 0, "byteOffset": 48, "byteLength": 120, "target": 34962},
            {"buffer": 1, "byteOffset": 0, "byteLength": 320, "byteStride": 16},
            {"buffer": 2, "byteOffset": 0, "byteLength": 128},
            {"buffer": 3, "byteOffset": 0, "byteLength": 8},
            {"buffer": 3, "byteOffset": 8, "byteLength": 32},
            {"buffer": 4, "byteOffset": 0, "byteLength": 8},
            {"buffer": 4, "byteOffset": 8, "byteLength": 32},
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5123,
                "count": 24,
                "type": "SCALAR",
                "max": [9],
                "min": [0],
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": 10,
                "type": "VEC3",
                "max": [1.0, 2.0, 0.0],
                "min": [0.0, 0.0, 0.0],
            },
            {
                "bufferView": 2,
                "componentType": 5123,
                "count": 10,
                "type": "VEC4",
                "max": [0, 1, 0, 0],
                "min": [0, 1, 0, 0],
            },
            {
                "bufferView": 2,
                "byteOffset": 160,
                "componentType": 5126,
                "count": 10,
                "type": "VEC4",
                "max": [1.0, 1.0, 0.0, 0.0],
                "min": [0.0, 0.0, 0.0, 0.0],
            },
            {
                "bufferView": 3,
                "componentType": 5126,
                "count": 2,
                "type": "MAT4",
                "max": [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -0.5, -1.0, 0.0, 1.0],
                "min": [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -0.5, -1.0, 0.0, 1.0],
            },
            {
                "bufferView": 4,
                "componentType": 5126,
                "count": 2,
                "type": "SCALAR",
                "max": [1.0],
                "min": [0.0],
            },
            {
                "bufferView": 5,
                "componentType": 5126,
                "count": 2,
                "type": "VEC4",
            },
            {
                "bufferView": 6,
                "componentType": 5126,
                "count": 2,
                "type": "SCALAR",
                "max": [0.5],
                "min": [0.0],
            },
            {
                "bufferView": 7,
                "componentType": 5126,
                "count": 2,
                "type": "VEC4",
            },
        ],
    }

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(gltf, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()
