# Train simulator

## Train packages

Every direct subdirectory of `trains/` is loaded as a train package. Put a
model (`.glb`, `.gltf`, or `.obj`) and a `config.json` in that directory. The
legacy filename `confg.json` is also accepted, so the included TEM2 package
works without renaming it.

The loader applies the `name`, `type`, `description`, `physical`, and
`technical` properties from the configuration to its loaded train instance.
For an idling engine sound, set `visual.engine_sound` to a PCM WAV filename;
the file is resolved relative to the package's `sounds/` directory:

```json
{
  "name": "TEM2",
  "visual": { "engine_sound": "engine.wav" }
}
```

At runtime the engine sound loops at the train's world position. OpenAL uses
the camera as its listener, so volume falls off naturally when the camera moves
away. Build with OpenAL development headers and link the OpenAL library to
enable sound; a build without OpenAL still loads all train packages and reports
that sound is disabled.

## Track builder

Press `H` to enter the track builder, `I` for straight track, and `J` for a
curve. Clicking near either endpoint of existing track snaps the new track's
starting point and tangent to that endpoint. A straight track must remain
within 5° of that tangent; connecting two existing endpoints with the straight
tool is intentionally ignored. Curves are limited to a 20 m minimum radius and
90° maximum turn, preventing unrealistically sharp geometry.

Pressing `Esc` saves all placed rail segments to `maps/latest_track_map.json`.
The human-readable JSON file is versioned and stores each segment's endpoints,
initial heading, curvature, and length; a later save replaces the previous map.
The simulator loads this file automatically on startup; it uses the built-in
demonstration track only when the file is absent or invalid.

Press `P` to create a custom train route. Left-click rail segments to add blue
route points; every connection follows the shortest path through the existing
rails, including rail junctions, rather than a direct line or their creation
order. Disconnected rails cannot be added to the same route. A route is changed
only by clicks: simply moving the cursor over rails does not add or preview a
connection. Click the first point to close the route. Press `P` to close or
reopen the route editor without changing a completed route; closing the editor
also hides its blue route guide. After reopening, the first click on a rail
starts its replacement. Right-click cancels the route being created, or press
`X` at any time to completely clear the
custom route and return trains to the normal track route. Every route point
between the endpoints is a stop: the train brakes to a complete halt there and
waits briefly before continuing to the next point. This also ensures a stop at
a point before the route leaves it in the opposite direction or onto another
branch. Sharp junctions inserted while the simulator finds a shortest rail
path are also stops, even when they are not a point clicked in the editor. An
open route makes trains shuttle between its endpoints; at an endpoint, a train
reverses its movement direction while keeping its current visual orientation. A
train also keeps its orientation when a route traverses a junction onto a rail
in the opposite direction. A closed route loops continuously.
