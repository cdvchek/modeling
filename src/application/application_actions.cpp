#include "application/application.hpp"
#include "application/actions/editing_actions.hpp"
#include "application/actions/project_actions.hpp"
#include "application/actions/asset_actions.hpp"
#include "application/ui/modal_windows.hpp"
#include "application/actions/origin_actions.hpp"
#include "application/commands/shading_commands.hpp"
#include "application/uv/uv_editor.hpp"
#include "application/actions/checks/action_checks.hpp"

void Application::registerDefaultActions(AppContext& ctx) {
    auto& actions = ctx.systems.actions;

    actions.subscribe(Action::Quit,                DefaultKeybinds::Quit,                InputContext_Global);
    actions.subscribe(Action::ToggleConsole,       DefaultKeybinds::ToggleConsole,       InputContext_Global | InputContext_Console);
    actions.subscribe(Action::EnterCommand,        DefaultKeybinds::EnterCommand,        InputContext_Console);
    actions.subscribe(Action::ConsoleBackspace,    DefaultKeybinds::ConsoleBackspace,    InputContext_Console);
    actions.subscribe(Action::ConsoleDelete,       DefaultKeybinds::ConsoleDelete,       InputContext_Console);
    actions.subscribe(Action::ConsoleCursorLeft,   DefaultKeybinds::ConsoleCursorLeft,   InputContext_Console);
    actions.subscribe(Action::ConsoleCursorRight,  DefaultKeybinds::ConsoleCursorRight,  InputContext_Console);
    actions.subscribe(Action::ConsoleHistoryOlder, DefaultKeybinds::ConsoleHistoryOlder, InputContext_Console);
    actions.subscribe(Action::ConsoleHistoryNewer, DefaultKeybinds::ConsoleHistoryNewer, InputContext_Console);
    actions.subscribe(Action::ViewportOrbit,       DefaultKeybinds::ViewportOrbit,       InputContext_AnySelection);
    actions.subscribe(Action::ViewportPan,         DefaultKeybinds::ViewportPan,         InputContext_AnySelection);
    actions.subscribe(Action::ViewportZoom,        DefaultKeybinds::ViewportZoom,        InputContext_AnySelection);
    actions.subscribe(Action::VertexMode,          DefaultKeybinds::VertexMode,          InputContext_AnySelection);
    actions.subscribe(Action::EdgeMode,            DefaultKeybinds::EdgeMode,            InputContext_AnySelection);
    actions.subscribe(Action::FaceMode,            DefaultKeybinds::FaceMode,            InputContext_AnySelection);
    actions.subscribe(Action::Select,              DefaultKeybinds::Select,              InputContext_AnySelection);
    actions.subscribe(Action::ToggleSelection,     DefaultKeybinds::ToggleSelection,     InputContext_AnySelection);
    actions.subscribe(Action::SelectLoop,          DefaultKeybinds::SelectLoop,          InputContext_SelectionEdge | InputContext_SelectionFace);
    actions.subscribe(Action::SelectRing,          DefaultKeybinds::SelectRing,          InputContext_SelectionEdge);
    actions.subscribe(Action::GrabSelection,       DefaultKeybinds::GrabSelection,       InputContext_AnySelection);
    actions.subscribe(Action::ConfirmGrab,         DefaultKeybinds::ConfirmGrab,         InputContext_Grab);
    actions.subscribe(Action::CancelGrab,          DefaultKeybinds::CancelGrab,          InputContext_Grab);
    actions.subscribe(Action::ScaleSelection,      DefaultKeybinds::ScaleSelection,      InputContext_AnySelection);
    actions.subscribe(Action::ConfirmScale,        DefaultKeybinds::ConfirmScale,        InputContext_Scale);
    actions.subscribe(Action::CancelScale,         DefaultKeybinds::CancelScale,         InputContext_Scale);
    actions.subscribe(Action::ExtrudeSelection,    DefaultKeybinds::ExtrudeSelection,    InputContext_SelectionFace);
    actions.subscribe(Action::InsetSelection,      DefaultKeybinds::InsetSelection,      InputContext_SelectionFace);
    actions.subscribe(Action::DeleteSelection,     DefaultKeybinds::DeleteSelection,     InputContext_AnySelection);
    actions.subscribe(Action::FillFaceLoop,        DefaultKeybinds::FillFaceLoop,        InputContext_SelectionEdge);
    actions.subscribe(Action::FrameSelected,       DefaultKeybinds::FrameSelected,       InputContext_EditModes);
    actions.subscribe(Action::FrameAll,            DefaultKeybinds::FrameAll,            InputContext_EditModes);
    actions.subscribe(Action::IslandMode,          DefaultKeybinds::IslandMode,          InputContext_EditModes);
    actions.subscribe(Action::SelectAll,           DefaultKeybinds::SelectAll,           InputContext_EditModes);
    actions.subscribe(Action::UVGrab,              DefaultKeybinds::GrabSelection,       InputContext_EditModes);
    actions.subscribe(Action::UVScale,             DefaultKeybinds::ScaleSelection,      InputContext_EditModes);
    actions.subscribe(Action::UVRotate,            DefaultKeybinds::RotateSelection,     InputContext_EditModes);
    actions.subscribe(Action::ConnectVertices,     DefaultKeybinds::ConnectVertices,     InputContext_SelectionVertex);
    actions.subscribe(Action::XAxis,               DefaultKeybinds::XAxis,               InputContext_Grab | InputContext_Scale | InputContext_Rotate);
    actions.subscribe(Action::YAxis,               DefaultKeybinds::YAxis,               InputContext_Grab | InputContext_Scale | InputContext_Rotate);
    actions.subscribe(Action::ZAxis,               DefaultKeybinds::ZAxis,               InputContext_Grab | InputContext_Scale | InputContext_Rotate);
    actions.subscribe(Action::RotateSelection,     DefaultKeybinds::RotateSelection,     InputContext_AnySelection);
    actions.subscribe(Action::RotateConfirm,       DefaultKeybinds::RotateConfirm,       InputContext_Rotate);
    actions.subscribe(Action::RotateCancel,        DefaultKeybinds::RotateCancel,        InputContext_Rotate);
    actions.subscribe(Action::BevelSelection,      DefaultKeybinds::BevelSelection,      InputContext_EditModes);
    actions.subscribe(Action::ConfirmBevel,        DefaultKeybinds::ConfirmBevel,        InputContext_Bevel);
    actions.subscribe(Action::CancelBevel,         DefaultKeybinds::CancelBevel,         InputContext_Bevel);
    actions.subscribe(Action::ConfirmInset,        DefaultKeybinds::ConfirmInset,        InputContext_Inset);
    actions.subscribe(Action::CancelInset,         DefaultKeybinds::CancelInset,         InputContext_Inset);
    actions.subscribe(Action::Undo,                DefaultKeybinds::Undo,                InputContext_AnySelection);
    actions.subscribe(Action::Redo,                DefaultKeybinds::Redo,                InputContext_AnySelection);
    actions.subscribe(Action::RadialMenu,          DefaultKeybinds::RadialMenu,          InputContext_Global);
    actions.subscribe(Action::ObjectMode,          DefaultKeybinds::ObjectMode,          InputContext_AnySelection);
    actions.subscribe(Action::ToggleObjectMode,    DefaultKeybinds::ToggleObjectMode,    InputContext_AnySelection);
    actions.subscribe(Action::SaveProject,         DefaultKeybinds::SaveProject,         InputContext_AnySelection);
    actions.subscribe(Action::SaveProjectAs,       DefaultKeybinds::SaveProjectAs,       InputContext_AnySelection);
    actions.subscribe(Action::OpenProject,         DefaultKeybinds::OpenProject,         InputContext_AnySelection);
    actions.subscribe(Action::NewProject,          DefaultKeybinds::NewProject,          InputContext_AnySelection);
    actions.subscribe(Action::ExportAssets,        DefaultKeybinds::ExportAssets,        InputContext_AnySelection);
    actions.subscribe(Action::ImportAssets,        DefaultKeybinds::ImportAssets,        InputContext_AnySelection);
    actions.subscribe(Action::ParentToActive,      DefaultKeybinds::ParentToActive,      InputContext_SelectionObject);
    actions.subscribe(Action::ClearParents,        DefaultKeybinds::ClearParents,        InputContext_SelectionObject);
    actions.subscribe(Action::ModalConfirm,        DefaultKeybinds::ModalConfirm,        InputContext_Modal);
    actions.subscribe(Action::ModalCancel,         DefaultKeybinds::ModalCancel,         InputContext_Modal);

    auto always = [] { return true; };

    actions.setHandler(Action::VertexMode, { "Vertex", always, [&ctx] { setSelectionMode(ctx, InputContext_SelectionVertex); } });
    actions.setHandler(Action::EdgeMode, { "Edge", always, [&ctx] { setSelectionMode(ctx, InputContext_SelectionEdge); } });
    actions.setHandler(Action::FaceMode, { "Face", always, [&ctx] { setSelectionMode(ctx, InputContext_SelectionFace); } });
    actions.setHandler(Action::ObjectMode, { "Object", always, [&ctx] { setSelectionMode(ctx, InputContext_SelectionObject); } });
    actions.setHandler(Action::TogglePanel, { "Panel", always, [&ctx] { togglePanel(ctx); } });
    actions.setHandler(Action::ToggleHeadlight, { "Headlight", always, [&ctx] { toggleHeadlight(ctx); } });
    actions.setHandler(Action::ToggleDebug, { "Debug", always, [&ctx] { toggleDebugView(ctx); } });
    actions.setHandler(Action::ToggleObjectMode, { "Object/Edit", always, [&ctx] { toggleObjectMode(ctx); } });

    actions.setHandler(Action::GrabSelection, { "Grab", [&ctx] { return canGrab(ctx); }, [&ctx] { startGrab(ctx); } });
    actions.setHandler(Action::ScaleSelection, { "Scale", [&ctx] { return canScale(ctx); }, [&ctx] { startScale(ctx); } });
    actions.setHandler(Action::ExtrudeSelection, { "Extrude", [&ctx] { return canExtrude(ctx); }, [&ctx] { extrudeSelection(ctx); } });
    actions.setHandler(Action::InsetSelection, { "Inset", [&ctx] { return canExtrude(ctx); }, [&ctx] { beginInset(ctx); } });
    actions.setHandler(Action::DeleteSelection, { "Delete", [&ctx] { return canDelete(ctx); }, [&ctx] { deleteSelection(ctx); } });
    actions.setHandler(Action::FillFaceLoop, { "Fill", [&ctx] { return canFillFaceLoop(ctx); }, [&ctx] { fillFaceLoop(ctx); } });
    actions.setHandler(Action::ConnectVertices, { "Connect", [&ctx] { return canConnectVertices(ctx); }, [&ctx] { connectVertices(ctx); } });
    actions.setHandler(Action::RotateSelection, { "Rotate", [&ctx] { return canRotate(ctx); }, [&ctx] { startRotate(ctx); } });
    actions.setHandler(Action::BevelSelection, { "Bevel", [&ctx] { return canBevel(ctx); }, [&ctx] { beginBevel(ctx); } });

    actions.setHandler(Action::MergeVertices, { "Merge", [&ctx] { return canMergeVertices(ctx); }, [&ctx] { mergeVertices(ctx); } });
    actions.setHandler(Action::DissolveSelection, { "Dissolve", [&ctx] { return canDissolve(ctx); }, [&ctx] { dissolveSelection(ctx); } });

    auto projectFiles = [&ctx] { return canUseProjectFiles(ctx); };
    actions.setHandler(Action::SaveProject, { "Save", projectFiles, [&ctx] { saveProject(ctx); } });
    actions.setHandler(Action::SaveProjectAs, { "Save As", projectFiles, [&ctx] { saveProjectAs(ctx); } });
    actions.setHandler(Action::OpenProject, { "Open", projectFiles, [&ctx] { openProject(ctx); } });
    actions.setHandler(Action::NewProject, { "New", projectFiles, [&ctx] { newProject(ctx); } });
    actions.setHandler(Action::ExportAssets, { "Export", projectFiles, [&ctx] { openExportWindow(ctx); } });
    actions.setHandler(Action::ImportAssets, { "Import", projectFiles, [&ctx] { importAssets(ctx); } });
    auto origin = [&actions, &ctx](Action action, std::string_view label, OriginTarget target) {
        actions.setHandler(action, { label, [&ctx, target] { return canMoveOrigin(ctx, target); }, [&ctx, target] { moveOrigin(ctx, target); } });
    };
    origin(Action::OriginToGeometry, "To geometry", OriginTarget::Geometry);
    origin(Action::OriginToBottom, "To bottom", OriginTarget::Bottom);
    origin(Action::OriginToWorld, "To world", OriginTarget::World);
    origin(Action::OriginResetRotation, "Reset rotation", OriginTarget::WorldRotation);
    origin(Action::OriginToSelection, "Origin here", OriginTarget::Selection);
    actions.setHandler(Action::ParentToActive, { "Parent", [&ctx] { return canParentToActive(ctx); }, [&ctx] { parentToActive(ctx); } });
    actions.setHandler(Action::ClearParents, { "Unparent", [&ctx] { return canClearParents(ctx); }, [&ctx] { clearParents(ctx); } });
    actions.setHandler(Action::ToggleOrigins, { "Origins", always, [&ctx] { toggleOrigins(ctx); } });
    actions.setHandler(Action::ToggleMaterials, { "Materials", always, [&ctx] { toggleMaterials(ctx); } });
    actions.setHandler(Action::ToggleUVChecker, { "UV checker", always, [&ctx] { toggleUVChecker(ctx); } });
    auto hasUVObject = [&ctx] { return ctx.scene.objects.isValid(uvObject(ctx)); };
    actions.setHandler(Action::FrameSelected, { "Frame selected", hasUVObject, [&ctx] { frameView(ctx, false); } });
    actions.setHandler(Action::FrameAll, { "Frame all", hasUVObject, [&ctx] { frameView(ctx, true); } });
    actions.setHandler(Action::IslandMode, { "Island", hasUVObject, [&ctx] {
        setSelectionMode(ctx, InputContext_SelectionFace);
        ctx.workspace.uvIslands = true;
    } });
    actions.setHandler(Action::SelectAll, { "Select all", hasUVObject, [&ctx] { selectAllUVs(ctx); } });
    auto uvTool = [&ctx] { return canStartUVTool(ctx); };
    actions.setHandler(Action::UVGrab, { "Grab", uvTool, [&ctx] { startUVTool(ctx, UVToolKind::Grab); } });
    actions.setHandler(Action::UVScale, { "Scale", uvTool, [&ctx] { startUVTool(ctx, UVToolKind::Scale); } });
    actions.setHandler(Action::UVRotate, { "Rotate", uvTool, [&ctx] { startUVTool(ctx, UVToolKind::Rotate); } });
    auto shading = [&ctx] { return canSetShading(ctx); };
    actions.setHandler(Action::ShadeFlat, { "Flat", shading, [&ctx] { setShading(ctx, ShadingMode::Flat); } });
    actions.setHandler(Action::ShadeSmooth, { "Smooth", shading, [&ctx] { setShading(ctx, ShadingMode::Smooth); } });
    actions.setHandler(Action::ShadeAuto, { "Auto smooth", shading, [&ctx] { setShading(ctx, ShadingMode::Auto); } });
    auto marking = [&ctx] { return canMarkEdges(ctx); };
    actions.setHandler(Action::MarkHard, { "Mark hard", marking, [&ctx] { markSelectedEdges(ctx, EdgeMark::Hard); } });
    actions.setHandler(Action::MarkSmooth, { "Mark smooth", marking, [&ctx] { markSelectedEdges(ctx, EdgeMark::Smooth); } });
    actions.setHandler(Action::ClearEdgeMark, { "Clear mark", marking, [&ctx] { markSelectedEdges(ctx, EdgeMark::None); } });

    actions.setHandler(Action::ModalConfirm, { "OK", always, [&ctx] { confirmModal(ctx); } });
    actions.setHandler(Action::ModalCancel, { "Cancel", always, [&ctx] { cancelModal(ctx); } });

    actions.setHandler(Action::Undo, { "Undo", [&ctx] { return ctx.history.canUndo(); }, [&ctx] { undo(ctx); } });
    actions.setHandler(Action::Redo, { "Redo", [&ctx] { return ctx.history.canRedo(); }, [&ctx] { redo(ctx); } });

    actions.setHandler(Action::XAxis, { "X", always, [&ctx] { toggleAxis(ctx, InputContext_XAxis); } });
    actions.setHandler(Action::YAxis, { "Y", always, [&ctx] { toggleAxis(ctx, InputContext_YAxis); } });
    actions.setHandler(Action::ZAxis, { "Z", always, [&ctx] { toggleAxis(ctx, InputContext_ZAxis); } });
    actions.setHandler(Action::AxisFree, { "Free", [&ctx] { return canChangeAxis(ctx); }, [&ctx] { clearAxes(ctx); } });

    actions.setHandler(Action::LightPoint, { "Point", [&ctx] { return canSetLightType(ctx, LightType::Point); }, [&ctx] { setLightType(ctx, LightType::Point); } });
    actions.setHandler(Action::LightSpot, { "Spot", [&ctx] { return canSetLightType(ctx, LightType::Spot); }, [&ctx] { setLightType(ctx, LightType::Spot); } });
    actions.setHandler(Action::LightDirectional, { "Directional", [&ctx] { return canSetLightType(ctx, LightType::Directional); }, [&ctx] { setLightType(ctx, LightType::Directional); } });
    actions.setHandler(Action::ToggleLights, { "On/Off", [&ctx] { return canToggleLights(ctx); }, [&ctx] { toggleLights(ctx); } });
}
