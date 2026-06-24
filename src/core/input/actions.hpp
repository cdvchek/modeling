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
    Insert,          // Insert a vertex
    DeleteSelection, // Delete one or more selected vertices
    Duplicate,       // Duplicate one or more selected vertices
    Connect,         // Connect two vertices
    Disconnect,      // Disconnect two vertices
    Save,            // Save the project
    Undo,            // Undo the last action
    Redo,            // Redo the last action
    Count
};