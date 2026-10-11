# Math

Small hand-written vector and matrix types. Part of the shared `core` library.

Files: `shared/core/math/`

## Vectors

`Vec2`, `Vec3`, `Vec4` ([vec3.hpp](../core/math/vec3.hpp) etc.) are plain structs of `f32` components.

| Feature | Vec2 | Vec3 | Vec4 |
|---|---|---|---|
| Constructors `()`, `(x, y, …)`, `(n)` (fill) | ✓ | ✓ | ✓ |
| `+ - * / -v`, `+= -=` | ✓ | ✓ | ✓ |
| `*= /=` | | ✓ | |
| `length()`, `normalized()` | | ✓ | |
| `dot(a, b)` (static) | | ✓ | ✓ |
| `cross(a, b)` (static) | | ✓ | |

`*` and `/` take a scalar; there's no component-wise vector multiply.

## Color

[color.hpp](../core/math/color.hpp): `srgbToLinear` and `linearToSrgb`, for one value or a `Vec3`, using the exact sRGB curve (a straight segment near black, then a 2.4 power). Colors are picked and stored as sRGB; lighting converts them to linear first (see [renderer.md](../../valuma/docs/renderer.md#object-drawing)).

## Mat4

[mat4.hpp](../core/math/mat4.hpp)

Column-major `f32 m[16]`, laid out the way OpenGL expects, so `m` uploads directly with `glUniformMatrix4fv(..., GL_FALSE, m)`. `matrix[c]` returns column `c`.

| Function | Description |
|---|---|
| `Mat4()` / `identity()` | Identity |
| `operator*(Mat4)` / `operator*(Vec4)` | Matrix product / transform a point or vector |
| `translation(v)`, `scale(v)` | Affine builders |
| `rotationX/Y/Z(radians)` | Rotation about one axis |
| `perspective(fov, aspect, near, far)` | OpenGL-style projection (clip z in −1…1) |
| `lookAt(eye, target, up)` | View matrix |
| `inverse(m)` | General 4×4 inverse. Used for mouse rays, the grid, and the normal matrix. |
| `transpose(m)` | Swaps rows and columns. `transpose(inverse(model))` is the normal matrix for lighting. |

Transforms compose right to left: `projection * view * model * point`.

## Projection

[projection.hpp](../core/math/projection.hpp): `projectToScreen(viewProjection, point, width, height, out)` turns a world point into pixel coordinates (origin top-left, y down, matching the UI draw list). Returns false for points behind the camera. Used to place light markers and the scale/rotate pivot.

[screen_drag.hpp](../core/math/screen_drag.hpp): `screenAngle(pivot, point)` is the mouse's angle around a screen point, counterclockwise as seen on screen; `wrapAngle` wraps a difference into [−π, π]. Rotate sums wrapped per-frame differences so full turns add up.

## Utilities

[math_utils.hpp](../core/math/math_utils.hpp) provides `Math::EPSILON` (1e-6), `PI`, `TWO_PI`, and `HALF_PI`.
