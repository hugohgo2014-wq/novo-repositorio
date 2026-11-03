# NEBULA — Universe Sandbox Mobile

**Android · C++20 · OpenGL ES 3.0 · JNI · CMake · Gradle · GitHub Actions**

NEBULA is a mobile, real-time space sandbox. The Android interface is Java, while the simulation and renderer run natively in C++ through the Android NDK. Planet surfaces are generated in the vertex shader; no planet JPEG is used as the surface itself.

## Build the APK with GitHub Actions

1. Create a GitHub repository and upload the contents of this folder (the `.github` folder must be included).
2. Push to `main` or `master`, or open **Actions → Build NEBULA APK → Run workflow**.
3. Open the successful workflow run and download the artifact named **NEBULA-Universe-Sandbox-APK**.
4. Inside the artifact is `app-debug.apk`. Install it on an Android device that supports OpenGL ES 3.0.

The workflow uses `android-actions/setup-android@v4` to install Android API 35, Build Tools `35.0.0`, NDK `27.2.12479018` and CMake `3.22.1` while accepting SDK licenses in that action. It then configures Java 17 and Gradle `8.10.2` and runs `gradle assembleDebug`. The workflow deliberately does not run `sdkmanager --licenses` a second time.

### Build locally

Install Android Studio / Android SDK, NDK `27.2.12479018`, CMake `3.22.1`, Java 17 and Gradle 8.10.2. Then run from this directory:

```bash
gradle assembleDebug
```

APK output:

```text
app/build/outputs/apk/debug/app-debug.apk
```

## Controls

- **Drag:** orbit the free camera.
- **Pinch:** zoom in and out. The zoom range extends from system scale to close planetary inspection.
- **Two-finger drag:** pan the camera target; one-finger drag continues to orbit.
- **Double-tap a body:** select it, center the camera and show its physical information.
- **Ⅱ / ▶:** pause or resume the simulation. The camera remains usable while paused.
- **TEMPO ×:** cycle through 1×, 60×, 600×, 3,600× and 86,400× simulation time.
- **CÂMERA:** switch between free camera and close planetary flyover.
- **+ PLANETA:** add a new rocky body to the N-body system.
- **METEORO:** launch a 10 km-class asteroid toward the selected body (or Earth when the star is selected) to test crater formation, ejecta and atmospheric damage. Choose 60× first to watch the approach; at higher speeds the impact arrives quickly. A convenient test: double-tap Earth, set 60×, press METEORO, and watch the asteroid arrive.
- **GRAVIDADE:** switch between Newtonian stable gravity and the experimental finite-propagation demonstration.
- **LUZ / VISÃO:** switch between present-position rendering and a light-time observer view. Illumination uses the source's retarded emission state in both views.
- **EDITAR:** change the selected body's mass, radius and spin rate.
- **REMOVER:** remove the selected body. Removing the Sun lets you observe delayed sunlight at Earth; the historical light source remains available until its photons' travel time has elapsed.
- **RESET:** restore the Solar System scenario.

## Simulation systems

### N-body dynamics

The initial scene contains the Sun, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune and the Moon. Bodies use SI units internally (metres, kilograms and seconds), velocity-Verlet integration, pairwise gravity and a bounded simulation step. The time controller adjusts simulated seconds per real second.

The default gravity setting is the **experimental retarded-source mode**. It samples historical source positions from a time ring buffer using distance divided by `c`. The alternate Newtonian mode is the stable reference mode. This retarded Newtonian demonstration is intentionally not described as a full general-relativistic solver: a physically accurate relativistic N-body integrator needs the appropriate velocity-dependent and field terms, not merely a delayed inverse-square force. This distinction is important for long-term orbital accuracy.

### Light travel time

The renderer stores historical source positions and luminosity states. A body's illumination samples the luminous source at approximately `t - distance/c`. Thus, when the Sun is removed, Earth does not lose sunlight at the same simulation instant. At one astronomical unit the light travel time is about 499 seconds (roughly 8 minutes 19 seconds; it varies slightly with orbital distance).

The observer mode also samples historical object positions to demonstrate light-cone viewing. The normal mode keeps object positions current while retaining delayed illumination.

### Procedural planetary surfaces

