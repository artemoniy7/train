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

Press `H` to enter the track builder. `I` and `J` both select the smooth-rail
tool (the `I` shortcut is retained for compatibility). Clicking near either
endpoint of existing track snaps the new track's starting point and tangent to
that endpoint. Clicking the middle of an existing rail splits it at the click
and starts (or ends) a connected turnout there, so outgoing rails are part of
the same network rather than only visually touching. While extending a snapped rail, a small sideways mouse movement
is projected onto its outgoing tangent, making it easy to continue straight;
moving farther sideways starts a curve. When hovering another rail, both of
its travel directions are evaluated and the shortest smooth tangent join is
previewed. A proposed build is green when it is valid and red when its radius
or either smooth connection is impossible. Curves first use a
single arc when it matches both endpoint tangents. When the headings differ,
the builder samples a tangent-constrained smooth connection, so both snapped
rails meet without a sharp bend. Curves have a 20 m minimum radius and a 90°
maximum single-arc turn.

When saving, the human-readable JSON map stores each segment's endpoints,
initial heading, curvature, and length; a later save replaces the previous map.
The simulator loads this file automatically on startup; it uses the built-in
demonstration track only when the file is absent or invalid.

## Lighting and shadows

The scene uses a directional sun and moon cycle that completes in one minute.
The sky, ambient illumination, and shadow colour transition between daylight
and moonlight automatically. A 1024×1024 depth shadow map with a small 3×3
filter provides soft shadows from trains and rails onto the terrain while
keeping the rendering cost modest.

Press `P` to create a custom train route. Left-click rail segments to add blue
route points; every connection follows the shortest path through the existing
rails, including rail junctions, rather than a direct line or their creation
order. The prospective shortest path is shown in blue on hover but is only
committed by a click; disconnected rails cannot be added to the same route.
Click the first point to close the route. Press `P` to close or
reopen the route editor without changing a completed route; closing the editor
also hides its blue route guide. After reopening, the first click on a rail
starts its replacement. Right-click cancels an unfinished route without
destroying a completed one, or press `X` at any time to completely clear the
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

`Esc` is safe to use: it cancels a pending rail placement first, then closes an
active editor, and only saves the map and exits when no editor is active.
