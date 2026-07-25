"""Builds a real Blender Armature from this project's own JSON skeleton
dump (see tools/dummyclient's --dump-anim-json-out).

Design note (see NOTES.md for the full reasoning): every edit bone is
given an IDENTICAL, trivial rest shape (head at the armature origin, a
tiny fixed-length tail, no roll, not connected to its parent). Since every
bone shares the exact same armature-space rest position/orientation, each
bone's rest transform RELATIVE TO ITS OWN PARENT (Bone.matrix_local) comes
out to exactly identity, regardless of hierarchy depth. That deliberately
makes the edit-bone geometry carry NO transform of its own - the real bind
pose (bindTranslation + preRotation*postRotation) and any later animation
are applied entirely through pose-space (PoseBone.location /
rotation_quaternion), which is what Blender's own hierarchy composition
combines with the parent chain when computing each bone's real
armature-space matrix. That's the one thing this toolkit exists to test
independently against this project's own C++ composition code - so the
edit-bone rest shape itself must not be able to interfere with it.
"""

import json

import bpy

from . import directxmath_compat as xm

_IDENTITY_QUAT = (0.0, 0.0, 0.0, 1.0)  # DirectXMath (x, y, z, w) order


def load_skeleton_json(json_path):
    with open(json_path, "r", encoding="utf-8") as f:
        return json.load(f)


def build_armature(data, armature_name="SWG_Skeleton"):
    """Creates a new Armature object from a loaded JSON dump (see
    load_skeleton_json), with every pose bone set to its real bind pose.
    Returns the new armature Object."""
    arm_data = bpy.data.armatures.new(armature_name)
    arm_obj = bpy.data.objects.new(armature_name, arm_data)
    bpy.context.collection.objects.link(arm_obj)

    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="EDIT")

    bones = data["bones"]
    edit_bones = []
    for bone in bones:
        eb = arm_data.edit_bones.new(bone["name"])
        eb.head = (0.0, 0.0, 0.0)
        eb.tail = (0.0, 0.05, 0.0)  # identical fixed stub for every bone - see module docstring
        eb.roll = 0.0
        eb.use_connect = False
        edit_bones.append(eb)

    for i, bone in enumerate(bones):
        parent_idx = bone["parentIndex"]
        if parent_idx is not None and parent_idx >= 0:
            edit_bones[i].parent = edit_bones[parent_idx]

    bpy.ops.object.mode_set(mode="POSE")

    for bone in bones:
        pb = arm_obj.pose.bones[bone["name"]]
        pb.rotation_mode = "QUATERNION"
        pre_rot = tuple(bone["preRotation"])
        post_rot = tuple(bone["postRotation"])
        bind_rotation = xm.compose_local_rotation(pre_rot, _IDENTITY_QUAT, post_rot)
        pb.rotation_quaternion = xm.to_blender_quat(bind_rotation)
        pb.location = tuple(bone["bindTranslation"])

    bpy.ops.object.mode_set(mode="OBJECT")
    return arm_obj


class SWG_OT_import_skeleton(bpy.types.Operator):
    """Import a real SWG skeleton (from a JSON dump produced by dummyclient
    --dump-anim-json-out) as a Blender Armature, set to its real bind pose."""

    bl_idname = "swg.import_skeleton"
    bl_label = "SWG: Import Skeleton (from JSON dump)"
    bl_options = {"REGISTER", "UNDO"}

    filepath: bpy.props.StringProperty(subtype="FILE_PATH")

    def execute(self, context):
        if not self.filepath:
            self.report({"ERROR"}, "No JSON dump path set")
            return {"CANCELLED"}
        data = load_skeleton_json(self.filepath)
        arm_obj = build_armature(data)
        context.scene["swg_last_imported_armature"] = arm_obj.name
        context.scene["swg_last_json_dump"] = self.filepath
        self.report({"INFO"}, f"Imported {len(data['bones'])} bones as '{arm_obj.name}'")
        return {"FINISHED"}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}