The OpenGL ES shader creates terrain from 3D noise, ridged detail and persistent crater records. Geometry is displaced at render time, rather than displaying a flat planet image. The renderer switches between a lower-resolution and a high-resolution sphere mesh based on camera distance, while retaining shader noise and crater displacement. At system scale, apparent radii are enlarged for touch selection; physical radii remain unchanged in gravity and collision calculations. Rotation-induced flattening and a deliberately exaggerated visual tidal-deformation term affect the rendered shape. Actual equilibrium tides are tiny at planetary scale, so the visual multiplier makes the effect inspectable instead of pretending it is naturally enormous. Close flyover uses the same spherical terrain and atmospheric shell, so the view can approach the surface.

### Atmospheres and gas giants

Atmosphere shells use view-angle scattering, animated cloud bands and impact-event data. Impacts can create a directional atmospheric scar / opening effect and an expanding shock feature that recovers gradually. Gas-giant surface bands are animated in the shader; Jupiter and Saturn have translucent ring meshes.

### Multi-material collisions

Collision handling evaluates relative impact speed, reduced mass, specific impact energy, body material and an approximate catastrophic-disruption threshold. Small projectiles are captured rather than bouncing unrealistically off a planet; their impact energy is used for craters and ejecta. Low-energy contacts can accrete into a larger remnant with mass and linear momentum conserved; grazing impacts use a material-dependent restitution; energetic collisions reduce the largest remnant and spawn material-tagged ejecta. Ejecta retain iron / silicate / ice identity, ballistic velocity and a cooling state. Their local cloud uses a compact SPH kernel for density, compressive pressure and artificial viscosity, with material-dependent reference density and sound speed; a spatial hash limits neighbour work on mobile hardware. Crater records deform the target's procedural terrain, and atmospheric impacts modify the atmosphere shader.

This is a **mobile reduced-order impact model inspired by multi-material SPH giant-impact methods**, not a million-particle research hydrocode. Research-grade planetary impact calculations use very large SPH particle counts and material equations of state; the mobile model trades numerical resolution for interactive frame rates while retaining energy-based outcomes and visible material ejecta.

## Project layout

```text
.github/workflows/android.yml       GitHub Actions APK build
app/src/main/AndroidManifest.xml    Android app declaration
app/src/main/java/com/nebula/universelab/MainActivity.java  Touch UI, gestures, OpenGL surface and JNI bridge
app/src/main/cpp/CMakeLists.txt     Native build configuration
app/src/main/cpp/native-lib.cpp     N-body simulation, collisions, history, shaders and renderer
app/build.gradle                    Android Gradle / NDK configuration
```

## Technical references

- Stanford / Gravity Probe B: gravity propagation and relativistic corrections — https://einstein.stanford.edu/content/relativity/q1510.html
- Stanford / Gravity Probe B: why a naive Newtonian delay is not the full relativistic result — https://einstein-new.stanford.edu/content/relativity/a10662.html
- Lunar-forming impacts and SPH modelling — https://pmc.ncbi.nlm.nih.gov/articles/PMC4128262/
- Differentiated planetary impact simulations — https://academic.oup.com/mnras/article/517/3/3132/6632983
- Android NDK and CMake configuration — https://developer.android.com/studio/projects/install-ndk

## License

Source code in this project is provided under the MIT License. See `LICENSE`.

## Build troubleshooting

- If the job fails in the Android SDK setup step, check that `android-actions/setup-android@v4` is used and that the workflow has not added a second `sdkmanager --licenses` command.
- If the build reaches `:app:externalNativeBuildDebug`, inspect the CMake/NDK compiler output; that is a separate stage from SDK installation.
- The Android launcher Activity is `app/src/main/java/com/nebula/universelab/MainActivity.java`; its native declarations match the JNI exports in `native-lib.cpp`.

## Changes in the CI repair

- Android SDK setup was updated to `android-actions/setup-android@v4` and command-line tools `22.0` (build `15859902`); the action accepts licenses itself, so the workflow no longer invokes a duplicate manual license step.
- The workflow pins API 35, Build Tools `35.0.0`, NDK `27.2.12479018`, CMake `3.22.1`, Java 17 and Gradle `8.10.2`.
- Added the missing launcher `MainActivity.java`, including the OpenGL ES 3.0 surface, native JNI bridge, touch gestures, status panel and controls.
- Removed `-ffast-math` from the native compile flags to avoid relaxing floating-point behavior used by numerical simulation checks.
