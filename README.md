# Subdrum Processor (VST3 / AU / Standalone)

Un plugin de procesamiento DSP en C++ (JUCE 8 / C++20) enfocado en la manipulación y degradación armónica de baterías estilo **lo-fi / 2-step británico subterráneo**, logrando un tono denso, cálido, texturizado y con pegada analógica.

---

## 🛠 Arquitectura DSP

```
[Audio In] 
    │
    ▼
┌────────────────────────────────────────────────────────┐
│ 1. TAPE SATURATION & OVERDRIVE (4x Oversampled)        │
│    - Sobremuestreo polifásico 4x (Anti-aliasing)       │
│    - Transferencia no lineal asimétrica (2º/3º armón.) │
│    - Filtro DC Blocker (High-Pass @ 15 Hz)             │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ 2. VINTAGE SAMPLER FILTER (State-Variable TPT)         │
│    - Pendiente lo-fi con resonancia (20 Hz - 20 kHz)   │
│    - Emulación del roll-off de samplers clásicos       │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ 3. VCA TRANSIENT COMPRESSOR                            │
│    - Ataque agresivo (0.1 ms - 50 ms) para transitorios│
│    - Liberación rápida (10 ms - 400 ms)                │
│    - Soft-knee con sidechain estéreo enlazado          │
│    - Medidor visual de reducción de ganancia (GR)      │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ 4. VINYL TEXTURE & DUST ENGINE                         │
│    - Generador de ruido estocástico libre de bloqueo   │
│    - Banda pasante para hiss de ranura + rumble bajo   │
│    - Disparo tipo Poisson de clicks/pops de polvo      │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ 5. MASTER OUTPUT TRIM & SMOOTHING                      │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
                       [Audio Out]
```

---

## 🚀 Cómo Compilar el Proyecto

El archivo `CMakeLists.txt` está configurado para descargar automáticamente JUCE mediante `FetchContent` (si no está disponible localmente) y generar los targets **VST3**, **AU** (macOS) y **Standalone** (ejecutable de escritorio para pruebas rápidas sin DAW).

### En macOS:
```bash
# 1. Crear directorio de compilación
mkdir build && cd build

# 2. Configurar con CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. Compilar todos los formatos
cmake --build . --config Release -j$(sysctl -n hw.ncpu)

# 4. Ejecutar la versión Standalone directamente
./SubdrumProcessor_artefacts/Release/Standalone/Subdrum\ Processor.app/Contents/MacOS/Subdrum\ Processor
```

### En Windows:
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## ⚡ Garantías de Rendimiento en Tiempo Real
1. **Cero asignaciones dinámicas en el hilo de audio (`processBlock`)**: Todas las estructuras, buffers de sobremuestreo y filtros son pre-asignados en `prepareToPlay()`.
2. **Parámetros libres de bloqueos (*Lock-Free*)**: Lectura directa de parámetros mediante `std::atomic<float>` y suavizado temporal con `juce::SmoothedValue`.
3. **Generador pseudoaleatorio ultrarrápido**: Algoritmo `Xorshift32` para generar ruido y polvo de vinilo sin invocar mutexes ni llamadas lentas del sistema operativo.
