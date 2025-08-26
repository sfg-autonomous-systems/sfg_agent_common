import math
import os
import sys
from pathlib import Path

import bpy


def optimize_collada_mesh(input_filepath: Path, output_filepath: Path) -> None:
    print(f"Optimizing {input_filepath}...")

    # Clear the scene.
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    bpy.ops.wm.collada_import(filepath=input_filepath.as_posix())

    # Optimize each imported mesh.
    for obj in bpy.context.selected_objects:
        if obj.type == "MESH":
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.select_all(action="SELECT")
            bpy.ops.mesh.remove_doubles(
                threshold=0.0005, use_sharp_edge_from_normals=True
            )
            bpy.ops.object.mode_set(mode="OBJECT")
            bpy.ops.object.shade_auto_smooth(use_auto_smooth=False)

    if output_filepath.exists():
        output_filepath = output_filepath.with_suffix(".optimized.dae")

    bpy.ops.wm.collada_export(filepath=output_filepath.as_posix())


def batch_optimize_collada_meshes(
    input_driectory: Path, output_directory: Path
) -> None:
    print(f"Batch optimizing meshes in '{input_driectory}' to '{output_directory}'...")

    os.makedirs(output_directory, exist_ok=True)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    # Process each file in the input folder.
    for filename in os.listdir(input_driectory):
        if not filename.lower().endswith(".dae"):
            continue

        optimize_collada_mesh(input_driectory / filename, output_directory / filename)


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []

    if len(argv) < 2:
        raise Exception(
            "Usage: blender --background --python script.py -- <input_folder|input_dae> <output_folder|output_dae>"
        )

    input_path, output_path = Path(argv[0]), Path(argv[1])

    if (
        input_path.is_file()
        and input_path.suffix.lower() == ".dae"
        and output_path.suffix.lower() == ".dae"
    ):
        optimize_collada_mesh(input_path, output_path)
    elif input_path.is_dir() and output_path.is_dir():
        batch_optimize_collada_meshes(input_path, output_path)
    else:
        raise Exception("Invalid input/output paths.")

    print("Done.")
