# Alcance verificable y criterios de aceptación

## Estado del código frente al roadmap original

| Milestone | Entrega | Cierre pendiente |
|---|---|---|
| M0 | C++23, puente VS2026, CMake, core tests | Build Windows y lock real de dependencias |
| M1 | STEP, OCCT, hash, topología | Suite de importación ejecutada con CAD real del usuario |
| M2 | Visor AIS y controles | GPU, HiDPI, selección Windows probados |
| M3 | Caja, área, volumen, centro y clasificación | Herramienta interactiva de distancia/ángulo no incluida |
| M4 | Superficies y candidatos cilíndricos + clasificación manual | Reconocimiento automático general de pockets/slots/chamfers/fillets/walls y extremos de agujeros |
| M5 | Tres reglas configurables con evidencia | Biblioteca de reglas validada con ingeniería de manufactura |
| M6 | Proyecto/parte/revisiones, archivos, SQL agregado | Árbol proyecto→varias piezas y catálogos normalizados independientes |
| M7 | Propuesta preliminar y operaciones editables | Factibilidad, tool access, selección automática de máquina/tool/setup |
| M8 | Desglose, faltantes, historial, snapshots | Modelo de tiempos por parámetros de corte y confianza calibrada |
| M9 | Issues/estados/decisiones/asignación ligados a feature | Workflow formal de aprobaciones y permisos por rol |
| M10 | Historial y comparación de métricas | Matching topológico persistente entre revisiones |
| M11 | STL, reportes, autosave, logging, CI y packaging source | Pruebas de carga, benchmark, instalador generado/firmado y QA de PDF |

Un ZIP con código no equivale a una release validada. Se denomina preview.1 hasta pasar los gates.

## Acceptance manual Windows

- [ ] Instalar dependencias en máquina limpia; registrar versiones exactas.
- [ ] Abrir `.slnx`; compilar Debug y Release con VS2026/v145 x64.
- [ ] Ejecutar `ctest --preset windows-debug` y `windows-release`.
- [ ] Generar STEP de bloque: volumen 120000, área 18400, seis caras, centro (50,30,10).
- [ ] Generar STEP con agujero pasante y ciego: volumen 120000 − π·480 mm³.
- [ ] Probar archivo STEP en pulgadas; verificar conversión mm contra modelo conocido.
- [ ] Archivo inválido no sustituye la revisión actual ni cierra la aplicación.
- [ ] Rotar/pan/zoom, fit, seleccionar cara/arista/sólido, HiDPI 100/150/200%.
- [ ] Abrir ruta con espacios y caracteres españoles; comprobar hashing/importación.
- [ ] Confirmar un hole candidate; cambiar relación límite produce finding explicable.
- [ ] Cambiar una regla conserva versiones y resultados de estimates ya emitidos.
- [ ] Editar, quitar/reordenar operaciones; revisar el audit antes/después del JSON.
- [ ] Datos sintéticos de `CoreCases.h` producen 324.50/lote, 32.45/pieza para cantidad 10.
- [ ] Eliminar precio del material muestra MISSING y evita total completo.
- [ ] Cambiar precios futuros no cambia ningún snapshot histórico.
- [ ] Cambiar físicamente CAD/PDF bloquea emisión hasta revisión/importación apropiada.
- [ ] Guardar, cerrar, abrir, recuperar autosave. Cancelar Save no pierde cambios.
- [ ] SQL: dos instancias abren mismo proyecto; primera guarda; segunda recibe conflicto.
- [ ] SQL: usuario cam_app no puede UPDATE/DELETE de Estimates ni ProjectHistory.
- [ ] SQL: documento que elimina o modifica estimate previo es rechazado.
- [ ] Exportar HTML/PDF y revisar todas las páginas y textos largos.
- [ ] Paquete abre en máquina limpia con VC++ runtime, ODBC18 y sin Visual Studio.

## Restricciones operativas actuales

Importación CAD corre en worker pero no ofrece cancelación durante un algoritmo OCCT.
El diálogo es modal y no permite cambiar de proyecto mientras importa. Archivos muy grandes
pueden usar mucha memoria; todavía no hay cuota de triángulos/caras ni aislamiento de proceso.
Las tarifas son por proyecto, no un maestro empresarial compartido entre proyectos.
El log contiene diagnósticos Qt; no es un sistema de telemetría. Los autosaves son locales.
El autor de un cambio local es una atribución declarada; la identidad autenticada del guardado
SQL es ORIGINAL_LOGIN(). No hay firmas digitales ni acreditación de aprobación profesional.
