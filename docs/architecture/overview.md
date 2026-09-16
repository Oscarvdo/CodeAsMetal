# Arquitectura y límites

Modular monolith desktop. El dominio no incluye Qt ni OpenCASCADE ni SQL.

| Target/carpeta | Responsabilidad | Depende de |
|---|---|---|
| cam_core | Datos de ingeniería, validación, DFM, plan preliminar, costo | Standard Library |
| cam_cad | Importación, medidas, superficie B-Rep, candidatos, SHA-256 | cam_core, OCCT, Qt Core |
| cam_persistence | Serialización versionada, validación de documentos, archivos, SQL | cam_core, Qt Core/SQL |
| CodeAsMetal | MainWindow, formularios, navegación, visor y coordinación | Targets anteriores + Qt Widgets |
| Tests | Invariantes de negocio y adaptadores | cam_core o adaptadores según suite |

El proyecto VS original es la puerta de entrada. CMake es la única definición del grafo de build.
Ninja compila con el compilador MSVC seleccionado por VsDevCmd. Qt no necesita moc personalizado:
se usan señales Qt existentes y lambdas; ninguna clase propia utiliza Q_OBJECT.

Product truth: CAD hash + definición material/dibujo + revisión. Manufacturing knowledge: plan,
operaciones, referencias de recursos. Business data: tarifas efectivas e inputs económicos.
Engineering intelligence: medidas, candidatos, findings y cálculo. Son estructuras distintas,
aunque el documento agrega sus snapshots para persistencia y transporte local.

## Importación y threading

QtConcurrent ejecuta importCad fuera del UI thread. Un mutex serializa traductores OCCT por
su configuración global de unidades. El worker devuelve datos y shape; no toca widgets/AIS.
La UI acepta la revisión solamente al finalizar con éxito. Eventos del visor viven en el UI thread.
Cerrar mientras importa se bloquea para no destruir objetos utilizados por el worker.
SQL y exportación se ejecutan en el UI thread en esta preview: una red lenta puede bloquear
interacción hasta el timeout ODBC. Separar repositorio por thread requiere conexiones propias.

## Revision identity

Una revisión = UUID + nombre + CAD path + SHA-256 + versión del kernel y del engine.
Una face ID es local al resultado de importación. Mismo hash con distinto kernel requiere
nueva revisión para evitar reutilizar IDs potencialmente distintos. No se mueven issues
entre revisiones automáticamente. Las features pueden agrupar varias caras por decisión humana.

## Estado de aplicación

Documentos guardados con QSaveFile (reemplazo atómico). La recuperación usa el mismo esquema.
El repositorio SQL guarda una unidad agregada por proyecto con control optimista. Esto evita
mezclar una geometría nueva con operaciones/resultados parcialmente persistidos.
La edición hace validación antes de reemplazar una colección. El audit registra actor, UTC,
acción y payload antes/después donde existe override. Los estimates incorporan todos sus inputs.

## Cost profiles

V1 tiene un perfil por proyecto: moneda, fecha, cantidad, grado de material, stock comprado,
tarifas y costos adicionales. La UI calcula el escenario actual para cada revisión.
Comparación histórica usa snapshots emitidos; puede comparar condiciones comerciales diferentes.
Para comparar solo geometría, emitir escenarios con los mismos inputs y explicitar esa decisión.
