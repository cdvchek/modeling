#pragma once

#include <types>

enum class Action : u8 {
    Quit,
    ViewportOrbit,   // Orbit around camera target
    ViewportPan,     // Pan tangentially to the camera target
    ViewportZoom,    // Zoom in and out of the camera target
    Select,          // Select one or more vertices
    AddSelection,    // Add one or more vertices to the current selection
    RemoveSelection, // Remove one or more vertices from the current selection
    GrabSelection,   // Move the selection tangent to the camera's direction
    ConfirmGrab,     // Quit grabbing and leave vertices where they are
    CancelGrab,      // Quit grabbing and reset vertices to where they were before grab
    ScaleSelection,  // Scale the vertices selected in or out
    ConfirmScale,    // Quit scaling and leave the vertices where they are
    CancelScale,     // Quit scaling and reset vertices to where they were before scale
    DeleteSelection, // Delete one or more selected vertices
    Duplicate,       // Duplicate one or more selected vertices
    Disconnect,      // Disconnect two vertices
    Save,            // Save the project
    Undo,            // Undo the last action
    Redo,            // Redo the last action
    Count
};