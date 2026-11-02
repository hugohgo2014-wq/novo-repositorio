# GDevelop C++ Runtime — v1.0.0

Engine de jogos em **C++17** que lê projetos no formato do GDevelop (`project.json`) e roda no Android
com **OpenGL ES 3** (2D + 3D na mesma câmera em perspectiva). O APK é gerado pelo **GitHub Actions**.

## Como gerar o APK (GitHub Actions)
1. Suba esta pasta inteira para um repositório no GitHub (branch `main`).
2. Vá em **Actions → Build APK → Run workflow** (também roda a cada push).
3. Ao terminar, baixe o artefato **gdcpp-player-apk** (`app-debug.apk` e `app-release.apk`).
   O release é assinado com a chave de debug para instalar direto; troque por uma keystore sua para publicar.

Requisitos do aparelho: Android 11+ (API 30), OpenGL ES 3.0.

## Estrutura
| Pasta | Conteúdo |
|---|---|
| `engine/` | Runtime C++: JSON, variáveis, expressões, cenas, eventos, colisão, renderer GLES |
| `android/` | Projeto Gradle + NativeActivity (sem Java). Assets do jogo em `app/src/main/assets/game/` |
| `tests/` | Testes do engine (rodam sem GPU, no Actions e localmente) |
| `tools/` | `make_sample_game.py` (jogo demo) e `export_game.py` (exportador v1: copia projeto+recursos para o APK) |

## Exportar um jogo próprio
```
python3 tools/export_game.py MeuJogo/project.json MeuJogo/assets
```
Depois rode o workflow. O APK passa a rodar o seu jogo.

## O que a v1.0.0 suporta
- **Projeto**: cenas, objetos, instâncias, camadas, grupos de objetos, variáveis (número/texto/booleano/estrutura/array), recursos de imagem.
- **Objetos**: `Sprite` (1ª animação, 1º frame), `Scene3D::Cube3DObject` (3D com iluminação básica).
- **Eventos**: Standard, Group, Comment, Repeat, While, ForEach, com seleção de objetos e subeventos.
- **Condições**: Always, DepartScene, VarScene, VarGlobal, CompareNumbers, CompareStrings, KeyPressed, MouseButtonPressed (toque = botão esquerdo), Timer, CollisionNP (AABB).
- **Ações**: ModVarScene, ModVarGlobal, SetX/SetY/SetXY, SetAngle, Rotate, SetNumberObjectVariable, Create, Delete, ResetTimer, Scene, Quit.
- **Expressões**: aritmética, textos, TimeDelta, Random*, MouseX/Y, Variable, GlobalVariable, `Objeto.X()/Y()/Width()/...`, funções matemáticas.
- Instruções não suportadas são registradas no log (`adb logcat -s gdcpp`) uma vez cada, sem travar o jogo.

## Fora da 1.0.0 (próximas versões)
Texto, áudio, física, comportamentos (plataforma, top-down, tween...), animações de Sprite, câmera/zoom por camada,
modelos 3D (glTF), extensões, geração de C++ a partir dos eventos (hoje os eventos são interpretados), editor.

## Limitações conhecidas
- O código Android/GLES **não foi executado em aparelho** durante o desenvolvimento: só o núcleo foi compilado e testado (com ASan/UBSan). Se o Actions apontar erro de compilação ou o jogo abrir em tela preta, mande o log.
- Colisão usa caixas alinhadas aos eixos (ignora rotação).
- O Gradle Wrapper não está incluído; o workflow usa o Gradle 8.9 instalado pela action.
