"""SWG Blender Toolkit - see ../NOTES.md for the full design/build log.

Bare-minimum version (2026-07-24): imports a real skeleton + animation clip
from this project's own JSON dump (produced by dummyclient
--dump-anim-json-out) as an independent reference implementation, to test
whether this project's own C++ skeletal-animation composition math matches
an unrelated, independently-implemented renderer (Blender's own armature
system) given the same already-decoded data.

Explicitly architected to grow into a real, permanent, menu-driven tool for
this project - not a throwaway diagnostic. Every capability here is a
registered Blender Operator (so Blender's own F3 operator search already
works) plus a minimal sidebar panel with one button per step. Planned,
not-yet-built growth (see NOTES.md): native .tre archive reading in Python
(removing the dependency on this project's own C++ build), texture import,
a fuller wizard-style UI.
"""

bl_info = {
    "name": "SWG Blender Toolkit",
    "author": "SWG_Client_New project",
    "version": (0, 1, 0),
    "blender": (4, 2, 0),
    "location": "View3D > Sidebar > SWG Tab",
    "description": "Import real SWG skeletons/animations/meshes for comparison against this project's own C++ skinning code",
    "category": "Import-Export",
}

import bpy

from . import animation_import, compare, mesh_bridge, skeleton_import


class SWG_PT_toolkit_panel(bpy.types.Panel):
    bl_label = "SWG Toolkit"
    bl_idname = "SWG_PT_toolkit_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "SWG"

    def draw(self, context):
        layout = self.layout
        layout.label(text="1. Skeleton + Animation")
        layout.operator(skeleton_import.SWG_OT_import_skeleton.bl_idname, icon="ARMATURE_DATA")
        layout.operator(animation_import.SWG_OT_import_animation.bl_idname, icon="ACTION")
        layout.separator()
        layout.label(text="2. Mesh")
        layout.operator(mesh_bridge.SWG_OT_import_mesh_bound.bl_idname, icon="MESH_DATA")
        layout.separator()
        layout.label(text="3. Compare")
        layout.operator(compare.SWG_OT_compare_at_frame.bl_idname, icon="VIEWZOOM")


_classes = (
    skeleton_import.SWG_OT_import_skeleton,
    animation_import.SWG_OT_import_animation,
    mesh_bridge.SWG_OT_import_mesh_bound,
    compare.SWG_OT_compare_at_frame,
    SWG_PT_toolkit_panel,
)


def register():
    for cls in _classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(_classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
