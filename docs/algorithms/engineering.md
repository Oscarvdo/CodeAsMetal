# Algoritmos, unidades y procedencia

## Geometry / cad-1.0

STEPControl_Reader transfiere raíces a B-Rep en milímetros (`xstep.cascade.unit=MM`).
STL se convierte en triángulos OCCT y exige declarar escala hacia mm; nunca habilita features.
Los conteos usan TopTools_IndexedMapOfShape: cuentan entidades únicas, no apariciones repetidas.
BRepBndLib produce caja alineada a ejes; puede incluir tolerancias geométricas y no es una caja
mínima orientada. BRepGProp::SurfaceProperties produce área y centro superficial.
Para exactamente un sólido válido que contiene todas las caras del import se habilita
VolumeProperties(OnlyClosed=true), volumen positivo y centro volumétrico. No se suman
volúmenes de ensamblajes como si fueran una única pieza. No hay reparación silenciosa.

Procedencia de esas cifras: MEASURED (CAD), engine cad-1.0, kernel registrado, SHA-256 y UTC de revisión.
El algoritmo no certifica la validez manufacturera de un B-Rep que pase BRepCheck_Analyzer.

## Cavity candidates

Solo una superficie analítica cilíndrica con apertura paramétrica 2π y extensión finita
se evalúa. En la sección media, la línea de eje debe caer fuera del sólido y un punto
1% del radio más allá de la pared (mínimo 0.0001 mm) dentro del material.
Así se descartan bosses exteriores comunes. Diámetro = 2r; profundidad = |vmax−vmin|.
El eje proviene de la superficie OCCT, no de ejes globales supuestos.
Esto NO prueba agujero pasante/ciego, no une caras cilíndricas divididas, no resuelve
intersecciones y puede perder paredes muy finas. Se devuelve Hole candidate para revisión.
Planes, cilindros, conos y toros son clasificaciones geométricas, no features funcionales.

## DFM / rule set configurable

- DRILL-HOLE-004: profundidad/diámetro > máximo configurado.
- MILL-WALL-001: espesor medido por ingeniero < mínimo configurado.
- MILL-RADIUS-001: radio interno suministrado > 0 y < mínimo configurado.

En la igualdad no hay finding. Ratio exige diámetro positivo. Umbrales positivos y finitos.
Una candidata produce severidad Review; una feature confirmada produce Warning.
Los valores iniciales 5, 1 mm y 1 mm son ejemplos editables, NO límites de ninguna norma.
Los findings registran medida, threshold, regla/version, feature y explicación; el documento
retiene el ruleset utilizado. Cambiar configuración requiere versión distinta.

## Cost / cost-1.0

Una moneda por estimate, sin conversiones FX, impuestos ni descuento financiero.
Q es cantidad de piezas. t es minutos/parte; s es minutos/lote. Tarifas/hora.

- Material = masa de STOCK comprado kg/parte × precio/kg × Q.
- Machine = Σ(t_i / 60 × tarifa_máquina_i × Q), operaciones no Inspection.
- Labor = Σ(t_i / 60 × tarifa_operador × Q), asistencia asumida 100%.
- Setup = Σ(s_i / 60 × (tarifa_máquina_i + tarifa_operador)).
- Tooling = costo_herramienta/parte × Q.
- Inspection = Σ(t_i / 60 × tarifa_inspección × Q), operaciones Inspection.
- Logistics = costo configurado del lote.
- Overhead = porcentaje × subtotal de las siete categorías anteriores.

La tarifa máquina debe EXCLUIR mano de obra y overhead para evitar duplicación.
La tarifa de inspección es un cargo combinado; no se suma labor separada en esas operaciones.
El setup solo se cobra en las operaciones que lo contienen: otras del mismo setup deben
registrar 0 explícito. No se infiere tiempo de corte desde volumen CAD ni MRR inventado.
Stock kg incluye material comprado que se convertirá en viruta. No hay cálculo de scrap/rework.
La UI muestra dos decimales; internamente se usa double con finitud validada. No es un ledger
contable, no incluye reglas monetarias por moneda ni precisión decimal contractual.

## Missingness and confidence

null/optional significa desconocido; cero significa cero configurado. Si falta un input,
la categoría completa queda desconocida. El subtotal conocido es conservador y NO un total final.
Overhead exige todos los costos directos; no se calcula sobre subtotal incompleto.
Completitud = 100 × categorías conocidas / 8; cada categoría pesa igual. No es cobertura
ponderada por dólares. No existe un valor numérico de confianza calibrado en esta versión.
Snapshot registra inputs y resultados inmutables; tarifas futuras no provocan recálculo.
