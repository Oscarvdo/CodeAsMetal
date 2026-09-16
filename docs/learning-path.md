# Cómo estudiar esta V1 sin perderte

Esta entrega contiene el conjunto de archivos solicitado; para aprender no necesitas leerlos todos a la vez.

## 1. Del template Win32 a la aplicación

Compara `legacy/Win32Template/CodeAsMetal.cpp` con `CodeAsMetal/CodeAsMetal.cpp`.
En Win32 registrabas una clase, creabas HWND y procesabas mensajes. QApplication realiza
la integración con el sistema de ventanas; MainWindow construye los widgets. Tu programa
sigue siendo C++ nativo. CMake no reemplaza C++; describe cómo compilar y enlazar cada módulo.

Objetivo verificable: abrir `.slnx`, compilar y ver la ventana. No agregar SQL antes de esto.

## 2. Un valor que falta no es cero

En Domain.h busca `std::optional<double> stockMassKg`. `nullopt` significa que no sabemos
la masa; `0.0` significa que alguien la configuró. Lee `MissingRateDoesNotBecomeZero`.
Compila las pruebas y cambia un input sintético: observa qué categoría deja de conocerse.

Objetivo: explicar con tus palabras por qué un costo parcial no puede mostrarse como total.

## 3. Ingeniería primero, costo después

Sigue Operation → CostInput → estimate(). Una operación tiene tiempo/parte y setup/lote.
Para 10 piezas con 6 minutos de máquina por pieza: 60 minutos. Un setup de 30 minutos
sigue siendo 30 minutos, no 300. El test `SetupNotMultipliedByBatchSize` protege esa diferencia.

## 4. Del STEP al B-Rep

En Cad.cpp observa ReadFile → TransferRoots → OneShape → topología y propiedades.
Una cara cilíndrica puede ser un agujero o un boss. `cavity` prueba el estado de puntos,
pero todavía exige revisión de los extremos y conexiones. Esa incertidumbre se conserva.
No comiences por añadir más patrones de diseño: primero verifica el STEP de bloque conocido.

## 5. El visor y la identidad

Viewer mantiene context/view/AIS_Shape. La geometría CAD vive en objetos OCCT con Handles.
La selección convierte una cara OCCT en un índice del mapa de esa revisión. El SHA-256 y
versión del kernel impiden tratar esa etiqueta como identidad universal entre revisiones.

## 6. Documento y persistencia

Document convierte tipos de dominio a JSON. SQLRepository envía parámetros; no concatena
valores del usuario en SQL. El JSON puede persistirse de manera atómica localmente o como
unidad de concurrencia en SQL. Compara datos de dos procesos para entender rowversion.

## 7. Override y evidencia

Edita una operación y observa en el archivo el tiempo efectivo, recommendation, actor,
reason y UTC; revisa también audit. Emite estimate y cambia tarifa: el snapshot no cambia.
La meta es explicar cada cifra, no presentar precisión aparente sin entradas confiables.
