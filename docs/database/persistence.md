# Persistencia SQL Server y concurrencia

## Esquema físico V1

- cam.Projects: identidad, nombre, documento JSON, último actor/UTC y rowversion.
- cam.ProjectHistory: snapshot de cada guardado, insert-only para la aplicación.
- cam.Estimates: snapshot de emisión, UUID global, project/revision UUID, actor SQL/UTC.

El documento contiene revisiones, features, findings, operaciones, issues, tarifas y auditoría.
Es una decisión explícita de persistencia por agregado, no la base relacional normalizada
completa propuesta en el roadmap. Permite transacción consistente del flujo integrado,
a costa de menor granularidad concurrente y consultas/reporting relacional menos cómodos.
Futura extracción: Materials/Grades, Suppliers, Machines/Tools, Rates y Plans normalizados,
con migraciones y sin cambiar el comportamiento del dominio. No se incluyen tablas vacías
que den la impresión de funcionalidad ya integrada.

## Compare-and-swap

ProjectLoad retorna documento y binary(8) rowversion. ProjectSave abre transacción y toma
UPDLOCK,HOLDLOCK sobre la clave. Un proyecto nuevo acepta token NULL; un existente exige
exactamente su token actual. La consulta de resultado se devuelve DESPUÉS del COMMIT.
Un conflicto lanza 51004; no se sobrescribe ni se reintenta con token eliminado.
La UI guarda una copia de recuperación antes del intento. No hay merge ni last-writer-wins.
El nivel de conflicto es proyecto entero, incluso si dos ingenieros editaron revisiones distintas.

## Snapshots

La UI no proporciona editar/eliminar estimates emitidos. SQL verifica que todos los estimates
previos sigan presentes y que no cambien revisión ni JSON. Inserta solo IDs nuevos.
El rol cam_app tiene EXECUTE de procedimientos y DENY de INSERT/UPDATE/DELETE directo.
Ownership chaining permite a los procedimientos efectuar escrituras autorizadas.
Administradores db_owner pueden alterar datos; no se pretende inmutabilidad frente al DBA.
Los JSON locales son archivos editables y no constituyen un archivo regulado inviolable.

## Usuarios

Una organización por base. Todos los usuarios de cam_app pueden listar y abrir sus proyectos.
No hay ACL por proyecto ni roles de aprobador diferentes. El actor local es declarativo;
ORIGINAL_LOGIN() en SQL registra quién autenticó el guardado. No guardar passwords en archivos.

## Operación

Backups/restores de SQL corresponden al DBA. Respaldar también carpeta CAD/PDF: sus bytes no
están en SQL. Usar rutas UNC y permisos de lectura/escritura apropiados. Si un archivo cambia,
importar una revisión nueva; jamás trasladar automáticamente las anotaciones por número de cara.
Retención/purga de ProjectHistory no está implementada y debe diseñarse antes de cargas grandes.
Compatibilidad SQL 150+, JSON/OPENJSON habilitado. Script de esquema idempotente para objetos
creados; no es migrador general de esquemas arbitrarios ya existentes.
