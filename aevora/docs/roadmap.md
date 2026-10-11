# Roadmap

Each step ends with something that runs. Valuma's rigging, animation, collision shapes, levels of detail, and sockets are a separate track that has to finish before characters (step 9).

1. **Aevora skeleton. Done** (see [editor.md](editor.md) and [project.md](project.md)). The `aevora/` folder: an engine library, the editor program on the shared window, input, console, and UI code, and a tests program. The editor opens a project folder through its `.aev` file and has workspace tabs. It stays on OpenGL until step 6.
2. **Job system. Done** (see [jobs.md](jobs.md)). Work spread across cores: worker threads with priorities, `parallelFor`, groups, and tasks handed back to the main thread. It lives in Aevora's engine library.
3. **Deterministic math.** The engine's own trigonometry, powers, noise, and random numbers, with tests that pin exact results so every machine builds the same world.
4. **Planet grid.** The cube sphere: by-angle mapping, addresses for cells and chunks, and the one neighbor lookup that handles face edges and corners.
5. **Planet generator with a map view.** Plates, elevation, erosion and rivers, climate, biomes, and the cube turned so its corners sit in deep ocean. A map workspace shows each stage as a layer.
6. **Vulkan renderer.** The graphics interface (shaped so DirectX 12 fits later), HLSL shaders, and the UI drawn through it. The editor moves over.
7. **Terrain.** Layered heightmap chunks with level of detail on the sphere, the bend, then caves and overhangs from curves. Site scenes for one biome at a time.
8. **Entities, data, and play in the editor.** The entity system, the text data format, hot reload of the game library, and the play button.
9. **Characters and physics.** A character controller on the terrain, collision, and the first gym with measurements.
10. **Multiplayer.** Ownership, a dedicated server from the same code, and several players launched from the editor.
11. **The world's systems.** Water, building, creatures, weather and seasons, zoos and museums, mods.

## What the curvature test showed

A throwaway program drew a planet with rough terrain, trees, and a sea, and bent it live to look like a larger one.

- A real radius of about 28.6 km drawn to look like about 200 km reads as a big world. At eye height the sea horizon is then about 825 m away (310 m unbent).
- The bend held up at dragon speed and from a 2 km peak, where the horizon sits about 8 degrees below eye level (21 unbent).
- Walking at a realistic 5.5 km/h felt far too slow; game-typical speeds were chosen instead (see The game).
- Scale can't be judged from bare terrain. Trees, person-sized markers, and range rings on the ground were needed, and the real editor should have range rings as a debug view.
