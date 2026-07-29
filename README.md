# OctoSnare

Effet VST3 pour Bitwig : transforme n'importe quel snare en snare boom bap old school.

## La chaine d'effets

| Effet | Description |
|---|---|
| Pitch down | Re-pitch vers le bas (0 a -12 demi-tons), le grain des samples pitches facon SP-1200 / DJ Premier |
| Crunch | Reduction 12-bit + sample rate 26.04 kHz avec anti-aliasing, le son E-mu SP-1200 |
| Vintage | Filtre passe-bas resonant 24 dB/oct, emulation du caractere SSM2044 |
| Saturation | Waveshaper tanh sur-echantillonne 4x, chaleur tape/analogique |
| Crack | Transient shaper, booste l'attaque pour que le snare claque |
| Punch | Compression parallele "New York" (ratio 12:1, attaque 1 ms) melangee sous le signal |
| Boom | Boost low shelf 90 Hz + creux 300 Hz |
| Dust | Craquements vinyle |
| Room | Petite reverb courte type piece |

Chaque effet est activable/desactivable individuellement. Presets d'usine + sauvegarde de presets perso (Documents/OctoSnare/Presets).

L'interface affiche une boule liquide deformee par l'onde du snare et un analyseur de spectre temps reel.

## Telecharger le plugin (Windows)

1. Onglet **Actions** du repo GitHub
2. Ouvrir le dernier build vert
3. Telecharger l'artefact **OctoSnare-Windows-VST3**
4. Dezipper et copier le dossier `OctoSnare.vst3` dans `C:\Program Files\Common Files\VST3`
5. Dans Bitwig : Settings > Locations > rescan plugins

## Compiler soi-meme

Prerequis : CMake 3.22+, un compilateur C++17 (Visual Studio 2022 sur Windows).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target OctoSnare_VST3
```

Le plugin se trouve ensuite dans `build/OctoSnare_artefacts/Release/VST3/`.

JUCE est telecharge automatiquement par CMake (FetchContent).
