# Factory presets

Each `NN-<name>.wt2` file is one factory slot. `NN` is the number on the device and in the editor: `01` is F01 (`init`), `09` is F09. The file is a `wt2-patch` with all 206 parameters.

Add or replace a sound from anywhere in the tree:

```
python3 tools/factory_import.py <slot> <file.wt2> <name>
```

Example, the command that installed F09:

```
python3 tools/factory_import.py 9 path/to/Filthy_Reece.wt2 reece1
```

`<name>` is lowercase, at most 11 characters (the device's name limit): letters, digits, or a hyphen.

The script copies the file here as `NN-<name>.wt2` (with that name inside the file) and regenerates:

- `core/wt_factory_gen.inc`, the firmware and wasm factory table
- the name list in `core/wt_patch.c`
- `PATCHES` in `web/app.js`, the editor's factory library (only when the editor source is present; it is not in this repository)

Five parameters have a different name in the WT-1 build and the WT-2 build. The generator writes both names with the same number, so the slot value matches the `.wt2` in either build.

Rebuild so the keyboard and the editor pick it up:

```
VARIANT=wt2 ./build-fw.sh
./web/build-wt2.sh   # editor only, not in this repository
```
