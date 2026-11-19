// Genera la guía de exposición en TXT (UTF-8 con BOM para que el Bloc de notas muestre bien las tildes).
const fs = require('fs');
const G = require('./contenido');

const ANCHO = 92;
function envolver(texto, sangria = '') {
  const out = [];
  for (const parrafo of texto.split('\n')) {
    let linea = sangria;
    for (const palabra of parrafo.split(' ')) {
      if ((linea + palabra).length > ANCHO && linea.trim()) { out.push(linea.replace(/\s+$/, '')); linea = sangria; }
      linea += palabra + ' ';
    }
    out.push(linea.replace(/\s+$/, ''));
  }
  return out.join('\n');
}
const seg = t => { const [m, s] = t.split(':').map(Number); return m * 60 + s; };
const mmss = n => `${Math.floor(n / 60)}:${String(n % 60).padStart(2, '0')}`;
const RAYA = '='.repeat(ANCHO);

const total = G.reduce((a, s) => a + seg(s.tiempo), 0);
const L = [];
L.push(RAYA, 'EXPOSICIÓN · SIMULACIÓN PARALELA DE LA RESPUESTA SÍSMICA', RAYA,
  'Curso:      Programación Concurrente y Paralela · Proyecto 2 · UNMSM-FISI · 2026-2',
  'Docente:    [nombre del docente]',
  'Equipo:     Maylle Poma, Fran Diego · [integrantes]',
  `Archivos:   Simulacion_Sismica_Exposicion.pptx (${G.length} láminas; esta guía resumida va en las notas del orador)`,
  '            Simulacion_Sismica_Exposicion.pdf  (respaldo por si la PC del aula no abre el PPTX)',
  '', `Duración estimada: ${mmss(total)} min (con ritmo pausado). Si te dan menos tiempo, mira el punto 5.`, '',
  RAYA, 'CÓMO USAR ESTA GUÍA', RAYA,
  envolver('1. No es un guion para memorizar. Para cada lámina tienes: QUÉ HAY en la lámina (cada recuadro, número, dibujo o línea de código, en el orden en que conviene señalarlo) y CÓMO EXPLICARLO. Tú lo dices con tus palabras.'),
  envolver('2. La "frase de ejemplo" es solo un apoyo por si te quedas en blanco. El "paso a la siguiente" te ayuda a enlazar las láminas sin silencios. "Si te preguntan" tiene las preguntas más probables del docente con su respuesta.'),
  envolver('3. Señala cada elemento mientras lo explicas. En las láminas de código, recórrelo con los círculos numerados (1, 2, 3...), nunca línea por línea: el código está simplificado para leerse en la exposición; el real está en el repositorio.'),
  envolver('4. En PowerPoint usa la Vista del moderador (Alt+F5): verás un resumen de esta guía en las notas, debajo de cada lámina.'),
  envolver('5. Si te dan solo 10 a 12 minutos: en la lámina 4 di solo los nombres de las fases y "estamos aquí"; en la 7 explica solo el código (salta el enchufe); en la 10 explica el diagrama y salta la tabla; en la 11 explica solo denso, "solo K·U" contra "todo el paso" y el recuadro amarillo. Ahorras unos 5 minutos.'),
  envolver('6. Los números de la lámina 11 son mediciones reales hechas en la PC DESKTOP-R3PLOD9 (i5-11400F) el 14-10-2026, con el protocolo del proyecto (mediana de 5). Si el equipo vuelve a medir en la máquina oficial y salen distintos, se cambian en _generador_expo/build.js (objeto RES) y se regenera, o se editan a mano en PowerPoint. El dibujo del oscilador de la lámina 5 es ilustrativo; la figura real es results/figuras/01_validacion_1gdl.png.'),
  envolver('7. Antes de exponer, reemplaza en la portada "[nombre del docente]" e "[integrantes]".'),
  '', RAYA, 'GUÍA LÁMINA POR LÁMINA', RAYA);

