"""Bridges this toolkit's own skeleton import to the existing community
io_scene_swg_mgn addon's mesh import - deliberately a real dependency, not
a reimplementation. That addon already correctly imports .mgn vertex
positions/UVs/bone-weight vertex groups (confirmed working live
2026-07-23/24, including two small Blender 4.1+ API-removal patches - see
NOTES.md). All this module adds is binding the imported mesh to THIS
toolkit's own armature (built from real decoded skeleton/animation data,
not the mesh file's own bare "skeleton name" string property) via a real
Armature modifier.
"""

import bpy


def import_mesh_and_bind(mesh_path, armature_obj):
    """Imports a real .mgn file via io_scene_swg_mgn and adds an Armature
    modifier binding it to `armature_obj`, using the mesh's own vertex
    groups (already bone-named by the community importer) for deform.
    Returns the new mesh Object."""
    existing = set(bpy.data.objects.keys())
    bpy.ops.import_scene.mgn(filepath=mesh_path)
    new_objects = [o for o in bpy.data.objects if o.name not in existing and o.type == "MESH"]
    if not new_objects:
        raise RuntimeError(f"io_scene_swg_mgn import produced no new mesh object for {mesh_path}")
    mesh_obj = new_objects[0]

    mesh_obj.parent = armature_obj
    mod = mesh_obj.modifiers.new(name="SWG_Armature", type="ARMATURE")
    mod.object = armature_obj
    mod.use_vertex_groups = True
    return mesh_obj


class SWG_OT_import_mesh_bound(bpy.types.Operator):
    """Import a real .mgn mesh and bind it to the currently-selected SWG
    armature (built by SWG: Import Skeleton) via an Armature modifier."""

    bl_idname = "swg.import_mesh_bound"
    bl_label = "SWG: Import Mesh (bind to selected armature)"
    bl_options = {"REGISTER", "UNDO"}

    filepath: bpy.props.StringProperty(subtype="FILE_PATH")

    def execute(self, context):
        arm_obj = context.active_object
        if arm_obj is None or arm_obj.type != "ARMATURE":
            self.report({"ERROR"}, "Select the imported SWG armature first")
            return {"CANCELLED"}
        if not self.filepath:
            self.report({"ERROR"}, "No .mgn path set")
            return {"CANCELLED"}
        try:
            mesh_obj = import_mesh_and_bind(self.filepath, arm_obj)
        except RuntimeError as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        self.report({"INFO"}, f"Imported and bound '{mesh_obj.name}' to '{arm_obj.name}'")
        return {"FINISHED"}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}
