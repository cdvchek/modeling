#include "application/ui/main_panel.hpp"
#include "application/ui/ui_undo.hpp"
#include "application/commands/light_commands.hpp"
#include "application/commands/object_commands.hpp"
#include "application/viewport/reference_images.hpp"
#include "application/commands/material_commands.hpp"
#include "application/actions/origin_actions.hpp"
#include "application/commands/texture_commands.hpp"
#include "application/viewport/material_view.hpp"
#include "ui/ui_style.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
    constexpr f32 PANEL_WIDTH = 380.0f;
    constexpr f32 PANEL_HEIGHT = 620.0f;
    constexpr f32 PANEL_MARGIN = 20.0f;
    constexpr f32 RADIANS_TO_DEGREES = 180.0f / 3.14159265f;

    // The object and light lists show this many rows before they scroll
    constexpr u32 LIST_ROWS = 6;
    constexpr f32 TYPE_BUTTON_WIDTH = 120.0f;
    constexpr f32 PRESET_DROPDOWN_WIDTH = 130.0f;
    constexpr f32 MIN_SCALE = 0.001f;

    const std::vector<std::string_view> TABS = { "Objects", "Materials", "Lights", "References" };
    constexpr i32 OBJECTS_TAB = 0;
    constexpr i32 MATERIALS_TAB = 1;
    constexpr i32 LIGHTS_TAB = 2;
    constexpr i32 REFERENCES_TAB = 3;

    // The big swatch at the top of the selected material
    constexpr f32 PREVIEW_SIZE = 112.0f;
    constexpr f32 MAX_GLOW = 10.0f;

    const std::vector<AlphaMode> ALPHA_MODES = { AlphaMode::Opaque, AlphaMode::Cutout, AlphaMode::Blend };
    const std::vector<std::string_view> ALPHA_MODE_LABELS = { "Opaque", "Cutout", "Blend" };

    // Order of the depth switch
    const std::vector<ReferenceDepth> DEPTHS = { ReferenceDepth::Behind, ReferenceDepth::InScene, ReferenceDepth::InFront };
    const std::vector<std::string_view> DEPTH_LABELS = { "Behind", "Scene", "Front" };

    f32 listHeight() {
        return LIST_ROWS * UIStyle::ROW_HEIGHT + (LIST_ROWS - 1) * UIStyle::ITEM_SPACING + UIStyle::CHILD_PADDING * 2.0f;
    }

    // A heading row with three controls on the right: picker, +, and -
    struct ListHeader {
        Rect minus;
        Rect plus;
        Rect picker;
    };

    ListHeader listHeader(UIContext& ui, std::string_view title, f32 pickerWidth) {
        const Rect header = ui.row();

        // A narrow panel shrinks the picker first, so the title keeps some room
        const f32 picker = std::min(pickerWidth, std::floor(header.width * 0.45f));

        ListHeader rects;
        rects.minus = { header.right() - header.height, header.y, header.height, header.height };
        rects.plus = { rects.minus.x - UIStyle::COMPONENT_GAP - header.height, header.y, header.height, header.height };
        rects.picker = { rects.plus.x - UIStyle::COMPONENT_GAP - picker, header.y, picker, header.height };

        const f32 titleRoom = rects.picker.x - header.x - UIStyle::COMPONENT_GAP;
        ui.text(header, fitText(ui.font(), title, titleRoom), UIStyle::ACCENT_GREEN);
        return rects;
    }

    constexpr std::string_view IMPORT_LABEL = "Import...";

    std::vector<std::string_view> presetLabels() {
        std::vector<std::string_view> labels;
        for (const ObjectPreset& preset : objectPresets()) labels.push_back(preset.displayName);
        labels.push_back(IMPORT_LABEL);
        return labels;
    }

    // Heading with the preset dropdown, + and - on the right, then a fixed-height scrolling list
    void objectListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        ObjectCollection& objects = ctx.scene.objects;
        Selection& selection = ctx.scene.selection;
        i32& preset = ctx.viewport.newObjectPreset;

        const ListHeader header = listHeader(ui, "Objects", PRESET_DROPDOWN_WIDTH);
        ui.dropdown("preset", header.picker, preset, presetLabels());

        // The entry after the presets is Importâ€¦: + opens the file dialog (next frame, not while drawing)
        if (ui.button("+", header.plus)) {
            if (preset >= static_cast<i32>(objectPresets().size())) {
                ctx.importRequested = true;
            } else {
                const ObjectPreset& chosen = objectPresets()[preset];
                addObject(ctx, chosen.preset, objects.uniqueName(chosen.displayName));
            }
        }
        if (ui.button("-", header.minus, objects.isValid(selection.getActiveObject()))) {
            removeObject(ctx, selection.getActiveObject());
        }

        const bool objectMode = ctx.systems.input_ctx.getModeContext() == InputContext_SelectionObject;

        std::vector<ObjectHandle>& folded = ctx.viewport.foldedObjects;
        std::erase_if(folded, [&objects](ObjectHandle handle) { return !objects.isValid(handle); });

        // The dragged object, if a row is being dragged; it can go under any object that isn't itself or one of its own
        const ObjectHandle dragged = ui.isDragging() ? objects.handleAt(ui.dragPayload()) : INVALID_OBJECT;
        auto canDropOn = [&](ObjectHandle target) {
            return objects.isValid(dragged) && target != dragged && !objects.isAncestor(dragged, target) && objects.parentOf(dragged) != target;
        };

        const Rect box = ui.beginChild("list", listHeight());

        // Parents before children; a folded object's children (and theirs) are skipped
        u32 hideBelow = UINT32_MAX;
        for (const HierarchyEntry& entry : objects.hierarchy()) {
            if (entry.depth > hideBelow) continue;
            hideBelow = UINT32_MAX;

            const ObjectHandle handle = entry.handle;
            const Object& object = objects.get(handle);
            const std::size_t faces = object.meshData.getFaceHandles().size();
            const std::string detail = std::to_string(faces) + (faces == 1 ? " face" : " faces");
            const bool hasChildren = !objects.childrenOf(handle).empty();
            const bool wasFolded = std::find(folded.begin(), folded.end(), handle) != folded.end();
            bool open = !wasFolded;

            // Object mode lists the selected objects; edit modes the one being edited
            const bool selected = objectMode ? selection.hasObject(handle) : handle == selection.getActiveObject();

            ui.pushId(handle.index);
            if (ui.treeRow(object.name, selected, detail, entry.depth, hasChildren, open)) {
                selection.clearLights();
                selection.setActiveObject(handle);
                if (objectMode) {
                    selection.clearObjects();
                    selection.selectObject(handle);
                }
            }
            ui.dragSource(handle.index, object.name);

            // Dropping one row on another parents it there, keeping it where it is in the world
            const Rect row = ui.lastItemRect();
            if (canDropOn(handle) && ui.mouseIn(row)) ui.drawList().roundedRect(row, UIStyle::CORNER_RADIUS, { 0.0f, 0.0f, 0.0f, 0.0f }, UIStyle::ACCENT, 1.5f);
            u32 dropped = 0;
            if (canDropOn(handle) && ui.acceptDrop(row, dropped)) setObjectParent(ctx, objects.handleAt(dropped), handle);
            ui.popId();

            if (open == wasFolded) {
                if (open) std::erase(folded, handle);
                else folded.push_back(handle);
            }
            if (!open) hideBelow = entry.depth;
        }
        ui.endChild();

        // Dropping on the list's empty space takes the object out of its parent
        u32 dropped = 0;
        if (ui.acceptDrop(box, dropped)) {
            const ObjectHandle handle = objects.handleAt(dropped);
            if (!objects.parentOf(handle).isNull()) setObjectParent(ctx, handle, INVALID_OBJECT);
        }
    }

    // Edits a copy of the transform so an undo restore mid-frame never leaves a dangling reference
    void selectedObjectSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        ObjectCollection& objects = ctx.scene.objects;
        const ObjectHandle handle = ctx.scene.selection.getActiveObject();

        ui.heading("Selected object");

        if (!objects.isValid(handle)) {
            ui.label("No object selected", true);
            return;
        }

        const Object& object = objects.get(handle);
        const MeshData& mesh = object.meshData;
        Transform transform = object.transform;
        bool changed = false;

        ui.pushId(handle.index);

        std::string name = object.name;
        const bool renamed = ui.textField("Name", name);
        trackUndo(ctx);

        ui.label(std::to_string(mesh.getVertexHandles().size()) + " verts, " + std::to_string(mesh.getEdgeHandles().size() / 2) + " edges, "
                 + std::to_string(mesh.getFaceHandles().size()) + " faces", true);

        changed |= ui.dragFloat3("Position", transform.position, 0.01f);
        trackUndo(ctx);

        Vec3 degrees = transform.rotation * RADIANS_TO_DEGREES;
        if (ui.dragFloat3("Rotation", degrees, 0.5f, "%.0f")) {
            transform.rotation = degrees / RADIANS_TO_DEGREES;
            changed = true;
        }
        trackUndo(ctx);

        // A zero scale can't be inverted for lighting, so keep each axis a little away from it
        if (ui.dragFloat3("Scale", transform.scale, 0.01f)) {
            for (f32* axis : { &transform.scale.x, &transform.scale.y, &transform.scale.z }) {
                if (std::abs(*axis) < MIN_SCALE) *axis = *axis < 0.0f ? -MIN_SCALE : MIN_SCALE;
            }
            changed = true;
        }
        trackUndo(ctx);

        // Bakes position, rotation, and scale into the mesh; dimmed when there's nothing to apply
        const Transform& own = object.transform;
        const auto equals = [](const Vec3& v, f32 value) { return v.x == value && v.y == value && v.z == value; };
        const bool applied = equals(own.position, 0.0f) && equals(own.rotation, 0.0f) && equals(own.scale, 1.0f);
        if (ui.button("Apply transform", ui.row(), !applied)) {
            applyTransform(ctx, handle);
            changed = false;
        }

        // The object's material, with swatches; picking one is an undo step
        const MaterialCollection& materials = ctx.scene.materials;
        const std::vector<MaterialHandle> materialHandles = materials.handles();
        std::vector<std::string_view> materialNames;
        std::vector<u32> swatches;
        i32 current = 0;
        for (u32 i = 0; i < materialHandles.size(); ++i) {
            materialNames.push_back(materials.get(materialHandles[i]).name);
            swatches.push_back(ctx.materialPreviews.texture(materialHandles[i]));
            if (materialHandles[i] == materials.resolve(object.material)) current = static_cast<i32>(i);
        }

        const Rect materialRow = ui.row();
        const f32 labelWidth = std::floor(materialRow.width * 0.38f);
        ui.text({ materialRow.x, materialRow.y, labelWidth, materialRow.height }, "Material", UIStyle::TEXT_DIM);
        if (ui.dropdown("Material", { materialRow.x + labelWidth, materialRow.y, materialRow.width - labelWidth, materialRow.height }, current, materialNames, swatches)) {
            assignMaterial(ctx, { handle }, materialHandles[current]);
        }
        const u32 ownFaces = facesWithOwnMaterial(ctx, object);
        if (ownFaces > 0) ui.label(std::to_string(ownFaces) + (ownFaces == 1 ? " face has its own material" : " faces have their own materials"), true);

        // Shading: picking a mode is an undo step; Auto adds its angle
        static const std::vector<std::string_view> SHADING_LABELS = { "Flat", "Smooth", "Auto" };
        i32 shading = static_cast<i32>(mesh.getShading());
        const Rect shadingRow = ui.row();
        ui.text({ shadingRow.x, shadingRow.y, labelWidth, shadingRow.height }, "Shading", UIStyle::TEXT_DIM);
        if (ui.dropdown("Shading", { shadingRow.x + labelWidth, shadingRow.y, shadingRow.width - labelWidth, shadingRow.height }, shading, SHADING_LABELS)) {
            ctx.history.begin(ctx.scene);
            objects.get(handle).meshData.setShading(static_cast<ShadingMode>(shading));
            objects.get(handle).meshDirty = true;
            ctx.history.commit();
        }
        if (objects.get(handle).meshData.getShading() == ShadingMode::Auto) {
            f32 degrees = objects.get(handle).meshData.getSmoothAngle() * RADIANS_TO_DEGREES;
            if (ui.dragFloat("Angle", degrees, 0.5f, "%.0f")) {
                objects.get(handle).meshData.setSmoothAngle(std::clamp(degrees, 0.0f, 180.0f) / RADIANS_TO_DEGREES);
                objects.get(handle).meshDirty = true;
            }
            trackUndo(ctx);
        }

        ui.popId();

        if (changed && objects.isValid(handle)) objects.get(handle).transform = transform;
        if (renamed && objects.isValid(handle)) objects.get(handle).name = name;
    }

    // Order used by the type buttons and the + toggle
    const std::vector<LightType> LIGHT_TYPES = { LightType::Point, LightType::Spot, LightType::Directional };
    const std::vector<std::string_view> LIGHT_TYPE_LABELS = { "Point", "Spot", "Directional" };

    i32 typeIndex(LightType type) {
        return static_cast<i32>(std::find(LIGHT_TYPES.begin(), LIGHT_TYPES.end(), type) - LIGHT_TYPES.begin());
    }

    const char* typeName(LightType type) {
        switch (type) {
            case LightType::Point: return "point";
            case LightType::Directional: return "directional";
            case LightType::Spot: return "spot";
        }
        return "";
    }

    void addLight(AppContext& ctx, LightType type) {
        LightCollection& lights = ctx.scene.lights;

        // Spread new lights out a little so they don't all land on the same spot
        Light light;
        light.type = type;
        light.name = lights.nextName();
        light.position.x = (static_cast<f32>(lights.count() % 5) - 2.0f) * 0.8f;

        ctx.history.begin(ctx.scene);
        const LightHandle handle = lights.add(light);
        ctx.history.commit();

        ctx.scene.selection.clear();
        ctx.scene.selection.addLight(handle);
    }

    // Heading with the type toggle and + button on the right, then a fixed-height scrolling list
    void lightListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;
        Selection& selection = ctx.scene.selection;
        LightType& newType = ctx.viewport.newLightType;

        const ListHeader header = listHeader(ui, "Lights", TYPE_BUTTON_WIDTH);

        if (ui.button(LIGHT_TYPE_LABELS[typeIndex(newType)], header.picker)) {
            newType = LIGHT_TYPES[(typeIndex(newType) + 1) % LIGHT_TYPES.size()];
        }
        if (ui.button("+", header.plus)) addLight(ctx, newType);
        if (ui.button("-", header.minus, selection.hasLights())) deleteSelectedLights(ctx);

        ui.beginChild("list", listHeight());
        for (LightHandle handle : lights.handles()) {
            const Light& light = lights.get(handle);

            ui.pushId(handle.index);
            if (ui.selectable(light.name, selection.hasLight(handle), typeName(light.type))) {
                selection.clear();
                selection.addLight(handle);
            }
            ui.popId();
        }
        ui.endChild();
    }

    // Edits a copy so an undo restore mid-frame never leaves a dangling reference
    void selectedLightSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;
        const std::vector<LightHandle>& selected = ctx.scene.selection.getLights();

        ui.heading("Selected light");

        if (selected.empty() || !lights.isValid(selected.front())) {
            ui.label("No light selected", true);
            return;
        }

        const LightHandle handle = selected.front();
        Light light = lights.get(handle);
        bool changed = false;

        ui.pushId(handle.index);

        changed |= ui.textField("Name", light.name);
        trackUndo(ctx);

        i32 type = typeIndex(light.type);
        if (ui.segmented("", type, LIGHT_TYPE_LABELS)) {
            light.type = LIGHT_TYPES[type];
            changed = true;
        }
        trackUndo(ctx);

        changed |= ui.checkbox("Enabled", light.enabled);
        trackUndo(ctx);
        changed |= ui.colorEdit("Color", light.color);
        trackUndo(ctx);
        changed |= ui.sliderFloat("Intensity", light.intensity, 0.0f, 5.0f);
        trackUndo(ctx);

        if (light.type != LightType::Directional) {
            changed |= ui.dragFloat3("Position", light.position, 0.01f);
            trackUndo(ctx);
            changed |= ui.sliderFloat("Range", light.range, 0.1f, 30.0f, "%.1f");
            trackUndo(ctx);
        }

        if (light.type != LightType::Point) {
            if (ui.dragFloat3("Direction", light.direction, 0.01f)) {
                if (light.direction.length() > 1e-4f) light.direction = light.direction.normalized();
                changed = true;
            }
            trackUndo(ctx);
        }

        if (light.type == LightType::Spot) {
            f32 inner = light.innerConeRadians * RADIANS_TO_DEGREES;
            f32 outer = light.outerConeRadians * RADIANS_TO_DEGREES;

            if (ui.sliderFloat("Inner cone", inner, 0.0f, 89.0f, "%.0f")) {
                light.innerConeRadians = std::min(inner, outer) / RADIANS_TO_DEGREES;
                changed = true;
            }
            trackUndo(ctx);

            if (ui.sliderFloat("Outer cone", outer, 1.0f, 89.0f, "%.0f")) {
                light.outerConeRadians = outer / RADIANS_TO_DEGREES;
                light.innerConeRadians = std::min(light.innerConeRadians, light.outerConeRadians);
                changed = true;
            }
            trackUndo(ctx);
        }

        ui.popId();

        if (changed && lights.isValid(handle)) lights.replace(handle, light);
    }

    // Heading with + and - on the right, then a fixed-height list of swatches
    void materialListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        MaterialCollection& materials = ctx.scene.materials;
        MaterialHandle& selected = ctx.viewport.selectedMaterial;
        selected = materials.resolve(selected);

        const ListHeader header = listHeader(ui, "Materials", 0.0f);
        if (ui.button("+", header.plus)) {
            Material material;
            material.name = materials.uniqueName("Material");
            ctx.history.begin(ctx.scene);
            selected = materials.add(material);
            ctx.history.commit();
        }
        if (ui.button("-", header.minus, !materials.isDefault(selected))) {
            removeMaterial(ctx, selected);
            selected = materials.defaultMaterial();
        }

        ui.beginChild("list", listHeight());
        for (MaterialHandle handle : materials.handles()) {
            const std::string detail = describeUse(materialUse(ctx, handle));

            ui.pushId(handle.index);
            if (ui.selectable(materials.get(handle).name, handle == selected, detail, ctx.materialPreviews.texture(handle))) selected = handle;
            ui.popId();
        }
        ui.endChild();
    }

    // The material's base color map: None, a texture (with its thumbnail), or Load PNG... to load one onto it
    void baseColorMapRow(AppContext& ctx, MaterialHandle handle, Material& material) {
        UIContext& ui = ctx.ui;
        const TextureCollection& textures = ctx.scene.textures;
        const std::vector<TextureHandle> handles = textures.handles();

        std::vector<std::string_view> labels = { "None" };
        std::vector<u32> icons = { 0 };
        i32 current = 0;
        for (u32 i = 0; i < handles.size(); ++i) {
            labels.push_back(textures.get(handles[i]).name);
            icons.push_back(textureImage(ctx, handles[i]));
            if (handles[i] == material.baseColorMap) current = static_cast<i32>(i) + 1;
        }
        labels.push_back("Load PNG...");
        icons.push_back(0);
        const i32 load = static_cast<i32>(labels.size()) - 1;

        const Rect row = ui.row();
        const f32 labelWidth = std::floor(row.width * 0.38f);
        ui.text({ row.x, row.y, labelWidth, row.height }, "Base map", UIStyle::TEXT_DIM);
        if (ui.dropdown("Base map", { row.x + labelWidth, row.y, row.width - labelWidth, row.height }, current, labels, icons)) {
            // The file dialog opens next frame, not while drawing
            if (current == load) ctx.textureRequest = { true, handle };
            else {
                material.baseColorMap = current == 0 ? INVALID_TEXTURE : handles[current - 1];
                setBaseColorMap(ctx, handle, material.baseColorMap);
            }
        }
    }

    // Heading with + and - on the right, then a fixed-height list of thumbnails
    void textureListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        TextureCollection& textures = ctx.scene.textures;
        TextureHandle& selected = ctx.viewport.selectedTexture;
        if (!textures.isValid(selected)) selected = textures.count() > 0 ? textures.handles().front() : INVALID_TEXTURE;

        const ListHeader header = listHeader(ui, "Textures", 0.0f);
        // The file dialog opens next frame, not while drawing
        if (ui.button("+", header.plus)) ctx.textureRequest = { true, INVALID_MATERIAL };
        if (ui.button("-", header.minus, textures.isValid(selected))) {
            removeTexture(ctx, selected);
            selected = INVALID_TEXTURE;
        }

        ui.beginChild("list", listHeight());
        for (TextureHandle handle : textures.handles()) {
            const Texture& texture = textures.get(handle);
            const std::string detail = describeTextureUse(textureUse(ctx, handle));

            ui.pushId(handle.index);
            if (ui.selectable(texture.name, handle == selected, detail, textureImage(ctx, handle))) selected = handle;
            ui.popId();
        }
        ui.endChild();
    }

    void selectedTextureSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        TextureCollection& textures = ctx.scene.textures;
        const TextureHandle handle = ctx.viewport.selectedTexture;

        ui.heading("Selected texture");
        if (!textures.isValid(handle)) {
            ui.label("No textures yet: + loads a PNG", true);
            return;
        }

        const Texture& texture = textures.get(handle);
        ui.pushId(handle.index);

        // The picture, fitted into a square the size of the material swatch
        if (texture.width() > 0 && texture.height() > 0) {
            const f32 aspect = static_cast<f32>(texture.width()) / static_cast<f32>(texture.height());
            const f32 width = aspect >= 1.0f ? PREVIEW_SIZE : std::floor(PREVIEW_SIZE * aspect);
            const f32 height = aspect >= 1.0f ? std::floor(PREVIEW_SIZE / aspect) : PREVIEW_SIZE;
            const Rect previewRow = ui.row(PREVIEW_SIZE);
            ui.drawList().image({ previewRow.x + std::floor((previewRow.width - width) * 0.5f), previewRow.y + std::floor((PREVIEW_SIZE - height) * 0.5f), width, height },
                                textureImage(ctx, handle));
        }

        std::string name = texture.name;
        if (ui.textField("Name", name) && textures.isValid(handle)) textures.get(handle).name = name;
        trackUndo(ctx);

        const std::string size = std::to_string(texture.width()) + " x " + std::to_string(texture.height());
        if (texture.layered()) ui.label(size + ", " + std::to_string(texture.layers.layers.size()) + (texture.layers.layers.size() == 1 ? " layer" : " layers"), true);
        else if (texture.picture) ui.label(size + ", " + texture.picture->fileName, true);
        ui.label("Used by " + describeTextureUse(textureUse(ctx, handle)), true);

        // Reads the file again after it was painted in another program
        if (ui.button("Reload from file", ui.row(), !texture.sourcePath.empty())) reloadTexture(ctx, handle);

        // Puts it on the material shown above
        const MaterialHandle material = ctx.scene.materials.resolve(ctx.viewport.selectedMaterial);
        const std::string useLabel = "Use as " + ctx.scene.materials.get(material).name + "'s base map";
        const bool alreadyUsed = ctx.scene.materials.get(material).baseColorMap == handle;
        if (ui.button(useLabel, ui.row(), !alreadyUsed)) setBaseColorMap(ctx, material, handle);

        ui.popId();
    }

    // Edits a copy so an undo restore mid-frame never leaves a dangling reference
    void selectedMaterialSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        MaterialCollection& materials = ctx.scene.materials;
        const MaterialHandle handle = materials.resolve(ctx.viewport.selectedMaterial);

        ui.heading("Selected material");

        Material material = materials.get(handle);
        bool changed = false;

        ui.pushId(handle.index);

        // The swatch, large; see-through materials sit on a checkerboard
        const Rect previewRow = ui.row(PREVIEW_SIZE);
        ui.drawList().image({ previewRow.x + std::floor((previewRow.width - PREVIEW_SIZE) * 0.5f), previewRow.y, PREVIEW_SIZE, PREVIEW_SIZE },
                            ctx.materialPreviews.texture(handle));

        // Default keeps its name, so it's always clear what objects without a material use
        if (materials.isDefault(handle)) {
            ui.label("Default: used by objects without a material", true);
        } else {
            changed |= ui.textField("Name", material.name);
            trackUndo(ctx);
        }

        changed |= ui.colorEdit("Base color", material.baseColor);
        trackUndo(ctx);
        baseColorMapRow(ctx, handle, material);
        changed |= ui.sliderFloat("Roughness", material.roughness, 0.0f, 1.0f);
        trackUndo(ctx);
        changed |= ui.sliderFloat("Metallic", material.metallic, 0.0f, 1.0f);
        trackUndo(ctx);
        changed |= ui.colorEdit("Emissive", material.emissiveColor);
        trackUndo(ctx);
        changed |= ui.sliderFloat("Glow", material.emissiveStrength, 0.0f, MAX_GLOW, "%.1f");
        trackUndo(ctx);

        i32 mode = static_cast<i32>(std::find(ALPHA_MODES.begin(), ALPHA_MODES.end(), material.alphaMode) - ALPHA_MODES.begin());
        if (ui.segmented("Alpha", mode, ALPHA_MODE_LABELS)) {
            material.alphaMode = ALPHA_MODES[mode];
            changed = true;
        }
        trackUndo(ctx);

        if (material.alphaMode != AlphaMode::Opaque) {
            changed |= ui.sliderFloat("Opacity", material.opacity, 0.0f, 1.0f);
            trackUndo(ctx);
        }
        if (material.alphaMode == AlphaMode::Cutout) {
            changed |= ui.sliderFloat("Cutoff", material.alphaCutoff, 0.0f, 1.0f);
            trackUndo(ctx);
        }

        changed |= ui.checkbox("Both sides", material.doubleSided);
        trackUndo(ctx);

        ui.label("Used by " + describeUse(materialUse(ctx, handle)), true);

        // Selected faces in face mode; otherwise the selected objects (object mode) or the object being edited
        const Rect assignRow = ui.row();
        if (assignsToFaces(ctx)) {
            const std::size_t faces = ctx.scene.selection.getFaces().size();
            const std::string label = "Assign to " + std::to_string(faces) + (faces == 1 ? " face" : " faces");
            if (ui.button(label, assignRow)) assignFaceMaterial(ctx, handle);

            // Back to the object's material, whatever material the tab shows
            if (ui.button("Use object's material", ui.row())) assignFaceMaterial(ctx, INVALID_MATERIAL);
        } else {
            const std::vector<ObjectHandle> targets = materialTargets(ctx);
            const std::string label = targets.size() > 1 ? "Assign to " + std::to_string(targets.size()) + " objects" : "Assign to selected";
            if (ui.button(label, assignRow, !targets.empty())) assignMaterial(ctx, targets, handle);
        }

        const bool faceMode = ctx.systems.input_ctx.getModeContext() == InputContext_SelectionFace;
        if (faceMode && ui.button("Select its faces", ui.row(), ctx.scene.objects.isValid(ctx.scene.selection.getActiveObject()))) {
            selectFacesWithMaterial(ctx, handle);
        }

        ui.popId();

        if (changed && materials.isValid(handle)) materials.get(handle) = material;
    }

    // Heading with + and - on the right, then a fixed-height scrolling list
    void imageListSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        ReferenceCollection& references = ctx.scene.references;
        Selection& selection = ctx.scene.selection;

        const ListHeader header = listHeader(ui, "Reference images", 0.0f);
        // The file dialog opens next frame, not while drawing
        if (ui.button("+", header.plus)) ctx.referenceRequested = true;
        if (ui.button("-", header.minus, selection.hasReferences())) deleteSelectedReferences(ctx);

        ui.beginChild("list", listHeight());
        for (ReferenceHandle handle : references.handles()) {
            const ReferenceImage& image = references.get(handle);
            const std::string detail = !image.visible ? "hidden" : image.locked ? "locked" : "";

            // Locked images can still be picked here, to change them or unlock them
            ui.pushId(handle.index);
            if (ui.selectable(image.name, selection.hasReference(handle), detail)) {
                selection.clear();
                selection.addReference(handle);
            }
            ui.popId();
        }
        ui.endChild();
    }

    // Edits a copy so an undo restore mid-frame never leaves a dangling reference
    void selectedImageSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        ReferenceCollection& references = ctx.scene.references;
        const std::vector<ReferenceHandle>& selected = ctx.scene.selection.getReferences();

        ui.heading("Selected image");

        if (selected.empty() || !references.isValid(selected.front())) {
            ui.label("No image selected", true);
            return;
        }

        const ReferenceHandle handle = selected.front();
        ReferenceImage image = references.get(handle);
        bool changed = false;

        ui.pushId(handle.index);

        changed |= ui.textField("Name", image.name);
        trackUndo(ctx);

        if (image.picture) {
            ui.label(image.picture->fileName + ", " + std::to_string(image.picture->width) + " x " + std::to_string(image.picture->height), true);
        }

        changed |= ui.checkbox("Visible", image.visible);
        trackUndo(ctx);
        // A locked image ignores clicks in the viewport; it can still be picked in the list above
        changed |= ui.checkbox("Locked", image.locked);
        trackUndo(ctx);

        changed |= ui.sliderFloat("Opacity", image.opacity, 0.0f, 1.0f);
        trackUndo(ctx);

        i32 depth = static_cast<i32>(std::find(DEPTHS.begin(), DEPTHS.end(), image.depth) - DEPTHS.begin());
        if (ui.segmented("Depth", depth, DEPTH_LABELS)) {
            image.depth = DEPTHS[depth];
            changed = true;
        }
        trackUndo(ctx);

        changed |= ui.dragFloat3("Position", image.position, 0.01f);
        trackUndo(ctx);

        Vec3 degrees = image.rotation * RADIANS_TO_DEGREES;
        if (ui.dragFloat3("Rotation", degrees, 0.5f, "%.0f")) {
            image.rotation = degrees / RADIANS_TO_DEGREES;
            changed = true;
        }
        trackUndo(ctx);

        // Its height; the width follows the picture's proportions
        if (ui.dragFloat("Size", image.size, 0.01f)) {
            image.size = std::max(image.size, MIN_REFERENCE_SIZE);
            changed = true;
        }
        trackUndo(ctx);

        ui.popId();

        if (changed && references.isValid(handle)) references.get(handle) = image;
    }

    void ambientSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        LightCollection& lights = ctx.scene.lights;

        ui.heading("Ambient");

        f32 strength = lights.getAmbient().strength;
        if (ui.sliderFloat("Strength", strength, 0.0f, 1.0f)) lights.setAmbientStrength(strength);
        trackUndo(ctx);

        Vec3 color = lights.getAmbient().color;
        if (ui.colorEdit("Color", color)) lights.setAmbientColor(color);
        trackUndo(ctx);
    }

    // View-only setting, so no undo
    void headlightSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;
        Headlight& headlight = ctx.viewport.headlight;

        ui.heading("Headlight");
        ui.checkbox("Enabled", headlight.enabled);
        ui.sliderFloat("Strength", headlight.strength, 0.0f, 1.0f);
        ui.colorEdit("Color", headlight.color);
    }

    // View-only setting, so no undo
    void exposureSection(AppContext& ctx) {
        UIContext& ui = ctx.ui;

        ui.heading("Exposure");
        ui.sliderFloat("Stops", ctx.viewport.exposure, ViewportSettings::MIN_EXPOSURE, ViewportSettings::MAX_EXPOSURE, "%.1f");
    }
}

