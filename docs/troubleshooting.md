# Solución de problemas

| Síntoma | Revisión concreta |
|---|---|
| VCPKG_ROOT missing | Ejecutar prepare-dependencies y reiniciar VS. Ver `$env:VCPKG_ROOT` en una consola nueva |
| No se encuentra v145 | Modificar VS2026 Installer, agregar MSVC C++ x64/x86 y Windows SDK |
| No se encuentra cmake/ninja | Agregar C++ CMake tools for Windows; build.cmd usa los incluidos con VS |
| Qt6Config/OpenCASCADE no encontrado | Revisar instalación x64-windows dentro de VCPKG_ROOT. No mezclar Qt descargado con otro compiler ABI |
| OCCT 8.x rechazado | Usar registro 2025.12.12 o un commit 7.x revisado. Migración API 8.x no incluida |
| DLL no encontrada | Ejecutar build.cmd que despliega DLL/plugins; verificar no usar Release con Qt debug |
| Qt platform plugin no inicializa | Debe existir platforms/qwindows.dll junto a EXE en Release, qwindowsd.dll en Debug |
| QODBC no disponible | Instalar qtbase sql-odbc; verificar carpeta sqldrivers junto a EXE |
| ODBC driver not found | Instalar Microsoft ODBC Driver 18 x64; plugin Qt y driver Microsoft son piezas distintas |
| TLS certificate error | Configurar certificado SQL confiable. No desactivar validación para una red empresarial |
| Concurrency conflict | Conservar recuperación, abrir de nuevo proyecto del servidor y reaplicar cambios revisados |
| Hash mismatch | El archivo cambió; importarlo como otra revisión. Para relocalizar debe ser byte-idéntico |
| Kernel version changed | Volver a importar como revisión nueva. No reciclar anotaciones con nuevos índices OCCT |
| STL tamaño incorrecto | Reimportar seleccionando unidades de origen; STL no las declara |
| Costo incompleto | Revisar mass stock, rates con fecha/moneda, tiempos de ciclo/setup, inspección, tooling, logística, overhead |
| F5 sin CAD o negro | Revisar driver OpenGL; probar ejecución local sin sesión remota y log de AppLocalData |

Si una dependencia no compila, conserva `dependencies.lock.json`, el log vcpkg y el primer
error real de compiler/linker. No reemplaces el código aleatoriamente ni desactives pruebas.

## Empaquetado

Después del build y aceptación Windows:

```powershell
.\scripts\package.ps1
# Opcional, con NSIS 3 instalado:
.\scripts\package.ps1 -Installer
```

El paquete no instala SQL Server. Instalar runtime VC++ y ODBC18 mediante sus instaladores
o distribución corporativa. NSIS genera un instalador por usuario, sin firma digital.
Los paquetes comerciales deben acompañar notices/licencias de bibliotecas utilizadas.
No se incluye un binario preconstruido porque no fue compilado/verificado en Windows aquí.
