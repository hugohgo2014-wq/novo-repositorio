# GDevelop C++ Native Editor — Preview 1.1.0

Editor Android nativo em C++ usando **NativeActivity + EGL + OpenGL ES 3.0**. A interface é desenhada por shaders e primitivas OpenGL. Não usa WebView, HTML, CSS, JavaScript, Chromium, React ou Canvas.

## Objetivo desta versão

Substituir a interface web da tentativa anterior por uma base de editor nativa, mantendo a engine/runtime C++ existente.

## O que esta prévia faz

- Carrega a estrutura de `game/project.json` com o parser C++ existente.
- Lista cenas e objetos.
- Alterna entre cenas.
- Desenha o palco, grade e instâncias com OpenGL ES 3.0.
- Seleciona instâncias tocando nelas.
- Move a instância selecionada tocando em outra posição do palco.
- Cria uma definição de objeto baseada no primeiro objeto existente.
- Cria instâncias a partir do primeiro objeto da cena.
- Ajusta a posição X no inspetor e exclui instâncias.
- Grava um snapshot textual da sessão no armazenamento interno do aplicativo.

## Limitações explícitas

Esta ainda é uma prévia técnica, não um editor equivalente ao GDevelop completo. O botão de salvar grava um snapshot da sessão, **não reserializa o `project.json` original**. Ainda faltam: serializador JSON com preservação de extensões desconhecidas, seletor/importador nativo de arquivos, editor de propriedades numéricas, editor visual de eventos, gerenciamento de assets, undo/redo, preview de runtime dentro do editor e pipeline de exportação APK dentro do app. A interface OpenGL atual usa primitivas e uma fonte bitmap 5x7 desenhada como geometria; o próximo passo é um sistema de UI nativo mais completo e renderização de assets no palco.

## Compilar

O projeto Android usa NDK `26.1.10909125`, CMake `3.22.1`, API 34 e Gradle 8.9:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
gradle -p android assembleDebug
```

O APK Android precisa ser validado pelo GitHub Actions ou em uma máquina com Android SDK/NDK. O ambiente de edição não inclui um dispositivo Android para validar a interface visualmente.

## Estrutura

- `engine/`: runtime e parser C++ existentes.
- `android/app/src/main/cpp/android_main.cpp`: aplicação nativa e interface OpenGL ES 3.
- `android/app/src/main/cpp/CMakeLists.txt`: build NDK.
- `android/app/src/main/assets/game/`: projeto de demonstração.
