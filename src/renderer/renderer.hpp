#pragma once

#include <types>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

#include "core/math/mat4.hpp"
#include "core/math/vec3.hpp"
#include "renderer/gpu_mesh.hpp"
#include "renderer/render_stats.hpp"
#include "core/font/font_library.hpp"
#include "ui/ui_draw_list.hpp"

struct WindowHandle;

enum class RendererBackend {
    RB_None,
    RB_OpenGL,
    RB_Vulkan,
    RB_DirectX12,
    RB_Metal
};

struct RendererConfig {
    u32 width = 1280;
    u32 height = 720;
    bool enableVSync = true;
    bool enableValidation = true;
};

struct ClearState {
    float r = 0.15f;
    float g = 0.15f;
    float b = 0.15f;
    float a = 1.0f;
    float depth = 1.0f;
    float stencil = 1.0f;
    bool clearColor = true;
    bool clearDepth = true;
    bool clearStencil = false;
};

constexpr u32 MAX_DIRECTIONAL_LIGHTS = 4;
constexpr u32 MAX_LOCAL_LIGHTS = 8;

struct BackgroundGradient {
    // Dracula backgrounds: #343746 at the top to #191a21 at the bottom
    Vec3 top { 0.20f, 0.22f, 0.28f };
    Vec3 bottom { 0.10f, 0.10f, 0.13f };
};

// Colors here are linear (converted from the sRGB colors that were picked) and include intensity
struct LightingState {
    Vec3 ambientColor { 1.0f, 1.0f, 1.0f };
    f32 ambientStrength = 1.0f;

    // Directions are normalized and point the way the light travels; colors include intensity
    Vec3 directionalDirections[MAX_DIRECTIONAL_LIGHTS];
    Vec3 directionalColors[MAX_DIRECTIONAL_LIGHTS];
    u32 directionalCount = 0;

    // Point and spot lights; point lights use cosInner = -1, cosOuter = -2 so the cone never cuts them off
    Vec3 localPositions[MAX_LOCAL_LIGHTS];
    Vec3 localDirections[MAX_LOCAL_LIGHTS];
    Vec3 localColors[MAX_LOCAL_LIGHTS];
    f32 localRanges[MAX_LOCAL_LIGHTS] = {};
    f32 localCosInner[MAX_LOCAL_LIGHTS] = {};
    f32 localCosOuter[MAX_LOCAL_LIGHTS] = {};
    u32 localCount = 0;

    // The environment surfaces reflect and are lit by: a sky above and a ground below
    Vec3 skyColor { 0.3f, 0.3f, 0.3f };
    Vec3 groundColor { 0.1f, 0.1f, 0.1f };

    Vec3 cameraPosition;

    // Faces show a UV checker grid in place of their base colors
    bool uvChecker = false;
};

// What happens to the back of a face
enum class BackFaces : u8 {
    Tinted,     // drawn and tinted, so flipped or open faces stand out (clay view)
    Lit,        // drawn and lit as if they faced you (double-sided materials)
    Culled      // not drawn, as the game engine does for single-sided materials
};

// How a mesh's faces look: a material's values (colors sRGB), and how its back faces and transparency are drawn
struct SurfaceLook {
    Vec3 baseColor { 0.72f, 0.73f, 0.78f };
    f32 roughness = 0.5f;
    f32 metallic = 0.0f;
    Vec3 emissiveColor { 1.0f, 1.0f, 1.0f };
    f32 emissiveStrength = 0.0f;

    // Blended faces are see-through by opacity and don't write depth; draw them after everything solid, farthest
    // first. Back faces of a blended mesh are drawn before its front faces.
    bool blend = false;
    f32 opacity = 1.0f;
    // Cutout: pixels whose opacity (times the map's alpha) is below this aren't drawn; below 0 for no cutout
    f32 alphaCutoff = -1.0f;
    // A texture (from createTexture) multiplied into the base color through the UVs; 0 for none
    u32 baseColorMap = 0;

    BackFaces backFaces = BackFaces::Tinted;
};

// Every value the same: a run of draws in one material sends it once
inline bool sameSurface(const SurfaceLook& a, const SurfaceLook& b) {
    return a.baseColor.x == b.baseColor.x && a.baseColor.y == b.baseColor.y && a.baseColor.z == b.baseColor.z
        && a.roughness == b.roughness && a.metallic == b.metallic
        && a.emissiveColor.x == b.emissiveColor.x && a.emissiveColor.y == b.emissiveColor.y && a.emissiveColor.z == b.emissiveColor.z
        && a.emissiveStrength == b.emissiveStrength && a.blend == b.blend && a.opacity == b.opacity && a.backFaces == b.backFaces
        && a.alphaCutoff == b.alphaCutoff && a.baseColorMap == b.baseColorMap;
}

