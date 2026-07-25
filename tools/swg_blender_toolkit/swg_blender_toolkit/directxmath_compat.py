"""Ports the exact DirectXMath quaternion multiply formula this project's
own C++ code uses (libs/animation/src/SkeletalPose.cpp,
XMQuaternionMultiply), rather than relying on mathutils.Quaternion's own
multiply operator.

Why this matters: DirectXMath's XMQuaternionMultiply(Q1, Q2) applies Q1
FIRST then Q2 - the OPPOSITE of the naive mathematical convention, and
different from how most other quaternion libraries (including Blender's
own mathutils) compose. Getting this backwards would silently produce a
different rotation than our C++ code computes, invalidating the entire
comparison this toolkit exists to make. Porting the documented formula
directly, rather than trusting an operator's assumed convention, removes
that risk.

Quaternions here are represented as plain (x, y, z, w) tuples, matching
this project's own JSON dump format and DirectXMath's own field order -
NOT Blender's own (w, x, y, z) Quaternion order. Convert at the boundary
(see skeleton_import.py/animation_import.py) when handing a result to
Blender's own APIs.
"""


def xm_quaternion_multiply(q1, q2):
    """Exact port of DirectXMath's XMQuaternionMultiply(Q1, Q2). Result
    represents "rotate by Q1 first, then by Q2" - matches
    libs/animation/src/SkeletalPose.cpp's own
    XMQuaternionMultiply(preRot, animRot) etc. calls exactly."""
    x1, y1, z1, w1 = q1
    x2, y2, z2, w2 = q2
    return (
        (w2 * x1) + (x2 * w1) + (y2 * z1) - (z2 * y1),
        (w2 * y1) - (x2 * z1) + (y2 * w1) + (z2 * x1),
        (w2 * z1) + (x2 * y1) - (y2 * x1) + (z2 * w1),
        (w2 * w1) - (x2 * x1) - (y2 * y1) - (z2 * z1),
    )


def compose_local_rotation(pre_rotation, anim_rotation, post_rotation):
    """Matches SkeletalPose.cpp's default composition variant exactly:
    rotation = XMQuaternionMultiply(XMQuaternionMultiply(preRot, animRot), postRot)."""
    return xm_quaternion_multiply(xm_quaternion_multiply(pre_rotation, anim_rotation), post_rotation)


def to_blender_quat(xm_quat):
    """(x, y, z, w) -> Blender's own (w, x, y, z) Quaternion field order."""
    x, y, z, w = xm_quat
    return (w, x, y, z)
