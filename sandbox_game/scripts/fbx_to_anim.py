import os
import sys
import struct
import math

try:
    import bpy
    import mathutils
except ImportError:
    print("[Error] This script must be run inside Blender's python environment.")
    print("Usage: blender --background --python fbx_to_anim.py -- <input_fbx_path> <output_anim_path>")
    sys.exit(1)

def export_fbx_to_anim(fbx_path, anim_path):
    # Clear existing objects in the Blender scene
    bpy.ops.wm.read_factory_settings(use_empty=True)

    # Import the FBX file
    print(f"Importing FBX from: {fbx_path}")
    bpy.ops.import_scene.fbx(filepath=fbx_path)

    # Find the armature object
    armature = None
    for obj in bpy.context.scene.objects:
        if obj.type == 'ARMATURE':
            armature = obj
            break

    if not armature:
        print("[Error] No armature found in the FBX file.")
        return False

    print(f"Found Armature: {armature.name}")
    bpy.context.view_layer.objects.active = armature

    # Get bones in order
    bones = armature.data.bones
    bone_names = [bone.name for bone in bones]
    joint_count = len(bones)

    joints_data = []
    # Build list of joints
    for i, bone in enumerate(bones):
        parent_idx = bone_names.index(bone.parent.name) if bone.parent else -1
        
        # Calculate local transform TRS
        # In Blender, local bone matrix is bone.matrix_local relative to parent if it has one
        if bone.parent:
            local_matrix = bone.parent.matrix_local.inverted() @ bone.matrix_local
        else:
            local_matrix = bone.matrix_local

        t, r, s = local_matrix.decompose()

        # Compute inverse bind matrix
        # In Blender, inverse bind matrix is bone.matrix_local inverted
        inv_bind = bone.matrix_local.inverted()

        joints_data.append({
            "name": bone.name,
            "parentIndex": parent_idx,
            "inv_bind": inv_bind,
            "local": local_matrix,
            "t": t,
            "r": r, # Blender quats are w, x, y, z
            "s": s
        })

    # Read active animations/actions
    actions = bpy.data.actions
    clips_data = []

    print(f"Found {len(actions)} animation actions.")
    for action in actions:
        armature.animation_data.action = action
        
        # Get frame range
        start_frame, end_frame = int(action.frame_range[0]), int(action.frame_range[1])
        fps = bpy.context.scene.render.fps
        duration = (end_frame - start_frame) / fps if fps > 0 else 1.0

        channels = []

        # Parse keyframes for each bone
        for joint_idx, bone_name in enumerate(bone_names):
            # Sample translation, rotation, scale at keyframes
            trans_keys = []
            rot_keys = []
            scale_keys = []

            # We sample each frame along the action range to bake the keyframes safely
            for frame in range(start_frame, end_frame + 1):
                time = (frame - start_frame) / fps
                bpy.context.scene.frame_set(frame)

                # Get pose bone transform relative to parent pose bone
                pose_bone = armature.pose.bones[bone_name]
                if pose_bone.parent:
                    local_mat = pose_bone.parent.matrix.inverted() @ pose_bone.matrix
                else:
                    local_mat = pose_bone.matrix

                t, r, s = local_mat.decompose()

                trans_keys.append((time, t))
                rot_keys.append((time, r))
                scale_keys.append((time, s))

            channels.append({
                "jointIndex": joint_idx,
                "trans": trans_keys,
                "rot": rot_keys,
                "scale": scale_keys
            })

        clips_data.append({
            "name": action.name,
            "duration": duration,
            "channels": channels
        })

    # Write the binary animation file (.anim)
    with open(anim_path, "wb") as f:
        # 1. Write Header: Magic + Version
        f.write(b"ANIM")
        f.write(struct.pack("<I", 1)) # Version = 1

        # 2. Write Skeleton Data
        f.write(struct.pack("<I", joint_count))
        for joint in joints_data:
            name_bytes = joint["name"].encode("utf-8")
            f.write(struct.pack("<I", len(name_bytes)))
            f.write(name_bytes)
            f.write(struct.pack("<i", joint["parentIndex"]))
            
            # Inverse bind matrix (16 floats, row-major or column-major. GLM expects column-major.
            # Blender matrices are row-major, so we transpose before writing to match column-major!)
            inv_bind_col = joint["inv_bind"].transposed()
            for r in range(4):
                f.write(struct.pack("<4f", *inv_bind_col[r]))

            # Local transform (16 floats, column-major)
            local_col = joint["local"].transposed()
            for r in range(4):
                f.write(struct.pack("<4f", *local_col[r]))

            # Bind TRS components (glm::quat is x, y, z, w. Blender is w, x, y, z)
            f.write(struct.pack("<3f", *joint["t"]))
            f.write(struct.pack("<4f", joint["r"].x, joint["r"].y, joint["r"].z, joint["r"].w))
            f.write(struct.pack("<3f", *joint["s"]))

        # 3. Write Clips Data
        f.write(struct.pack("<I", len(clips_data)))
        for clip in clips_data:
            name_bytes = clip["name"].encode("utf-8")
            f.write(struct.pack("<I", len(name_bytes)))
            f.write(name_bytes)
            f.write(struct.pack("<f", clip["duration"]))

            f.write(struct.pack("<I", len(clip["channels"])))
            for chan in clip["channels"]:
                f.write(struct.pack("<i", chan["jointIndex"]))

                # Write translation keys
                f.write(struct.pack("<I", len(chan["trans"])))
                for key in chan["trans"]:
                    f.write(struct.pack("<f", key[0]))
                    f.write(struct.pack("<3f", *key[1]))

                # Write rotation keys (Blender w,x,y,z -> GLM x,y,z,w)
                f.write(struct.pack("<I", len(chan["rot"])))
                for key in chan["rot"]:
                    f.write(struct.pack("<f", key[0]))
                    f.write(struct.pack("<4f", key[1].x, key[1].y, key[1].z, key[1].w))

                # Write scale keys
                f.write(struct.pack("<I", len(chan["scale"])))
                for key in chan["scale"]:
                    f.write(struct.pack("<f", key[0]))
                    f.write(struct.pack("<3f", *key[1]))

    print(f"[Success] Exported binary animation file to: {anim_path}")
    return True

if __name__ == "__main__":
    # blender --background --python fbx_to_anim.py -- <input_fbx> <output_anim>
    args = sys.argv
    if "--" in args:
        idx = args.index("--")
        extra_args = args[idx+1:]
        if len(extra_args) >= 2:
            export_fbx_to_anim(extra_args[0], extra_args[1])
            sys.exit(0)
            
    print("Usage: blender --background --python fbx_to_anim.py -- <input_fbx_path> <output_anim_path>")
    sys.exit(1)
