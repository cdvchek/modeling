#pragma once

#include <types>

enum class Action : u8 {
    Quit,
    ToggleConsole,    // Toggles whether the console is open or closed
    EnterCommand,     // Button to push to submit the command in the console if its open
    ConsoleBackspace,
    ConsoleDelete,
    ConsoleCursorLeft,
    ConsoleCursorRight,
    ConsoleHistoryOlder,
    ConsoleHistoryNewer,
    ViewportOrbit,    // Orbit around camera target
    ViewportPan,      // Pan tangentially to the camera target
    ViewportZoom,     // Zoom in and out of the camera target
    VertexMode,       // Change the selection mode to vertex selection
    EdgeMode,         // Change the selection mode to edge selection
    FaceMode,         // Change the selection mode to face selection
    Select,           // Select one or more vertices
    AddSelection,     // Add one or more vertices to the current selection
    RemoveSelection,  // Remove one or more vertices from the current selection
    GrabSelection,    // Move the selection tangent to the camera's direction
    ConfirmGrab,      // Quit grabbing and leave vertices where they are
    CancelGrab,       // Quit grabbing and reset vertices to where they were before grab
    ScaleSelection,   // Scale the vertices selected in or out
    ConfirmScale,     // Quit scaling and leave the vertices where they are
    CancelScale,      // Quit scaling and reset vertices to where they were before scale
    ExtrudeSelection, // Extrude the vertices selected
    InsetSelection,   // Inset the vertices selected
    DeleteSelection,  // Delete one or more selected vertices
    FillFaceLoop,     // Fills a half edge loop by creating a face
    ConnectVertices,  // Connects two vertices together by creating a face
    RotateSelection,  // Rotates the selection around its average position in the plane perpendicular to the camera
    RotateConfirm,
    RotateCancel,
    XAxis,
    YAxis,
    ZAxis,
    Count
};