void drawMainPanel(AppContext& ctx, const Rect& bounds) {
    UIContext& ui = ctx.ui;
    UIPanelState& panel = ctx.viewport.panel;

    // First use: top right of the viewport
    if (panel.rect.width == 0.0f) {
        const f32 height = std::min(PANEL_HEIGHT, bounds.height - PANEL_MARGIN * 2.0f);
        panel.rect = { bounds.right() - PANEL_WIDTH - PANEL_MARGIN, bounds.y + PANEL_MARGIN, PANEL_WIDTH, height };
    }

    ui.setViewport(bounds);
    ui.beginPanel("main panel", panel, bounds, TABS);

    // Each section gets its own ID scope so labels like "Strength" can repeat across sections
    if (panel.activeTab == OBJECTS_TAB) {
        ui.pushId("objects");
        objectListSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("selected object");
        selectedObjectSection(ctx);
        ui.popId();
    } else if (panel.activeTab == MATERIALS_TAB) {
        ui.pushId("materials");
        materialListSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("selected material");
        selectedMaterialSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("textures");
        textureListSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("selected texture");
        selectedTextureSection(ctx);
        ui.popId();
    } else if (panel.activeTab == LIGHTS_TAB) {
        ui.pushId("lights");
        lightListSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("selected light");
        selectedLightSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("ambient");
        ambientSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("headlight");
        headlightSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("exposure");
        exposureSection(ctx);
        ui.popId();
    } else if (panel.activeTab == REFERENCES_TAB) {
        ui.pushId("images");
        imageListSection(ctx);
        ui.popId();
        ui.spacing();

        ui.pushId("selected image");
        selectedImageSection(ctx);
        ui.popId();
    }

    ui.endPanel();
}