// One material's run of a mesh's faces (a FaceGroup), with how it looks
struct DrawPart {
    u32 firstIndex = 0;
    u32 indexCount = 0;
    SurfaceLook surface;
};

struct DrawCommand {
    bool showVerts = true;
    bool showEdges = true;
    bool showFaces = true;

    std::vector<VertexHandle> highlightedVerts;
    std::vector<EdgeHandle> highlightedEdges;
    std::vector<FaceHandle> highlightedFaces;

    IMesh* mesh = nullptr;
    Mat4 model;
    Mat4 mvp;

    // The object's own look: every face when parts is empty, and the selection tint's base
    SurfaceLook surface;
    // Faces drawn by material; empty draws every face in surface
    std::vector<DrawPart> parts;
    // Object mode's outline: every edge in the selection style, in one draw per pass
    bool outlineAll = false;
    // Edges that shade hard (smooth shading), drawn in their own color under the selection
    std::vector<EdgeHandle> hardEdges;
};

struct DrawText3DCommand {
    const std::string& text;
    const Vec3& position;
    const Vec3& right;
    const Vec3& up;
    f32 size;
    const Mat4& mvp;
};

// A picture on the unit square (-0.5 to 0.5 in X and Y) placed by mvp
struct DrawImageCommand {
    u32 texture = 0;
    Mat4 mvp;
    f32 opacity = 1.0f;
    bool depthTest = true;      // false draws it over whatever is there
};

struct DrawGridCommand {
    Mat4 viewProjection;
    Vec3 cameraPosition;
    f32 cameraDistance;
    f32 farPlane;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(void* window, void* surface, const RendererConfig& config) = 0;
    virtual bool loadFonts(const FontLibrary& fonts) = 0;
    virtual void shutdown() = 0;

    virtual void resize(u32 width, u32 height) = 0;
    virtual void setVSync(bool enabled) = 0;
    virtual bool getVSync() const = 0;

    virtual void beginFrame() = 0;
    virtual void beginMainPass(const ClearState& clearState) = 0;
    // Where the 3D scene draws, in window pixels (origin top left); drawUI goes back to the whole window
    virtual void setSceneViewport(u32 x, u32 y, u32 width, u32 height) = 0;
    virtual void setLighting(const LightingState& lighting) = 0;
    virtual void setBackground(const BackgroundGradient& background) = 0;
    virtual BackgroundGradient getBackground() const = 0;
    // Brightens or darkens lit surfaces before tone mapping, in stops (each +1 doubles the light)
    virtual void setExposure(f32 stops) = 0;
    virtual f32 getExposure() const = 0;
    virtual void setBackFaceTint(const Vec3& tint) = 0;
    virtual Vec3 getBackFaceTint() const = 0;
    virtual void draw(const DrawCommand& command) = 0;
    virtual void drawText3D(const DrawText3DCommand& command) = 0;
    virtual void drawGrid(const DrawGridCommand& command) = 0;
    virtual void drawImage(const DrawImageCommand& command) = 0;

    // A texture from 8-bit RGBA pixels, rows from the top, with mipmaps; 0 if it couldn't be made (too large)
    virtual u32 createTexture(const u8* pixels, u32 width, u32 height) = 0;
    virtual void destroyTexture(u32 texture) = 0;

    // A swatch: a lit sphere in the look, size pixels square, over a checkerboard when it's see-through, under fixed
    // studio lighting. Draws into texture (a new one when 0) and returns it; call outside the main pass.
    virtual u32 renderMaterialPreview(const SurfaceLook& look, u32 size, u32 texture) = 0;
    virtual void drawDebugLine(const Vec3& start, const Vec3& end, const Mat4& mvp) = 0;
    virtual void drawUI(const UIDrawList& list) = 0;
    virtual void endMainPass() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;

    // What the last finished frame did (GPU time from a few frames back, as it arrives)
    virtual RenderStats getStats() const = 0;

    virtual RendererBackend getBackend() const = 0;
    virtual const char* getBackendName() const = 0;
    ClearState m_clearState;
};

std::unique_ptr<IRenderer> createRenderer(RendererBackend backend);