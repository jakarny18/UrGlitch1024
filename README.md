# GLTCH-1024 (VST3 Windows)

Port JUCE du séquenceur glitch HTML : 6 pistes (KICK, SNARE, HAT, CLAP, BASS, LEAD), 1024 pas,
glitch lane, FX perf, ~2500 sons procéduraux, 50 presets.

## Obtenir le .vst3 (sans rien installer)

1. Crée un dépôt GitHub et envoie tous les fichiers de ce dossier à la racine (Add file > Upload files). Puis crée à la main le fichier `.github/workflows/build.yml` (Add file > Create new file, tape ce chemin dans le nom) et colles-y le contenu de `build.yml`.
2. Onglet **Actions** > workflow **Build VST3 (Windows)** (se lance au push, ou « Run workflow »).
3. Télécharge l'artefact **GLTCH-1024-VST3-Windows** (dossier `GLTCH-1024.vst3`).
4. Copie `GLTCH-1024.vst3` dans `C:\Program Files\Common Files\VST3\` puis rescanne les plugins dans ta DAW.

L'artefact **Standalone** est un .exe autonome, pratique pour tester sans DAW.

## Build local (optionnel)

Visual Studio 2022 + CMake :

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target GLTCH1024_VST3

## Utilisation en DAW

- Plugin de type instrument (sortie stéréo). Pense à armer/charger-le sur une piste instrument.
- **Synchro hôte** : quand la DAW joue, le séquenceur suit sa position et son tempo (1 pas = 1/16).
  Quand la DAW est à l'arrêt, le bouton PLAY lance l'horloge interne (BPM du plugin).
- Automatisables : BPM, densité, chaos, swing, ton, gamme, auto-aléa, choix des sons par piste,
  et les 15 FX perf (tu peux donc enregistrer des glitchs en automation).
- Les motifs sont sauvegardés avec le projet. Les presets sont dans `%APPDATA%\GLTCH-1024\presets.json`
  (même format que l'export de la page HTML : EXPORT/IMPORT .JSON compatibles).

## Différences avec la page HTML

- Pas de REC / EXPORT WAV : enregistre via ta DAW.
- Compresseur, reverb et filtres réécrits en C++ : très proches, pas identiques à Web Audio.
- Avec la synchro hôte, « ½ TEMPO » n'allonge plus les pas (il agit sur la durée des glitchs/roulades).

## Licence

JUCE est sous licence AGPLv3 ou licence commerciale : à vérifier si tu distribues le plugin.
