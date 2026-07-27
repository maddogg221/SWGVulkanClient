"""Builds a real Blender Action (keyframed pose-bone rotations) from this
project's own JSON animation-clip dump, driving the armature built by
skeleton_import.py.

Deliberately keyframes the exact same real frame numbers the source data
has, using this project's own ported composition formula (see
directxmath_compat.py) - not Blender's own quaternion interpolation for
anything other than viewing motion between real keyframes. For the actual
numeric comparison (compare.py), always park the playhead on a frame
number that's an exact real keyframe for the bones being compared, so
neither side is interpolating - see compare.py's own note on this.

Bones with no rotationKeyframes in the dump (e.g. lwrist/lulna in the real
idle clip - confirmed live 2026-07-24, see NOTES.md) are left exactly as
skeleton_import.py set them (their real bind rotation) - this matches
SkeletalPose.cpp's own behavior exactly: no clip channel means no
animation, not an error.
"""

import bpy

from . import directxmath_compat as xm

_IDENTITY_QUAT = (0.0, 0.0, 0.0, 1.0)


def apply_animation(arm_obj, data, action_name="SWG_Clip"):
    action = bpy.data.actions.new(action_name)
    if arm_obj.animation_data is None:
        arm_obj.animation_data_create()
    arm_obj.animation_data.action = action

    # Real bug found live (2026-07-25/26): the JSON dump's skeleton section
    # uses mixed-case real bone names (e.g. "lThigh") while its clip section
    # uses lowercase real names (e.g. "lthigh") - a real, confirmed casing
    # difference between the two real sources this dump pulls from, NOT a
    # bug in either C++ parser (this project's own bindClipBoneIndices()
    # already matches case-insensitively - see SkeletalPose.cpp - so the
    # live rendering path was never affected). This toolkit's own lookup
    # was case-SENSITIVE, silently skipping every real bone whose clip name
    # and skeleton name happened to differ in case - which turned out to be
    # most of the leg chain, so only 5 of ~30+ real animated bones were
    # ever actually keyframed. Matched case-insensitively now, same
    # convention as the C++ side.
    bones_by_name_lower = {b["name"].lower(): b for b in data["bones"]}
    pose_bones_by_name_lower = {pb.name.lower(): pb for pb in arm_obj.pose.bones}
    animated_count = 0

    for clip_bone in data["clipBones"]:
        bone_name = clip_bone["boneName"]
        skel_bone = bones_by_name_lower.get(bone_name.lower())
        if skel_bone is None:
            continue  # real clip bone with no matching skeleton bone - skip, same as this
            # project's own bindClipBoneIndices() falling back to -1
        keyframes = clip_bone["rotationKeyframes"]
        if not keyframes:
            continue
        pb = pose_bones_by_name_lower.get(bone_name.lower())
        if pb is None:
            continue
        pre_rot = tuple(skel_bone["preRotation"])
        post_rot = tuple(skel_bone["postRotation"])
        for kf in keyframes:
            anim_rot = tuple(kf["rotation"])
            composed = xm.compose_local_rotation(pre_rot, anim_rot, post_rot)
            pb.rotation_quaternion = xm.to_blender_quat(composed)
            pb.keyframe_insert(data_path="rotation_quaternion", frame=kf["frame"], group=bone_name)
        animated_count += 1

    return action, animated_count


class SWG_OT_import_animation(bpy.types.Operator):
    """Apply a real SWG animation clip (from the same JSON dump used for
    SWG: Import Skeleton) to the currently-selected armature as a real
    Blender Action."""

    bl_idname = "swg.import_animation"
    bl_label = "SWG: Import Animation (from same JSON dump)"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        arm_obj = context.active_object
        if arm_obj is None or arm_obj.type != "ARMATURE":
            self.report({"ERROR"}, "Select the imported SWG armature first")
            return {"CANCELLED"}
        json_path = context.scene.get("swg_last_json_dump")
        if not json_path:
            self.report({"ERROR"}, "No JSON dump on record - run SWG: Import Skeleton first")
            return {"CANCELLED"}
        from .skeleton_import import load_skeleton_json

        data = load_skeleton_json(json_path)
        _, animated_count = apply_animation(arm_obj, data)
        self.report({"INFO"}, f"Keyframed {animated_count} real animated bones")
        return {"FINISHED"}
