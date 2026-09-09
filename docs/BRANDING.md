# WumpaForge app icon

`assets/branding/wumpaforge-icon.png` is new project artwork, generated with the
built-in image generation tool on 2026-09-08. It is not extracted from the game ISO.
The selected source is square with alpha. `tools/package.py` uses macOS `sips`
and `iconutil` to generate the standard 16–1024 pixel Retina icon representations
under ignored `build/branding/`, then embeds `WumpaForge.icns` and the
`CFBundleIconFile` entry in the local app. The source artwork stays in Git;
compiled icon/app products stay ignored. Relaunch the app after packaging to
refresh its Dock icon.

## Generation prompt

> Use case: logo-brand. Create a polished macOS app icon for WumpaForge, a native port project for Crash Bandicoot: The Wrath of Cortex. A single large stylized tropical wumpa-like fruit, warm golden yellow fading into orange-red, two vivid green leaves, sitting in front of one small chunky wooden adventure-game crate. Playful early-2000s 3D platform game aesthetic, sculpted forms, bold readable silhouette, soft highlights and subtle depth, crisp clean edges that read at tiny Dock sizes. Square 1024x1024 composition, centered subject, comfortable 10% safe margins, deep teal rounded-square icon tile with a restrained rim and gentle shading. Fruit dominates; crate is a supporting base. No text, letters, numbers, characters, logos or watermark. Finished icon artwork, not a mockup or a sheet of variants. Transparent pixels outside the rounded tile if supported.

The tool returned 1254×1254 artwork; packaging creates the required icon sizes
without changing the selected source. The original tool output remains outside
the repository; the project copy is self-contained. This attribution does not
change the separate runtime/game licensing inventory.
