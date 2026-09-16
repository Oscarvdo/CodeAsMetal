# Architecture Decision Records

## ADR-001 — STEP principal y STL limitado

Estado: implementado en fuente. STEP conserva superficies y topología; STL requiere unidades
explícitas y no alimenta feature recognition ni volumen exacto. No inferir ingeniería de triángulos
como si fueran caras CAD. Consecuencia: piezas STL siguen siendo visualizables y medibles de forma básica.

## ADR-002 — Mantener la solución del usuario y delegar build a CMake

Estado: implementado, validación VS2026 pendiente. Conservar nombre/GUID/resources facilita transición.
El vcxproj Makefile llama el mismo grafo usado por CLI/CI. Qt/OCCT no se enlazan con listas duplicadas
por configuración del IDE. El Win32 educativo queda como referencia separada. Solo x64 en V1.

## ADR-003 — Dominio independiente

Estado: probado. C++ Standard Library para DFM/costo. Qt/OCCT/SQL se quedan en adaptadores.
Permite probar invariantes financieras y de ingeniería sin ventana ni servidor. No se crean
interfaces/repositorios abstractos adicionales mientras hay una sola implementación útil.

## ADR-004 — SQL Server por agregado con history e estimates separados

Estado: fuente SQL y adapter, pruebas servidor pendientes. Prioridad: consistencia del proyecto,
optimistic concurrency y snapshots. Persistir agregado JSON es un compromiso visible de esta
preview; todavía no satisface catálogos empresariales relacionales completos. No es SaaS.

## ADR-005 — Candidatos geométricos y confirmación humana

Estado: implementado en fuente, CAD tests pendientes. No etiquetar superficies como features
sin evidencia suficiente. El reconocimiento general es una tarea separada del formulario manual.
No hay tiempos automáticos basados en MRR genérico. Se requieren tiempos justificados por ingeniero.

## ADR-006 — Snapshots y effective dating

Estado: motor probado, almacenamiento servidor pendiente. Tarifas por [from,to), sin solapamiento.
Nueva tarifa cierra intervalo previo conservando valor/ID/origen. Snapshot incluye entradas,
reglas, geometría, plan y resultado. Se bloquea edición/eliminación por procedimientos SQL.

## ADR-007 — Lenguaje C++23 sin dependencias artificiales

Estado: núcleo compilado en C++23. Se utiliza C++ moderno (RAII, optional, tipos de valor,
const-correctness, callbacks controlados); no se fuerza std::expected/modules si no resuelve
un problema presente. Versionar build/dependencias y documentar unidades importa más que keywords.
