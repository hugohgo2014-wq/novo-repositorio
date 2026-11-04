# NEBULA 360 Studio — Pro Workspace

Aplicativo Android em **Java + C++ (NDK/CMake)** para visualizar vídeo equiretangular em uma esfera 360° e exportar uma cópia MP4 com metadados esféricos Google Spatial Media V1.

## Workspace Pro (v1.1.0)

A interface foi reorganizada com referências de fluxo de trabalho do Adobe Premiere Pro e DaVinci Resolve: barra de projeto, monitor de programa, transporte, Project Media, timeline com faixas, ferramentas rápidas e Inspector. O desenho é adaptado para celular, com hierarquia visual consistente, estados explícitos e controles com altura mínima de 48dp sempre que aplicável.

### O que esta versão faz

- Importa um ou vários arquivos de vídeo pelo seletor do Android.
- Mostra uma prévia esférica 360° com OpenGL ES 2.0.
- Arrastar com um dedo gira a câmera; pinça altera o campo de visão.
- Organiza os clipes importados em uma timeline visual com faixa de vídeo, régua, playhead, zoom e seleção.
- Permite dividir visualmente um clipe em segmentos, duplicar e excluir segmentos da timeline.
- Possui painéis de Edição, Cor, Áudio e 360°, além de Inspector com propriedades do clipe selecionado.
- Exporta o clipe selecionado como cópia MP4 com metadados esféricos V1, preservando os bytes de áudio/vídeo.
- O processamento nativo C++ copia o arquivo em blocos e injeta o UUID de metadados espaciais.

### Limitações honestas desta versão

Esta versão **ainda não é um NLE completo**. A timeline já organiza e permite manipular segmentos visualmente, mas as operações de dividir/duplicar/excluir ainda não são renderizadas como uma sequência composta. A exportação injeta metadados no arquivo do clipe selecionado, não renderiza todos os clipes da timeline em um único filme. Os painéis de cor e áudio são estrutura de workspace, ainda sem ferramentas de processamento. Também não há costura de câmeras, títulos renderizados, transições, waveform ou keyframes.

O próximo passo técnico para exportação real de sequências é integrar Jetpack Media3 Transformer/Composition, com renderização acelerada por MediaCodec/OpenGL, e depois aplicar os metadados esféricos ao MP4 final. O Transformer oferece trimming, efeitos, transcodificação e composição de mídia; consulte a documentação oficial antes de ampliar o pipeline.

A prévia depende dos codecs suportados pelo aparelho Android. O vídeo de origem precisa já ser equiretangular (normalmente 2:1). Selecionar um modo estereoscópico nos metadados não converte vídeo mono em vídeo 3D real.

### Referências de interface e engenharia

- Adobe Premiere: painéis Project, Source/Program Monitor e Timeline: https://www.adobe.com/learn/premiere-pro/web/get-started-premiere-pro
- Adobe Premiere Mobile workspace: https://helpx.adobe.com/premiere/mobile/get-started/workspace-overview.html
- DaVinci Resolve Edit page: https://www.imaster.org/docs/davinci-resolve-21-manual/part-6-edit/ch.-40-using-the-edit-page
- Android touch targets (48dp): https://developer.android.com/guide/topics/ui/accessibility/views/apps-views
- Android Media3 Transformer: https://developer.android.com/media/media3/transformer

## Compilação pelo GitHub Actions

1. Crie um repositório no GitHub.
2. Envie **todo o conteúdo deste ZIP** para a raiz do repositório.
3. Abra a aba **Actions**.
4. Se necessário, habilite workflows no repositório.
5. Execute **Build NEBULA 360 Studio APK** em `workflow_dispatch`, ou faça um push na branch `main`/`master`.
6. Ao terminar, abra a execução e baixe o artefato `NEBULA-360-Studio-debug`.

O workflow instala JDK 17, Gradle 8.13, Android SDK 36, NDK `27.2.12479018` e CMake `3.22.1`. O Android Gradle Plugin está fixado em `8.13.2`, compatível com Gradle 8.13. O build gera APK debug não assinado para distribuição de teste.

## Compilação local

Com Android SDK, NDK `27.2.12479018`, CMake `3.22.1`, JDK 17 e Gradle 8.13 instalados:

```bash
gradle --no-daemon assembleDebug
```

Saída:

```text
app/build/outputs/apk/debug/app-debug.apk
```

## Estrutura

```text
NEBULA360Studio/
├── .github/workflows/build-apk.yml
├── app/
│   ├── build.gradle
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/nebula360/studio/
│       │   ├── MainActivity.java
│       │   ├── SpherePreview.java
│       │   └── NativeInjector.java
│       ├── cpp/
│       │   ├── CMakeLists.txt
│       │   └── spherical_injector.cpp
│       └── res/
├── build.gradle
├── gradle.properties
└── settings.gradle
```

## Referências técnicas

- Google Spatial Media: https://github.com/google/spatial-media
- Spherical Video Metadata V1: https://github.com/google/spatial-media/blob/master/docs/spherical-video-rfc.md
- Android NDK / CMake: https://developer.android.com/ndk/guides/cmake
- Android Gradle Plugin: https://developer.android.com/build/releases/gradle-plugin
- Gradle Actions: https://github.com/gradle/actions
