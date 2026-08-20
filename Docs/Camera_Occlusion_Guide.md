# Camera Occlusion Fade

`UCMCameraOcclusionComponent` is created automatically by `ACMChimera`. On the
locally viewed Chimera it sphere-traces from the active camera to the body and
updates materials on blocking `WorldStatic` components.

## Obstacle material contract

Obstacle materials that should fade must be `Masked` and expose these
parameters:

| Parameter | Type | Meaning |
| --- | --- | --- |
| `CM_OcclusionFade` | Scalar | `0` normally, interpolates toward `1` while occluding |
| `CM_OcclusionCenter` | Vector | Character center in viewport UV in the R/G channels |
| `CM_OcclusionRadius` | Scalar | Circular fade radius in normalized viewport units |
| `CM_OcclusionMinOpacity` | Scalar | Visible dither coverage at the center of the fade |
| `CM_OcclusionEdgeSoftness` | Scalar | Width of the soft circular transition |

Use this graph logic for the material's `Opacity Mask`:

1. Read viewport UV from `ScreenPosition` and calculate its distance from
   `CM_OcclusionCenter.RG`.
2. Use `SmoothStep(CM_OcclusionRadius, CM_OcclusionRadius +
   CM_OcclusionEdgeSoftness, Distance)`
   to get a soft circle that is `0` at the character and `1` outside.
3. Lerp from `CM_OcclusionMinOpacity` to `1.0` with that circle to get the
   local opacity.
4. Lerp from `1.0` to the local opacity using `CM_OcclusionFade`.
5. Pass the result through `DitherTemporalAA`, multiply it by any existing
   opacity mask, and connect it to `Opacity Mask`.

The dithered masked approach preserves opaque-material lighting and sorting
while visually producing a soft partial transparency around the character.
Materials without these parameters are left visually unchanged.

## Tuning

Select `CameraOcclusionComponent` on the Chimera Blueprint to tune trace
radius, target height, screen fade radius, minimum opacity, edge softness, and
fade-in/fade-out speed. Add an object channel to `OccluderObjectTypes` if a
blocking environment object is not `WorldStatic`.
