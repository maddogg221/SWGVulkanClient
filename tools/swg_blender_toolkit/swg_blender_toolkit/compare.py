"""Extracts Blender's own independently-computed world/armature-space bone
matrices at a specific frame, for a direct numeric diff against this
project's own C++ values printed at the same frame (see this project's own
[ANIMDBG] bone=... worldPos=... diagnostic in Visualizer.cpp).

Deliberately always operates on an EXACT real keyframe frame number (from
the same JSON dump, not an arbitrary time) - parking on a frame that both
this project's own C++ interpolation code AND Blender's own F-curve
evaluation treat as an exact stored keyframe (not interpolated) removes any
difference in interpolation scheme as a confound. Frame 0 is virtually
always an exact real keyframe for every channel in practice (standard
authoring convention) and is the default here for that reason.
"""

import bpy
import mathutils

_WATCH_BONES = ["root", "spine1", "spine2", "spine3", "lClav",
                "lArm", "lForeArm", "lUlna", "lWrist", "lRing01", "lRing02"]


def extract_world_bone_matrices(arm_obj, frame, bone_names=None):
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    names = bone_names if bone_names is not None else _WATCH_BONES
    results = {}
    for name in names:
        pb = arm_obj.pose.bones.get(name)
        if pb is None:
            continue
        world_matrix = arm_obj.matrix_world @ pb.matrix  # armature-space -> real world space
        translation = world_matrix.to_translation()
        rotation = world_matrix.to_quaternion()  # Blender (w, x, y, z) order
        results[name] = {
            "translation": (translation.x, translation.y, translation.z),
            "rotation_wxyz": (rotation.w, rotation.x, rotation.y, rotation.z),
        }
    return results


class SWG_OT_compare_at_frame(bpy.types.Operator):
    """Print the selected armature's real world-space bone matrices at a
    given frame (default: 0, an exact real keyframe) for the wrist/finger
    chain, to directly diff against this project's own C++-printed values
    for the same bones at the same frame."""

    bl_idname = "swg.compare_at_frame"
    bl_label = "SWG: Compare Bone World Transforms At Frame"
    bl_options = {"REGISTER"}

    frame: bpy.props.IntProperty(default=0, description="Real keyframe frame number to compare at")

    def execute(self, context):
        arm_obj = context.active_object
        if arm_obj is None or arm_obj.type != "ARMATURE":
            self.report({"ERROR"}, "Select the imported SWG armature first")
            return {"CANCELLED"}
        results = extract_world_bone_matrices(arm_obj, self.frame)
        print(f"=== SWG toolkit: Blender-computed world bone transforms at frame {self.frame} ===")
        for name, data in results.items():
            t = data["translation"]
            r = data["rotation_wxyz"]
            print(f"  bone={name} worldPos=({t[0]:.5f}, {t[1]:.5f}, {t[2]:.5f}) "
                  f"worldRot_wxyz=({r[0]:.5f}, {r[1]:.5f}, {r[2]:.5f}, {r[3]:.5f})")
        self.report({"INFO"}, f"Printed {len(results)} bone transforms to the system console")
        return {"FINISHED"}
