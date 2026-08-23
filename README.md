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
