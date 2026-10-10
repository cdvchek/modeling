# Scene

What's being edited and how you look at it: objects, materials, lights, reference images, transforms, the camera, the selection, picking, and undo history.

Files: `valuma/src/scene/` (meshes are covered separately in [mesh.md](mesh.md))

## Scene

[scene.hpp](../../valuma/src/scene/scene.hpp)

```cpp
struct Scene {
    Camera camera;
    ObjectCollection objects;
    LightCollection lights;
    ReferenceCollection references;
    MaterialCollection materials;
    TextureCollection textures;
    Selection selection;

    Object* activeObject();   // the object being edited, or null when there are none
};
```

## Objects

[object_collection.hpp](../../valuma/src/scene/objects/object_collection.hpp)

```cpp
struct Object {
    std::string name;
    Transform transform;    // relative to the parent (the world, for a top-level object)
    MeshData meshData;      // editable half-edge mesh
    ObjectHandle parent;    // INVALID_OBJECT when it has none
    bool meshDirty = true;  // set after any change to meshData
};
```

`Object` is scene data only. Its GPU copy lives in the application's `ObjectMeshCache` ([object_meshes.hpp](../../valuma/src/application/viewport/object_meshes.hpp)), keyed by handle, so objects can be copied freely (undo snapshots copy the whole collection). Set `meshDirty = true` after changing `meshData`; `renderFrame` brings the GPU copy up to date (patching only what moved when the layout is unchanged, see [renderer.md](renderer.md#gpu-meshes)) and clears the flag.

`ObjectCollection` stores objects in a `DynamicArray<Object>` and hands out `ObjectHandle` (`Handle<Object>`), the same pattern as lights. `INVALID_OBJECT` is the null handle. A removed object's handle stays invalid even if its slot is reused.