let acum = 0;
for (const s of G) {
  acum += seg(s.tiempo);
  L.push('', `--- LÁMINA ${s.n} de ${G.length} · ${s.titulo} ---`, `Tiempo: ${s.tiempo} min  ·  acumulado: ${mmss(acum)}`, '');
  L.push(envolver('OBJETIVO: ' + s.objetivo), '', 'RECORRIDO (en este orden):');
  s.elementos.forEach(([que, como], i) => {
    L.push(envolver(`${i + 1}) ${que}`, ''));
    L.push(envolver('→ ' + como, '     '));
  });
  L.push('', 'FRASE DE EJEMPLO (si te quedas en blanco):', envolver('"' + s.ejemplo + '"', '   '));
  L.push('', 'PASO A LA SIGUIENTE:', envolver(s.transicion, '   '));
  if (s.preguntas.length) {
    L.push('', 'SI TE PREGUNTAN:');
    s.preguntas.forEach(([p, r]) => { L.push(envolver('P: ' + p, '   ')); L.push(envolver('R: ' + r, '   ')); });
  }
}

const GLOSARIO = [
  ['Matriz de rigidez K', 'Dice cuánta fuerza aparece en cada nodo cuando se desplazan él y sus vecinos. Es simétrica y casi toda ceros.'],
  ['Matriz U', 'Desplazamientos de todos los nodos; una columna por cada sismo (escenario).'],
  ['GDL (grado de libertad)', 'Cada movimiento independiente del modelo. Aquí, uno por nodo: su desplazamiento horizontal.'],
  ['Deriva', 'Desplazamiento de un piso menos el del piso de abajo, entre la altura del piso.'],
  ['Resonancia', 'Cuando el sismo vibra a la misma frecuencia que el edificio, la respuesta se amplifica mucho.'],
  ['Diferencias centrales', 'Método explícito para avanzar en el tiempo: el siguiente instante sale de los dos anteriores. Exige un paso pequeño (Δt < 2/ω_max).'],
  ['Matriz dispersa / CSR', 'Matriz casi toda de ceros. CSR (Compressed Sparse Row) guarda solo los no nulos en tres arreglos: val, col_idx y row_ptr.'],
  ['SpMM', 'Sparse Matrix × Multiple vectors: matriz dispersa por varios vectores a la vez (nuestro K·U).'],
  ['Caché', 'Memoria pequeña y muy rápida dentro del procesador. Recorrer los datos en orden la aprovecha.'],
  ['Hilo', 'Una secuencia de ejecución dentro de un programa. Varios hilos comparten la misma memoria.'],
  ['OpenMP', 'Estándar para paralelizar en C/C++ con directivas (#pragma). El compilador crea los hilos y reparte el trabajo.'],
  ['#pragma omp parallel for', 'Reparte las iteraciones de un for entre los hilos. schedule(static) = trozos iguales y contiguos.'],
  ['Barrera', 'Punto donde cada hilo espera a que todos lleguen antes de seguir. Al final de cada "omp for" hay una implícita.'],
  ['Condición de carrera', 'Dos hilos escriben el mismo dato a la vez y el resultado depende de quién llega primero. Se evita si cada hilo escribe su propia parte.'],
  ['Goroutine', 'Tarea liviana de Go; el runtime la reparte entre hilos del sistema operativo.'],
  ['Canal (chan)', 'Cola segura para pasar datos entre goroutines.'],
  ['Worker pool', 'Grupo fijo de trabajadores (goroutines) que toman tareas de una cola común hasta que se acaban.'],
  ['sync.WaitGroup', 'Contador de Go para esperar a que un grupo de goroutines termine (Add, Done, Wait).'],
  ['T(1)', 'Tiempo de la MEJOR versión secuencial. Es la referencia del speedup.'],
  ['Speedup S(p)', 'T(1) / T(p): cuántas veces más rápido con p hilos. Lo ideal es S(p) = p.'],
  ['Eficiencia E(p)', 'S(p) / p: qué fracción del ideal se logra. 1 = perfecto.'],
  ['Ley de Amdahl', 'Si una fracción f del programa se paraleliza, el speedup máximo es 1 / ((1 − f) + f/p), y nunca pasa de 1 / (1 − f).'],
  ['Granularidad', 'Cuánto trabajo hay en cada tarea paralela. Si es muy poco, coordinar los hilos cuesta más de lo que se gana.'],
  ['Hyperthreading', 'Dos hilos lógicos por núcleo físico. El i5-11400F tiene 6 núcleos y 12 hilos lógicos.'],
  ['Cannon y SUMMA', 'Algoritmos de multiplicación de matrices con MPI que se implementan en S9–S10.'],
];
L.push('', RAYA, 'GLOSARIO RÁPIDO', RAYA);
GLOSARIO.forEach(([t, d]) => L.push(envolver(`• ${t}: ${d}`, '')));

