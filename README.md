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