| Method | Description |
|---|---|
| `add(name, PresetMesh)` | Adds an object with a preset mesh and returns its handle. |
| `add(Object)` | Adds a whole object as it is (opening a project); sets `meshDirty`. |
| `remove(handle)` | Removes the object. |
| `get(handle)` / `tryGet(handle)` | Reference (asserts) / pointer or `nullptr`. |
| `isValid(handle)` | Whether the handle points to a live object. |
| `handles()` | Every live object, in slot order. |
| `handleAt(slot)` | The live object in that slot, or an invalid handle. The `object` command uses slot indices as ids. |
| `count()` | Number of objects. |
| `uniqueName(base)` | `base`, or `base N` with the lowest N that isn't taken. |
| `markAllDirty()` | Sets `meshDirty` on every object and marks all their vertices moved (after an undo restore: a restored copy can hold other positions under the same layout). |
| `parentOf(h)`, `childrenOf(h)`, `isAncestor(a, h)`, `topLevelOf(h)` | The family: the parent (or `INVALID_OBJECT`), the direct children in slot order, whether `a` is above `h`, and the object at the top of `h`'s family. |
| `hierarchy()` | Every object as a `HierarchyEntry` (handle, depth): parents before their children, siblings in slot order. The Objects tab's tree. |
| `worldTransform(h)` / `worldMatrix(h)` | Where the object is in the world: its transform combined with every parent's (`combineTransforms`). A top-level object's is its own, returned as is. Drawing, picking, and every tool use these. |
| `parentWorldTransform(h)` | The parent's world transform, or the identity. |
| `setWorldTransform(h, world)` / `setWorldPosition(h, position)` | Put the object at a world transform (its relative one is worked out), or move only its origin, leaving its relative rotation and scale exactly as they are (grab and scale use this, so a long drag doesn't wear on the angles). |
| `setParent(child, parent)` | A new parent (`INVALID_OBJECT`: none), keeping the child where it is in the world. False, changing nothing, for an invalid handle or a parent that is the child or one of its children. |

`remove(h)` first moves the object's children up a level (to its parent, or the top), keeping them where they are.

### Active object

Only one object is edited at a time: `selection.getActiveObject()` (`scene.activeObject()` for the object itself). Selected vertices, edges, and faces always belong to it, and tools act on it. In edit modes only the active object draws its edges, vertices, and selection; the others draw just their lit faces. In object mode selected objects draw every edge in the selection color (a purple outline), and the others draw just their faces. A new object added in object mode becomes the selection.

Clicking in the viewport (any selection mode): the click is tested against the active object's elements for the current mode and its faces. If another object's face is closer than anything hit on the active object, that object becomes active instead, in the same mode with nothing selected. Loop and ring selection only look at the active object. The `object <id> edit` command also switches it.

## Lights

[light.hpp](../../valuma/src/scene/lights/light.hpp), [light_collection.hpp](../../valuma/src/scene/lights/light_collection.hpp)

Besides the individual lights, the collection holds one `AmbientLight` (color and strength only). All enabled lights shade mesh faces: each frame `renderFrame` copies them into a `LightingState` for the renderer (see [renderer.md](renderer.md#object-drawing)). Every light is drawn as a marker in the viewport (see [application.md](application.md)); `light list` prints them (see [console.md](console.md#light-command)).

The default scene (`loadTestScene`) adds one directional light, `Sun`, traveling along `(0.4, −1, −0.6)` (from above, front, left) at intensity 1.2.

```cpp
enum class LightType { Point, Directional, Spot };

struct Light {
    std::string name;
    LightType type = LightType::Point;
    Vec3 position;              // point and spot
    Vec3 direction;             // directional and spot
    Vec3 color;                 // linear RGB, default white
    f32 intensity = 1.0f;
    f32 range = 10.0f;          // point and spot
    f32 innerConeRadians;       // spot, half-angle
    f32 outerConeRadians;       // spot, half-angle
    bool enabled = true;
};

struct AmbientLight {
    Vec3 color;                 // default white
    f32 strength = 0.3f;
};
```

`LightCollection` stores lights in a `DynamicArray<Light>` and hands out `LightHandle` (`Handle<Light>`). A removed light's handle stays invalid even if its slot is reused. `INVALID_LIGHT` is the null handle.

| Method | Description |
|---|---|
| `add(light)` | Stores a copy and returns its handle. |
| `remove(handle)` | Removes the light. Does nothing for an invalid handle. |
| `replace(handle, light)` | Overwrites the light in that slot, keeping the handle. Returns false if the handle is invalid. |
| `get(handle)` | Reference to the light; edit fields through it. Asserts the handle is valid. |
| `tryGet(handle)` | Pointer, or `nullptr` if the handle is invalid. |
| `isValid(handle)` | Whether the handle points to a live light. |
| `handles()` | Handles of every live light, in slot order. |
| `handleAt(slot)` | Handle of the live light in that slot, or an invalid handle. The `light` command uses slot indices as light ids. |
| `count()` | Number of live lights. |
| `nextName()` | "Light N", with N one past the highest "Light N" in use (other names are ignored), so names don't repeat after deleting. |
| `getAmbient()` | The scene's single ambient light. |
| `setAmbientColor(color)` / `setAmbientStrength(s)` | Set the ambient light; values are clamped to 0..1. |

Lights are part of the undo snapshot (see [History](#history)), so wrapping a light edit in `history.begin`/`commit` makes it undoable.

## Materials

[material.hpp](../../valuma/src/scene/materials/material.hpp), [material_collection.hpp](../../valuma/src/scene/materials/material_collection.hpp)

How a surface looks, shared by the objects that use it (see [features.md](../features.md)). The values are the metallic-roughness set glTF and the game engines use, so a material exports as it is.

```cpp
struct Material {
    std::string name;
    Vec3 baseColor;             // sRGB, as picked
    TextureHandle baseColorMap; // multiplied by baseColor (its alpha by opacity) through the UVs; not valid = none
    f32 roughness = 0.5;        // 0 mirror-sharp highlights, 1 none
    f32 metallic = 0;           // 0 plastic, stone, wood; 1 metal
    Vec3 emissiveColor;         // sRGB
    f32 emissiveStrength = 0;   // 0 gives no glow
    f32 opacity = 1;
    AlphaMode alphaMode;        // Opaque, Cutout (drawn where opacity reaches alphaCutoff), Blend (see-through)
    f32 alphaCutoff = 0.5;
    bool doubleSided = false;   // otherwise back faces are culled, as the engine will

    bool sameLook(const Material& other) const;   // every value equal, the name aside
};
```

`MaterialCollection` always holds a **Default** material (the old face gray, roughness 0.5, not metallic), created first, so it's first in `handles()`. It can be edited but `remove` refuses it.

Each `Object` has a `MaterialHandle material`. A handle that isn't valid (none set, or its material was removed) means Default: `resolve(handle)` gives the handle itself when valid, otherwise `defaultMaterial()`. So new objects use Default without setting anything, removing a material puts its objects back on Default without touching them (handles are generational, so a reused slot never matches an old handle), and undoing the removal reconnects them.

| Method | Description |
|---|---|
| `add(material)`, `remove(handle)` | `remove` returns false for Default. |
| `isValid`, `isDefault`, `defaultMaterial()`, `resolve(handle)` | See above. |
| `get`, `tryGet`, `handles()`, `handleAt(slot)`, `count()` | As for the other collections. |
| `uniqueName(base)` | `base`, or `base N` with the lowest free N. |
| `findIdentical(material)` | A material with the same name and every value the same (import reuses it). |

`alphaModeName(mode)` gives `"opaque"`, `"cutout"`, or `"blend"`. `sameLook` compares the map handle too.

## Textures

[texture.hpp](../../valuma/src/scene/textures/texture.hpp), [texture_collection.hpp](../../valuma/src/scene/textures/texture_collection.hpp), [picture.hpp](../../valuma/src/scene/pictures/picture.hpp)

Pictures materials use as maps. A texture is its own thing in the scene, not part of a material, so several materials can share one (an atlas for several objects).

```cpp
struct Picture {                   // a PNG file as it was added; never changes (reference images use it too)
    std::string fileName;
    std::vector<u8> png;
    u32 width, height;             // pixels
};

struct Texture {
    std::string name;
    std::shared_ptr<const Picture> picture;   // the file as loaded, shared by copies (undo); null once it has layers
    std::string sourcePath;        // the file it was loaded from (UTF-8), for Reload; empty for one from an asset
    LayerStack layers;             // its paintable pixels (see Layers); empty until it's painted or given layers

    bool layered() const;          // has layers
    u32 width() const, height() const;   // from the layers, else the picture
};
```

A texture's pixels are its picture or its layers, never both: `makeLayered` turns the first into the second.

A material's `baseColorMap` that isn't valid (none set, or its texture removed) means no map, so removing a texture puts its materials back on their plain colors without touching them, and undoing the removal reconnects them. Reloading swaps in a new `Picture` (or, on a layered texture, new tiles for the layer that holds the file); older undo steps keep the old one.

| Method | Description |
|---|---|
| `add(texture)`, `remove(handle)` | |
| `isValid`, `get`, `tryGet`, `handles()`, `handleAt(slot)`, `count()` | As for the other collections. |
| `uniqueName(base)` | `base`, or `base N` with the lowest free N. |
| `findSamePicture(picture)` | A texture whose PNG is byte for byte the same (import reuses it). Layered textures have no picture, so they never match and importing one's export adds a new texture. |

[paint_targets.hpp](../../valuma/src/scene/textures/paint_targets.hpp): what an object can be painted into.

| Function | Description |
|---|---|
| `paintTargets(scene, object)` | One `PaintTarget` per texture the object's faces draw with as a base map (`texture`, and the `materials` using it), in the order their first faces come, then one per material without a valid map (`texture` invalid). Only materials some face uses count. |
| `faceDrawMaterial(scene, object, face)` | The face's own material, else the object's, resolved (Default when neither is valid). |
| `facePaints(scene, object, face, texture)` | The face's material's base map is that (valid) texture. |
| `paintIsland(scene, object, face, texture)` | The faces of the face's UV island that paint into the texture, in handle order: where a dab on that face may spread. Empty when the face itself doesn't paint into it. |
| `texturePoint(uv, width, height)` | A UV as a point on a texture, in its pixels from the top left (what a brush stroke takes). UVs outside 0 to 1 wrap around, as the texture repeats on the model; exactly 1 stays the far edge. |
| `textureAxes(corners, uvs, width, height, perX, perY)` | How a triangle's surface runs under its texture: the step in the world for one texture pixel across and one down, from its world corners and their UVs. A point `x`, `y` pixels from a spot on the texture is that spot's place in the world plus `perX × x + perY × y`, which is how the brush's outline is laid on the model. False when the triangle has no area on the texture. |
| `solidLayers(width, height, color)` | Layers for a new texture: one **Base** layer of an opaque sRGB color (a single shared tile). |
| `makeLayered(texture, error)` | Gives a texture layers if it has none: its picture becomes the Base layer, marked `fromFile` and keeping the file's bytes as its PNG, and the picture is let go. False with `error` when the picture can't be read. |

### Layers

[layers.hpp](../../valuma/src/scene/textures/layers.hpp): a paintable texture's pixels. A texture with layers draws from them (see `LayerTextureCache` in [application.md](application.md)), is saved layer by layer (see `TEXR` in [project.md](project.md)), and is exported as its combined picture.

```cpp
struct Tile { std::array<u8, 64 * 64 * 4> pixels; };   // TILE_SIZE = 64; RGBA, rows from the top

struct Layer {
    std::string name;
    bool visible = true;
    f32 opacity = 1.0f;
    bool fromFile = false;                      // holds the texture's file: Reload from file replaces this layer
    std::vector<std::shared_ptr<Tile>> tiles;   // row by row; null = fully transparent
    mutable std::shared_ptr<const LayerPng> png;   // its PNG, with the tiles it was made from (see layerPng)
};

struct LayerStack {
    u32 width, height;             // pixels
    std::vector<Layer> layers;     // bottom first; none = the texture isn't layered
    u32 active;                    // the layer paint goes to
};
```

- **Pixels** are 8-bit RGBA (`Pixel`), sRGB, with alpha not multiplied into the color, as PNG stores them.
- **Tiles** cover the picture in a grid (`tilesAcross()` × `tilesDown()`); a picture that isn't a whole number of tiles has edge tiles whose extra pixels are never read. A clear tile is null, so an empty layer takes no pixel memory, and whole tiles of one color share a single tile.
- **Copies share tiles.** Copying a `LayerStack` (every undo step copies the `TextureCollection`) copies only the tile pointers: 256 per layer at 1024 × 1024. Pixels are changed only through `editTile`, which copies a tile first when anything else still holds it, so an undo step keeps just the tiles that changed after it.
- **Blending** is ordinary "over", bottom to top, in sRGB values as image editors do by default: each visible layer's alpha is scaled by its opacity (rounded to 255ths), in integers. Under nothing, paint keeps its own color and alpha, so a texture can have clear parts.

| Function | Description |
|---|---|
| `layersFromImage(image)` | A stack with one layer, **Base**, holding the image. |
| `layersFromPng(png, out, error)`, `layersFromPicture(picture, out, error)` | The same from a PNG file, which Base keeps as its PNG; false with `error` if it can't be decoded, leaving `out` alone. |
| `addLayer(stack, name)` | A clear layer above the active one; it becomes active. Returns its index. Without a name it's `nextLayerName`. |
| `removeLayer(stack, index)` | False for the only layer (a layered texture keeps at least one). Removing the active layer makes the one below active. |
| `moveLayer(stack, from, to)` | Puts a layer at another place in the order; the active index follows its layer. |
| `nextLayerName(stack)` | `Layer N`, one past the highest number in use. |
| `layerPixel(stack, layer, x, y)` | One layer's pixel; clear outside the picture. |
| `editTile(stack, layer, tileX, tileY)` | The tile to write into: made if it was clear, copied if shared. |
| `fillLayer(stack, layer, rect, color)` | Sets every pixel of a `PixelRect` (clipped to the picture). Whole tiles filled with a color share one tile; whole tiles filled with clear are dropped. |
| `compositeLayers(stack, rect, out)` | The combined picture for a rectangle inside the picture, as `rect.width × rect.height` RGBA pixels: what gets sent to the GPU for the changed part. |
| `flattenLayers(stack)` | The whole combined picture as an `image::Image`, for export. |
| `layerImage(stack, layer)` | One layer's own pixels, not blended with anything. |
| `layerPng(stack, layer)` | The layer as a PNG file (`LayerPng`: the bytes and the tiles they were made from). The layer keeps it, and it's reused for as long as the layer holds those same tiles, so saving encodes only layers that were painted since; name, visibility, and opacity don't touch it, and copies of the layer share it. A layer read from a file keeps the file's bytes. Safe to call for different layers from different threads. |
| `setLayerPng(stack, layer, png, error)` | Replaces a layer's pixels with a PNG, which it keeps as its PNG. False with `error`, and no change, if it can't be decoded or isn't the stack's size. Opening a project and Reload from file use it. |
| `flattenedPng(stack)` | The combined picture as a PNG file, for export: when one layer shows and it's at full opacity, that layer's own PNG (a texture with only its loaded file exports the file unchanged); otherwise `flattenLayers` encoded. Where nothing shows, a flattened picture is clear black: color under fully clear pixels isn't kept. |
| `blendPixel(dst, src, opacity)` | One RGBA pixel over another, the source's alpha scaled by an opacity of 0 to 255: the blend compositing and the brush both use. |
| `changedRects(before, after)` | Where the combined picture may differ between two states of a stack, as rectangles (runs of tiles along a row, clipped to the picture). A tile counts as changed when the tiles showing there, in order, with their layers' opacities, aren't the same ones: pointers are compared, never pixels, so it costs nothing next to compositing. An empty layer, a rename, or a hidden layer's paint changes nothing; another size gives the whole picture. |

Name, visibility, and opacity are plain fields. Rename, show or hide, and opacity changes are undo steps like any other scene change.

### Brush

[brush.hpp](../../valuma/src/scene/textures/brush.hpp): painting into a layer.

```cpp
struct Brush {
    Vec3 color;          // sRGB
    f32 size = 24;       // across, in texture pixels (1 to 512)
    f32 softness = 0.5;  // 0 a crisp edge, 1 fading from the middle out
    f32 opacity = 1;     // the most one stroke lays down
    f32 spacing = 0.1;   // between dabs, as a part of the size (0.02 to 1)
    bool erase = false;  // the eraser
    void clamp();        // every value into its range
};
```

`Stroke` is one press, drag, and release on one layer:

| Method | Description |
|---|---|
| `begin(stack, layer, brush)` | Remembers the layer's tiles as they are (pointers, so nothing is copied) and the brush, clamped. |
| `moveTo(stack, point)` | Carries the stroke to a point in texture pixels (fractions allowed, off the picture allowed). The first point gets a dab; after that one every `size × spacing` pixels along the way (a quarter pixel at least), with the distance left over carried into the next move, so a fast drag leaves an unbroken line. True if any pixel changed. |
| `lift()` | Lifts the brush without ending the stroke: the next `moveTo` starts with a dab of its own instead of a line from the last point. It's still one stroke, so the opacity cap carries across. Painting on the model lifts when the mouse leaves the model or crosses a seam. |
| `setMask(mask)` | Limits the dabs that follow to a `PaintMask` (shared, so the caller can keep and reuse it): each pixel takes that much of a dab, so a pixel at 0 is never touched. It can change between moves (painting on the model changes it when the mouse crosses onto another island), a null mask lifts the limit, and a mask that isn't the texture's size lets nothing through. `end` drops it. |
| `end()`, `active()`, `painted()` | `painted` says whether any pixel changed since `begin`: a stroke that changed nothing isn't worth an undo step. |

- **A dab** is round: full strength inside an inner radius, falling off smoothly (smoothstep) to nothing at the outer one, measured at pixel centers. Softness 0 leaves a one-pixel blur around the radius; softness 1 fades all the way from the middle.
- **Opacity is a cap for the stroke.** The stroke keeps, per touched tile, how much brush each pixel has had so far (the most any dab gave it, 0 to 255). A pixel is repainted only when a dab reaches it further than before, and always from the layer as it was when the stroke began: that pixel, with the color blended over it at reach × opacity (`blendPixel`), or for the eraser with its alpha scaled down by it and its color kept. So crossing a spot again within a stroke adds nothing; a second stroke builds on the first.
- **Tiles** are written through `editTile`, so the tiles the stroke began with stay intact for the undo step that holds them, and only touched tiles are new. Erasing where the layer was clear makes no tiles.
- Everything is clipped to the picture.

[paint_mask.hpp](../../valuma/src/scene/textures/paint_mask.hpp): where on a texture paint may land.

```cpp
struct PaintMask {
    u32 width, height;        // the texture's size
    std::vector<u8> pixels;   // rows from the top; 0 takes none of a dab, 255 all of it
    u8 at(u32 x, u32 y) const;
};
```

`maskFromFaces(mesh, faces, width, height, reach)` marks (255) every pixel whose middle is inside one of the faces' UV triangles (their triangulation, with each corner's UV the face's own) or within `reach` pixels of one (`MASK_REACH`, 1, by default; 0 for the triangles alone). The reach is there so texture filtering along a seam never shows a thin unpainted line; islands packed closer than that pick up a fringe from each other. Triangles count either way round (mirrored UVs), a triangle with no area still marks what's within reach of its line, and UVs outside 0 to 1 mark where the repeating texture puts them. Painting on the model makes one from the paintable faces of the island under the mouse; active surfaces will make one from the selected faces.

## Reference images

[reference_image.hpp](../../valuma/src/scene/references/reference_image.hpp), [reference_collection.hpp](../../valuma/src/scene/references/reference_collection.hpp)

Pictures placed in the scene to model against (see [features.md](../features.md#reference-images)). They aren't objects: they have no mesh, aren't exported, and are selected on their own.

```cpp
struct ReferenceImage {
    std::string name;
    std::shared_ptr<const Picture> picture;   // see Textures; shared by copies, so undo doesn't copy the file
    Vec3 position, rotation;       // rotation in Euler angles, applied like an object's
    f32 size = 2;                  // height in world units; the width follows the picture
    f32 opacity = 1;
    ReferenceDepth depth = InScene;   // Behind, InScene, InFront
    bool locked, visible = true;

    f32 aspect() const;            // width over height of the picture (1 without one)
    Transform transform() const;   // scale = (size × aspect, size, 1)
    Mat4 matrix() const;
};
```

The plane is the unit square from −0.5 to 0.5 in X and Y, facing its own +Z, top at +Y; `matrix()` stretches it to the picture's proportions, so they can't be skewed. `MIN_REFERENCE_SIZE` (0.001) keeps the size away from zero. `referenceDepthName(depth)` gives `"behind"`, `"scene"`, or `"front"`.

`ReferenceCollection` is a `DynamicArray<ReferenceImage>` with the same methods as the light collection (`add`, `remove`, `isValid`, `get`, `tryGet`, `handles`, `handleAt(slot)`, `count`) plus `uniqueName(base)` (`base`, or `base N` with the lowest free N).

## Transform

[transform.hpp](../../valuma/src/scene/transform.hpp)

`position`, `rotation` (Euler angles in radians), `scale`. `getMatrix()` returns `T × Rz × Ry × Rx × S`. Edited from the panel's Objects tab, the `object` command, and object-mode grab/scale/rotate.

`ObjectSpace(transform)` holds the model matrix and its inverse: `pointToWorld`, `pointToLocal`, and `directionToLocal` (ignores position). Edit-mode grab, scale, and rotate use it to work in world space on the active object's mesh, so mouse movement and axis locks follow the screen and the world axes on any object transform.

`combineTransforms(parent, local)` gives a child's world transform without skew: `position = parent.position + R(parent) × (parent.scale ⊙ local.position)`, `rotation = R(parent) × R(local)` (back to Euler angles), `scale = parent.scale ⊙ local.scale`, so a parent's scale lands on the child's own axes. `relativeTransform(parent, world)` is its exact reverse. See [Parenting](../features.md#parenting).

`eulerFromAxes(x, y, z)` gives the Euler angles that turn the X, Y, and Z axes onto three given unit axes (right-handed, at right angles); a new reference image uses it to face the view (`x` = camera right, `y` = camera up, `z` = back toward the camera).

`rotateEuler(euler, axis, angle)` returns the Euler angles after a further world-space turn of `angle` around `axis` (builds both rotations as 3×3 matrices, multiplies, and reads the angles back; looking straight along Y, where X and Z turn about the same axis, it puts the whole turn in Z). Object-mode rotate uses it.

## Camera

[camera.hpp](../../valuma/src/scene/camera.hpp)

An orbit camera around `target`.

| Member | Description |
|---|---|
| `position`, `target`, `up` | View setup. Up is +Y. |
| `distance`, `yaw`, `pitch` | Orbit parameters. Zoom changes `distance`. |
| `fovRadians`, `nearPlane`, `farPlane` | 60°, 0.1, 1000. |
| `updatePositionFromOrbit()` | Recomputes `position` from target, distance, yaw, pitch. Call after changing any of them. |
| `getViewMatrix()` / `getProjectionMatrix(aspect)` | Look-at and perspective matrices. |
| `getForward()` / `getRight()` | Unit view direction and right vector. Used by grab, pan, and rotate. |

Orbit, pan, and zoom input is handled in `checkSelectionContext` (see [application.md](application.md#checkselectioncontext)). Panning moves `position` and `target` together.

## Selection

[selection.hpp](../../valuma/src/scene/selection/selection.hpp)

Separate lists of selected vertices, edges, faces, lights, objects, and reference images, plus the **active object**. Element entries store the object's handle and the element handle (`VertexSelection`, `EdgeSelection`, `FaceSelection`); they always belong to the active object.

| Method | Description |
|---|---|
| `clear()` | Empties the element, light, object, and reference image lists and the selected origin; the active object stays. |
| `addVertex/Edge/Face(object, handle)` | Adds if not already present. |
| `removeVertex/Edge/Face(object, handle)` | Removes if present. |
| `hasVertex/Edge/Face(object, handle)` | Membership test. |
| `hasVertices/Edges/Faces()` | Any selected. |
| `getVertices/Edges/Faces()` | Entries with object indices. |
| `getVertexHandles/EdgeHandles/FaceHandles()` | Element handles only (object dropped). |
| `getActiveObject()` / `setActiveObject(handle)` | The object being edited. Changing it clears the selected elements (and another object's selected origin); `clear()` leaves it alone. |
| `selectOrigin(object)`, `clearOrigin()`, `hasOrigin()`, `getOrigin()` | One object's origin, selected on its own. `selectOrigin` makes that object active and clears elements, lights, and objects; adding a vertex, edge, face, light, or object clears the origin. So origins never mix with anything else, enforced here rather than by the callers. |
| `getNumberOfVertices/Edges/Faces()` | Counts. |
| `addLight/removeLight/hasLight(handle)`, `hasLights()`, `getLights()` | Selected lights. Independent of the selection mode; `clear()` empties them too. |
| `selectObject/deselectObject/hasObject(handle)`, `hasObjects()`, `getObjects()`, `clearObjects()` | Whole objects selected in object mode. Changing the active object doesn't touch them. |
| `addReference/removeReference/hasReference(handle)`, `hasReferences()`, `getReferences()`, `clearReferences()` | Selected reference images. `addReference` clears every other kind (elements, lights, objects, the origin), and adding any other kind clears the images, so they never mix with anything, enforced here. |
| `setReferenceStartTransforms(transforms)` / `getReferenceStartTransforms()` | Reference image placements saved when grab, scale, or rotate starts, in the same order as `getReferences()` (`scale.y` holds the size). |
| `setObjectStartTransforms(transforms)` / `getObjectStartTransforms()` | Object transforms saved when grab, scale, or rotate starts, in the same order as `getObjects()`. Used to rebuild each frame and to cancel. |
| `clearMeshElements()` / `clearLights()` | Clear one kind only. Lights and mesh elements are never selected together: the click handlers in `check_selection.cpp` call `clearMeshElements` when a light is picked and `clearLights` when a vertex, edge, face, loop, or ring is picked (with or without Shift). An origin excludes both, enforced by `Selection` itself. |
| `setLightStartPositions(positions)` / `getLightStartPositions()` | Light positions saved when grab starts, in the same order as `getLights()`. Used to cancel or axis-snap. |
| `setLightStartDirections(directions)` / `getLightStartDirections()` | Light directions saved when rotate starts, same order. Used to cancel or reset on axis lock. |
| `setSelectionStartPositions(positions)` / `getSelectionStartPositions()` | Vertex positions saved when a modal tool starts, in the same order as `getVertices()`. Used to cancel or axis-snap. |

Conventions kept by the application code:
- Selecting an edge or face also selects its vertices. Tools that move geometry only look at the vertex list.
- An edge counts as selected if either of its half-edges is in the list.

### Selecting mesh elements

[mesh_selection.hpp](../../valuma/src/scene/selection/mesh_selection.hpp): keeps a selection whole, shared by the 3D view and the UV editor. `selectEdge` / `selectFace` bring the element's vertices along (an edge counts once, whichever half is stored; `isEdgeSelected` checks both), and `deselectEdge` / `deselectFace` drop only the vertices no other selected edge or face still uses. `selectAll(selection, object, mesh, mode)` takes every vertex, every edge (one half each), or every face of the object.

## Origins

[origin.hpp](../../valuma/src/scene/objects/origin.hpp)

An object's origin is its transform: the point its mesh is built around and the axes it turns on. These change it while the mesh stays where it is in the world, and so do its children. Origins are given as world transforms.

| Function | Description |
|---|---|
| `captureOrigin(objects, h)` | An `OriginStart`: the origin's world transform, the vertex positions, and each direct child's world transform. |
| `setOrigin(objects, h, start, to)` | Puts the origin at the world transform `to`, moves every vertex by `inverse(new world) × start world` so each keeps its world position, and puts each child back at its start world transform. Working from the start each time keeps a long drag from drifting. Marks the mesh dirty. |
| `setOrigin(objects, h, to)` | The same, from the object as it is now. |
| `vertexPositions(mesh)` | Every vertex position in `getVertexHandles` order. |
| `originAtCenter(objects, h)` | Target: the middle of the mesh's bounding box, in its own axes. |
| `originAtBottom(objects, h)` | Target: the middle of the bounding box's bottom (lowest along the object's own Y). |
| `originAtVertices(objects, h, vertices)` | Target: their average. |
| `originAtWorld(objects, h)` | Target: the world's 0, 0, 0, axes unchanged. |
| `originAlignedToWorld(objects, h)` | Target: rotation reset to 0 in the world, position unchanged. |
| `originApplied(objects, h)` | Target: the object's own transform at none (its parent's world transform, or the world's origin for a top-level object). Moving the origin there bakes position, rotation, and scale into the mesh (Apply transform). |

Targets keep the object's rotation and scale unless they say otherwise.

## Picking

[ray.hpp](../../valuma/src/scene/picking/ray.hpp), [scene_queries.hpp](../../valuma/src/scene/picking/scene_queries.hpp)

| Function | Description |
|---|---|
| `makeRayFromScreenPosition(mouseX, mouseY, width, height, camera)` | World-space ray through a pixel, built by unprojecting the near and far planes. |
| `pickVertex(scene, ray, radius, only)` | Nearest vertex within `radius` (world units) of the ray. |
| `pickEdge(scene, ray, radius, only)` | Nearest edge within `radius` of the ray. |
| `pickFace(scene, ray, only, exclude, culled)` | Nearest face whose triangulation the ray hits. The `FaceHit` also says where on the face: `point` (in the world), `triangle` (the one of the face's triangulation the ray went through) and `corners` (its corners in the world), `weights` (how much of each of its corners the point is, adding up to 1), `cornerUVs` (those corners' UVs, the face's own, so a vertex on a seam gives this face's side), and `uv` (the UV at the point, the corners' blended by the weights): what painting on the model needs to find the texture under the mouse. With `exclude`, finds another object under the mouse. `culled` (a `BackFacesCulled` function of the object) names objects whose back faces aren't drawn; their triangles seen from behind (counterclockwise is the front) are skipped. |
| `pickLight(scene, viewProjection, mouseX, mouseY, width, height, radius)` | Nearest light whose marker is within `radius` pixels of the mouse, measured on screen. Returns a `LightHit` with the handle and pixel distance. Checked before the mesh picks (after origins); the radius is `LIGHT_MARKER_PICK_RADIUS` (11 px) from `light_markers.hpp`. |
| `pickOrigin(scene, viewProjection, mouseX, mouseY, width, height, radius)` | The object whose origin projects nearest the mouse within `radius` pixels (`OriginHit`: object and pixel distance). Checked first of all, when origins show; the radius is `ORIGIN_MARKER_PICK_RADIUS` (9 px) from `origin_markers.hpp`. |

| `pickReference(scene, ray)` | The reference image the ray meets (either side), skipping hidden and locked ones (`ReferenceHit`: handle, its depth setting, and the distance along the ray). Images set to Front win over the rest and images set to Behind lose to the rest, whatever their distance; within a group the nearest wins. The click code then compares it with the mesh under the mouse (see [application.md](application.md#checkselectioncontext)). |

The mesh picks test every object (with its transform), or only `only` when it's valid, skipping `exclude`. They return a hit struct: `hit`, `object` (handle), the element handle, and `distance` along the ray. The selection code uses a radius of 0.03.

## History

[history.hpp](../../valuma/src/scene/history.hpp)

Undo/redo by snapshot. A `State` is a copy of the whole `ObjectCollection`, the whole `MaterialCollection`, the whole `TextureCollection` and `ReferenceCollection` (cheap: pictures and layer tiles are shared, not copied), the whole `LightCollection`, and the `Selection` (including the active object). Up to 100 undo steps are kept, and fewer when painting fills them: on each commit, paint tiles that steps older than the newest hold and the newest doesn't are counted (16 KB each), and the oldest steps are dropped while that's over the budget (256 MB; `setPaintBudget`), always keeping the newest.

| Method | Description |
|---|---|
| `begin(scene)` | Captures a pending snapshot. Does nothing if one is already pending, so nested `begin` calls are safe. |
| `commit()` | Pushes the pending snapshot onto the undo stack and clears redo. |
| `cancel(scene)` | Restores the pending snapshot and discards it. |
| `undo(scene)` / `redo(scene)` | Swaps the current state with the top of the undo / redo stack. Returns false if empty. |
| `stateId()` | Names the current state: each commit gives a new id, and undo or redo back to a state gives its id again. The app compares it with the id at the last save to show unsaved changes (see [project.md](project.md)). `cancel` keeps the id. |
| `clear()` | Drops every step (after opening a project or starting a new one); the state gets a new id. |

Usage pattern: call `begin` before changing anything, then exactly one of `commit` or `cancel`. Modal tools call `begin` when they start and `commit`/`cancel` when they end.

Restoring replaces objects, materials, textures, lights, and reference images wholesale and sets `meshDirty` on every object, so adding, removing, and editing objects and lights are all undoable.