L.push('', RAYA, 'CHULETA DE CIFRAS', RAYA,
  '• 12 000 productos K·U = 60 s de registro ÷ Δt de 0,005 s (cronograma del proyecto).',
  '• Edificio de 100 × 400: 40 100 nodos; K tiene 1 600 millones de casillas y 199 498 no nulos.',
  '• Fase 1: 70 pruebas (47 producto + 23 simulador); periodo medido = 2π√(m/k) con error 1e-5;',
  '  convergencia 4,00 (2.º orden); edificio 10 × 4: T1 = 1,005 s; a 1 Hz el techo se mueve',
  '  88,6 cm contra 14,7 cm a 0,5 Hz (6×).',
  '• Línea base (n = 1024, 1 hilo): naive 1,487 s · ikj 0,474 s · bloques 64 0,445 s → ~3×.',
  '• Simulador secuencial: K·U ≈ 47 % del tiempo → Amdahl ≤ 1 / 0,53 ≈ 1,9×.',
  '• S7 (DESKTOP-R3PLOD9, mediana de 5), speedup con 2 / 4 / 8 hilos:',
  '    denso n = 1024, OpenMP bloques ....... 1,97 / 3,38 / 4,71  (E(8) = 0,59)',
  '    denso n = 2048, OpenMP bloques ....... 1,88 / 3,38 / 3,73',
  '    K·U malla 512² (262 144 nodos) ....... 1,78 / 1,89 / 2,38',
  '    simulador 100 × 400, solo K·U ........ 1,27 / 1,35 / 1,31  (Amdahl: 1,30 / 1,54 / 1,69)',
  '    simulador 100 × 400, todo el paso .... 1,66 / 2,36 / 2,54',
  '    simulador 20 × 50 (chico) ............ 0,83 / 0,87 / 0,59  (no conviene: granularidad)',
  '    Go denso n = 1024 .................... 1,92 / 3,87 / 5,67  (T(1) Go 0,514 s vs C 0,445 s)',
  '    Go K·U malla 512² .................... 1,89 / 3,05 / 3,20',
  '• Pruebas S7: 51 de OpenMP (p = 1, 2, 3, 4 y 8; idéntico bit a bit) + las de Go (go test).',
  '• Hallazgo T(1): spmm_csr_omp con 1 hilo era ~1,3× más rápido que spmm_csr (limpiaba cada fila',
  '  en caché); se agregó spmm_csr_rowinit al secuencial y T(1) es la más rápida de las dos.');

L.push('', RAYA, 'FUENTES Y DÓNDE ESTÁ CADA COSA', RAYA,
  '• Código y documentación: carpeta matmul-paralelo (README.md, docs/HITO_H1.md, docs/GUIA_S7.md).',
  '• Cronograma: Cronograma_Simulacion_Sismica_S5-S16 (1).pptx.',
  '• H. Huang y E. Chow, "Exploring the Design Space of Distributed Parallel Sparse Matrix-Multiple',
  '  Vector Multiplication", IEEE TPDS, vol. 35, n.º 11, pp. 1977-1988, 2024.',
  '  DOI 10.1109/TPDS.2024.3452478',
  '• J.-N. Quintin, K. Hasanov y A. Lastovetsky, "Hierarchical Parallel Matrix Multiplication on',
  '  Large-Scale Distributed Memory Platforms", ICPP 2013, pp. 754-762. arXiv:1306.4161',
  '• T. Herault, Y. Robert, G. Bosilca y J. Dongarra, "Generic Matrix Multiplication for Multi-GPU',
  '  Accelerated Distributed-Memory Platforms over PaRSEC", ScalA 2019, pp. 33-41.',
  '  DOI 10.1109/ScalA49573.2019.00010');

fs.writeFileSync(process.argv[2] || 'guia_exposicion.txt', '﻿' + L.join('\r\n') + '\r\n', 'utf8');
console.log('ok', L.length, 'líneas', mmss(total));
