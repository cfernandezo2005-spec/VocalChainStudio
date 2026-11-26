# Vocal Chain Studio (v0.2)

Host de cadena de voz (VST3 + Standalone) con:
- **Slots por secciones**: PRE MIX / VOZ PRINCIPAL / BUS / REVERB / DELAY. Cada slot: On/Off, subir, bajar, abrir, borrar.
- **Efectos incluidos**: Input Gain, Vocal EQ, De-Esser, Compressor, Saturation, Reverb, Delay.
- **Tus plugins VST3** (los que ya tengas instalados): boton "Plugins..." para escanear y "+ Anadir" para cargarlos.
- **Asistente de texto**: "mas brillante", "menos sibilancia", "mas reverb", "quita el barro", "preset rap"...
- **Escuchar mi voz (8 s)**: analiza tu voz y arma la cadena (reglas sobre el espectro y la dinamica, no una red neuronal).
- **Presets de estilo**: Pop limpio, Rap / Trap, Podcast, Calido intimo, Radio brillante, Ambiental.
- Se guarda la cadena (y el estado de cada plugin) con el proyecto del DAW.

## Compilar
Requisitos: CMake 3.22+, Git, compilador (Visual Studio 2022 / Xcode / GCC).

    cmake -B build
    cmake --build build --config Release

El .vst3 queda en build/VocalChainStudio_artefacts/Release/VST3/ (y se copia a la carpeta VST3 del sistema).
Puedes probarlo sin DAW con la version Standalone.

## Notas
- Solo estéreo. Los plugins externos con sidechain se cargan sin esa entrada.
- Si el compilador da algun error, copia el mensaje y lo corregimos.

## Obtener el .vst3 SIN instalar nada (GitHub Actions)
1. Crea un repositorio en GitHub (puede ser privado) y sube TODO el contenido de esta carpeta, incluida `.github/`.
2. Ve a la pestana **Actions** -> **Build VST3** -> **Run workflow**.
3. Espera ~10-15 min. Dentro de la ejecucion, abajo en **Artifacts**, descarga `VocalChainStudio-windows-latest` (o macOS).

## Instalar en FL Studio (Windows)
1. Copia la carpeta **Vocal Chain Studio.vst3** a `C:\Program Files\Common Files\VST3\`.
2. FL Studio: Options -> Manage plugins -> **Find installed plugins** (con "Verify plugins" activado).
3. Anade "Vocal Chain Studio" como efecto en un canal del Mixer.
(Al no estar firmado, Windows puede mostrar un aviso de SmartScreen.)
