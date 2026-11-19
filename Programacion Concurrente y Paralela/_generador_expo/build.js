// Presentación "Simulación paralela de la respuesta sísmica" (13 láminas, 16:9 ancho).
// Misma estructura que la de Six Sigma: portada oscura, láminas claras con
// rótulo + título, tarjetas, íconos en círculos y notas del orador.
//
// Los gráficos están dibujados con formas (no son gráficos nativos) para que
// no se rompan al importar el PPTX en Canva. Los números salen de RES (abajo):
// si cambian las mediciones, se editan ahí y se vuelve a generar.
const pptxgen = require('pptxgenjs');
const React = require('react');
const ReactDOMServer = require('react-dom/server');
const sharp = require('sharp');
const fa = require('react-icons/fa');
const { applyTheme } = require(process.env.APPLY_THEME);
const GUIA = require('./contenido');

// ---- Mediciones (DESKTOP-R3PLOD9, 2026-10-14, mediana de 5; docs/GUIA_S7.md §9) ----
const RES = {
  denso:     { nombre: 'Producto denso\nn = 1024', s: [1.97, 3.38, 4.71] },    // OpenMP bloques 64
  ku:        { nombre: 'K·U\n262 144 nodos',       s: [1.78, 1.89, 2.38] },    // spmm_csr_omp, malla 512²
  simKU:     { nombre: 'Simulador:\nsolo K·U',     s: [1.27, 1.35, 1.31] },    // 100 × 400
  simTodo:   { nombre: 'Simulador:\ntodo el paso', s: [1.66, 2.36, 2.54] },    // 100 × 400
  goDenso:   { nombre: 'Go: denso\nn = 1024',      s: [1.92, 3.87, 5.67] },
  amdahl8: 1.69,   // límite de Amdahl con 8 hilos y f = 46 %
  eDenso8: 0.59,
};

const THEME = {
  name: 'Simulacion Sismica UNMSM',
  headFontFace: 'Calibri',
  bodyFontFace: 'Calibri',
  colors: {
    dk1: '1B2433', lt1: 'FFFFFF', dk2: '13233F', lt2: 'EEF2F7',
    accent1: '2F6DB5', accent2: 'E4572E', accent3: '3A9D5D', accent4: 'F2B134',
    accent5: '159A9C', accent6: '7A5CC0', hlink: '2F6DB5', folHlink: '7A5CC0',
  },
};
const HEX = THEME.colors;
const GRIS = '8A94A3';
const MONO = 'Consolas';

const pres = new pptxgen();
pres.layout = 'LAYOUT_WIDE'; // 13.333 x 7.5 in
pres.author = 'Maylle Poma, Fran Diego';
pres.company = 'UNMSM - FISI';
pres.title = 'Simulación paralela de la respuesta sísmica';
pres.theme = { headFontFace: THEME.headFontFace, bodyFontFace: THEME.bodyFontFace };
const C = pres.SchemeColor;
const HILO = [C.accent1, C.accent5, C.accent4, C.accent6];   // color de cada hilo (0..3)

const FOOT = 'Simulación paralela de la respuesta sísmica   |   Programación Concurrente y Paralela · UNMSM-FISI · 2026-2';

pres.defineSlideMaster({
  title: 'PORTADA',
  background: { color: C.text2 },
  objects: [
    { placeholder: { options: { name: 'title', type: 'title', x: 0.7, y: 1.75, w: 7.7, h: 2.1, fontSize: 40, color: C.background1, valign: 'top', align: 'left', margin: 0 }, text: '' } },
  ],
});
pres.defineSlideMaster({
  title: 'CONTENIDO',
  background: { color: C.background1 },
  margin: [0.5, 0.6, 0.6, 0.6],
  objects: [
    { placeholder: { options: { name: 'kicker', type: 'body', x: 0.6, y: 0.32, w: 9, h: 0.32, fontSize: 12, bold: true, color: C.accent2, charSpacing: 2, margin: 0 }, text: '' } },
    { placeholder: { options: { name: 'title', type: 'title', x: 0.6, y: 0.62, w: 12.1, h: 0.75, fontSize: 30, bold: true, color: C.text2, valign: 'middle', align: 'left', margin: 0 }, text: '' } },
    { text: { text: FOOT, options: { x: 0.6, y: 7.05, w: 10.5, h: 0.3, fontSize: 9, color: C.text1, transparency: 45, margin: 0 } } },
  ],
  slideNumber: { x: 12.25, y: 7.02, w: 0.5, h: 0.3, fontSize: 10, color: C.text1, align: 'right' },
});
pres.defineSlideMaster({
  title: 'FINAL',
  background: { color: C.background1 },
  objects: [
    { placeholder: { options: { name: 'title', type: 'title', x: 0.6, y: 3.0, w: 12.1, h: 1.5, fontSize: 54, bold: true, color: C.text2, valign: 'middle', align: 'center', margin: 0 }, text: '' } },
  ],
});

// ---------- utilidades ----------
async function icono(Comp, hex = 'FFFFFF') {
  const svg = ReactDOMServer.renderToStaticMarkup(React.createElement(Comp, { color: '#' + hex, size: 256 }));
  const buf = await sharp(Buffer.from(svg)).png().toBuffer();
  return 'image/png;base64,' + buf.toString('base64');
}
function circuloIcono(slide, img, x, y, d, color, nombre) {
  slide.addShape(pres.shapes.OVAL, { x, y, w: d, h: d, fill: { color }, line: { color, width: 0 }, objectName: nombre + ' círculo' });
  const p = d * 0.24;
  slide.addImage({ data: img, x: x + p, y: y + p, w: d - 2 * p, h: d - 2 * p, objectName: nombre + ' ícono' });
}
const T = (slide, text, opts) => slide.addText(text, { isTextBox: true, margin: 0, color: C.text1, fontSize: 14, valign: 'top', ...opts });
const tarjeta = (slide, x, y, w, h, nombre, color = C.background2, transparency = 0) =>
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h, rectRadius: 0.12, fill: { color, transparency }, line: { color, width: 0, transparency }, objectName: nombre });
function numero(slide, n, x, y, d, color, nombre) {
  slide.addShape(pres.shapes.OVAL, { x, y, w: d, h: d, fill: { color }, line: { color, width: 0 }, objectName: nombre + ' círculo' });
  T(slide, String(n), { x, y, w: d, h: d, fontSize: d > 0.6 ? 28 : (d < 0.3 ? 11 : 13), bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: nombre + ' número' });
}
function notas(slide, n) {
  const g = GUIA.find(s => s.n === n);
  const partes = [`IDEA: ${g.objetivo}`, ''];
  g.elementos.forEach(([que, como]) => partes.push(`• ${que}: ${como}`));
  partes.push('', `FRASE DE EJEMPLO: ${g.ejemplo}`, `PASO A LA SIGUIENTE: ${g.transicion}`, `Tiempo sugerido: ${g.tiempo} min`);
  slide.addNotes(partes.join('\n'));
}
const ln = (slide, x1, y1, x2, y2, nombre, line) => slide.addShape(pres.shapes.LINE, {
  x: Math.min(x1, x2), y: Math.min(y1, y2), w: Math.abs(x2 - x1), h: Math.abs(y2 - y1),
  flipV: (y2 < y1) !== (x2 < x1), line: { color: C.text2, width: 1.5, ...line }, objectName: nombre,
});

// ---- Bloques de código: tarjeta oscura, Consolas, colores simples de sintaxis ----
const COD = { texto: 'E8EEF6', clave: 'F2B134', pragma: 'FF8A65', coment: '8FA3BF' };
const CLAVES = /\b(?:for|int|double|go|func|defer|range|var|make|chan|close|return)\b/;
const ES_CLAVE = new RegExp('^' + CLAVES.source + '$');
function runsDeLinea(linea) {
  if (/^\s*#pragma/.test(linea)) return [{ text: linea, options: { color: COD.pragma } }];
  const out = [];
  let resto = linea;
  const com = resto.search(/\/\*|\/\//);
  let comentario = '';
  if (com >= 0) { comentario = resto.slice(com); resto = resto.slice(0, com); }
  const partes = resto.split(new RegExp('(' + CLAVES.source + ')'));
  partes.forEach(p => { if (p) out.push({ text: p, options: { color: ES_CLAVE.test(p) ? COD.clave : COD.texto } }); });
  if (comentario) out.push({ text: comentario, options: { color: COD.coment, italic: true } });
  if (!out.length) out.push({ text: ' ', options: { color: COD.texto } });
  return out;
}
// Devuelve la función y(i) con el centro vertical de la línea i (para los marcadores)
function codigo(slide, x, y, w, lineas, { tam = 14, paso = 20, rotulo = '', nombre = 'Código' } = {}) {
  const padX = 0.3, padTop = rotulo ? 0.52 : 0.25, alto = padTop + lineas.length * paso / 72 + 0.22;
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h: alto, rectRadius: 0.1, fill: { color: C.text2 }, line: { color: C.text2, width: 0 }, objectName: nombre + ' fondo' });
  if (rotulo) T(slide, rotulo, { x: x + padX, y: y + 0.16, w: w - 2 * padX, h: 0.26, fontSize: 11, color: COD.coment, fontFace: MONO, objectName: nombre + ' rótulo' });
  const runs = [];
  lineas.forEach((l, i) => {
    const r = runsDeLinea(l);
    if (i < lineas.length - 1) r[r.length - 1].options.breakLine = true;
    runs.push(...r);
  });
  T(slide, runs, { x: x + padX, y: y + padTop, w: w - 2 * padX, h: lineas.length * paso / 72 + 0.05, fontSize: tam, fontFace: MONO, lineSpacing: paso, objectName: nombre });
  return { alto, yLinea: i => y + padTop + (i + 0.5) * paso / 72 };
}
function marcador(slide, n, x, yc, color = C.accent2, nombre = 'Marcador') {
  const d = 0.27;
  numero(slide, n, x, yc - d / 2, d, color, `${nombre} ${n}`);
}

(async () => {
  const ic = {
    globo: await icono(fa.FaGlobeAmericas), edificio: await icono(fa.FaBuilding), chart: await icono(fa.FaChartBar),
    check: await icono(fa.FaCheck), unlock: await icono(fa.FaUnlock), equals: await icono(fa.FaEquals),
    capas: await icono(fa.FaLayerGroup), plug: await icono(fa.FaPlug), idea: await icono(fa.FaLightbulb),
    cohete: await icono(fa.FaRocket), reloj: await icono(fa.FaHourglassHalf, HEX.dk2),
    arrow: await icono(fa.FaArrowRight, HEX.dk2), arrowW: await icono(fa.FaArrowRight, 'FFFFFF'),
    checkV: await icono(fa.FaCheck, HEX.accent3),
  };

  // ================= 1 · PORTADA =================
  {
    pres.addSection({ title: 'Inicio' });
    const s = pres.addSlide({ masterName: 'PORTADA', sectionTitle: 'Inicio' });
    T(s, 'UNIVERSIDAD NACIONAL MAYOR DE SAN MARCOS', { x: 0.7, y: 0.55, w: 7.5, h: 0.3, fontSize: 12, color: C.accent4, charSpacing: 3, objectName: 'Universidad' });
    T(s, 'Facultad de Ingeniería de Sistemas e Informática', { x: 0.7, y: 0.85, w: 7.5, h: 0.3, fontSize: 12, color: C.background1, objectName: 'Facultad' });
    s.addText([
      { text: 'Simulación paralela de la', options: { breakLine: true } },
      { text: 'RESPUESTA SÍSMICA', options: { fontSize: 56, bold: true, color: C.accent4 } },
    ], { placeholder: 'title' });
    T(s, 'Multiplicación de matrices en paralelo con OpenMP, Go, MPI y CUDA', { x: 0.7, y: 3.85, w: 7.7, h: 0.45, fontSize: 18, italic: true, color: C.background1, objectName: 'Lema' });
    T(s, [
      { text: 'Curso: ', options: { color: C.accent4 } }, { text: 'Programación Concurrente y Paralela · Proyecto 2', options: { breakLine: true } },
      { text: 'Docente: ', options: { color: C.accent4 } }, { text: '[nombre del docente]', options: { breakLine: true } },
      { text: 'Equipo: ', options: { color: C.accent4 } }, { text: 'Maylle Poma, Fran Diego · [integrantes]' },
    ], { x: 0.7, y: 5.95, w: 7.5, h: 0.85, fontSize: 14, color: C.background1, objectName: 'Datos del curso' });
    // Emblema K·U
    s.addShape(pres.shapes.OVAL, { x: 8.75, y: 1.35, w: 3.9, h: 3.9, fill: { color: C.accent1, transparency: 80 }, line: { color: C.accent2, width: 3 }, objectName: 'Emblema anillo' });
    s.addShape(pres.shapes.OVAL, { x: 9.2, y: 1.8, w: 3.0, h: 3.0, fill: { color: C.accent1, transparency: 45 }, line: { color: C.accent1, width: 0 }, objectName: 'Emblema centro' });
    T(s, 'K·U', { x: 9.2, y: 1.8, w: 3.0, h: 3.0, fontSize: 92, bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: 'Emblema K·U' });
    T(s, '12 000 productos de matrices\npor cada sismo de 60 s', { x: 8.75, y: 5.6, w: 3.9, h: 0.8, fontSize: 14, color: C.background1, align: 'center', objectName: 'Emblema texto' });
    notas(s, 1);
  }

  // ================= 2 · EL PROBLEMA =================
  {
    pres.addSection({ title: 'Proyecto' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Proyecto' });
    s.addText('01 · EL PROBLEMA', { placeholder: 'kicker' });
    s.addText('¿Por qué simular un sismo en paralelo?', { placeholder: 'title' });
    tarjeta(s, 0.6, 1.65, 7.4, 1.55, 'Problema fondo');
    T(s, [
      { text: 'Para saber cómo responde un edificio ante un sismo hay que ' },
      { text: 'simular su movimiento', options: { bold: true } },
      { text: ' paso a paso durante todo el registro. Con edificios grandes y varios sismos, son ' },
      { text: 'miles de productos de matrices grandes', options: { bold: true } },
      { text: '.' },
    ], { x: 0.9, y: 1.8, w: 6.8, h: 1.25, fontSize: 18, valign: 'middle', objectName: 'Problema' });
    tarjeta(s, 0.6, 3.38, 7.4, 0.62, 'Objetivo fondo', C.accent3);
    T(s, 'Objetivo: el mismo resultado que el secuencial, en una fracción del tiempo', { x: 0.9, y: 3.38, w: 7.0, h: 0.62, fontSize: 16, bold: true, color: C.background1, valign: 'middle', objectName: 'Objetivo' });
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 0.6, y: 4.2, w: 7.4, h: 2.45, rectRadius: 0.12, fill: { color: C.background1 }, line: { color: C.text2, width: 1.25 }, objectName: 'Cifra marco' });
    T(s, '12 000', { x: 0.9, y: 4.4, w: 2.9, h: 1.0, fontSize: 48, bold: true, color: C.accent2, valign: 'middle', objectName: 'Cifra' });
    T(s, [
      { text: 'productos K·U para simular un registro de 60 s con Δt = 0,005 s', options: { breakLine: true } },
      { text: 'Uno por paso de tiempo; cada uno atiende todos los sismos a la vez.', options: { color: GRIS } },
    ], { x: 3.85, y: 4.42, w: 3.95, h: 1.3, fontSize: 15, paraSpaceAfter: 4, objectName: 'Cifra texto' });
    T(s, 'Ese producto es el que repartimos entre núcleos, procesos y la GPU.', { x: 0.9, y: 5.85, w: 6.9, h: 0.5, fontSize: 16, italic: true, color: C.text2, valign: 'middle', objectName: 'Frase producto' });
    const filas = [
      [ic.globo, 'Perú sísmico', 'Estamos en el Cinturón de Fuego del Pacífico: los sismos fuertes son parte de nuestra realidad.', C.accent2],
      [ic.edificio, 'El modelo', 'El edificio se representa con una matriz de rigidez K.', C.accent1],
      [ic.chart, 'El resultado', 'Desplazamiento y deriva de cada piso para cada sismo, y un mapa de demanda.', C.accent3],
    ];
    filas.forEach(([img, t, d, col], i) => {
      const y = 1.75 + i * 1.65;
      circuloIcono(s, img, 8.55, y, 1.0, col, t);
      T(s, t, { x: 9.8, y: y + 0.02, w: 2.9, h: 0.42, fontSize: 19, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, d, { x: 9.8, y: y + 0.45, w: 2.9, h: 0.95, fontSize: 14, objectName: t + ' texto' });
    });
    notas(s, 2);
  }

  // ================= 3 · EL MODELO =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Proyecto' });
    s.addText('02 · EL MODELO', { placeholder: 'kicker' });
    s.addText('Del sismo a las matrices: un producto K · U por paso', { placeholder: 'title' });
    const P = [
      ['Estructura', 'Un pórtico de pisos × vanos', ['Un nodo (masa) en cada cruce', 'Resortes: columnas y losas'], C.accent1],
      ['Matriz K', 'Rigidez, n × n', ['Dispersa: ≤ 5 no nulos por fila', 'Se guarda en formato CSR'], C.accent5],
      ['Matriz U', 'Escenarios, n × k', ['Una columna por sismo', 'Todos los sismos a la vez'], C.accent6],
      ['K · U', 'En cada paso de tiempo', ['El único trabajo pesado', 'Lo que paralelizamos'], C.accent2],
      ['Respuesta', 'Por piso y por sismo', ['Desplazamiento máximo', 'Deriva y mapa de demanda'], C.accent3],
    ];
    P.forEach(([nom, preg, items, col], i) => {
      const x = 0.6 + i * 2.45, cx = x + 1.15;
      if (i === 3) tarjeta(s, x, 2.2, 2.3, 2.75, nom + ' columna', C.accent2, 86);
      else tarjeta(s, x, 2.2, 2.3, 2.75, nom + ' columna');
      numero(s, i + 1, cx - 0.5, 1.5, 1.0, col, nom);
      T(s, nom, { x: x + 0.15, y: 2.62, w: 2.0, h: 0.42, fontSize: 20, bold: true, color: C.text2, align: 'center', objectName: nom + ' nombre' });
      T(s, preg, { x: x + 0.15, y: 3.05, w: 2.0, h: 0.6, fontSize: 15, italic: true, color: C.text2, align: 'center', valign: 'middle', objectName: nom + ' pregunta' });
      T(s, items.map((h, k) => ({ text: h, options: { bullet: { indent: 14 }, breakLine: k < items.length - 1 } })),
        { x: x + 0.18, y: 3.78, w: 2.0, h: 1.1, fontSize: 14, paraSpaceAfter: 4, objectName: nom + ' ideas' });
      if (i < 4) s.addImage({ data: ic.arrow, x: x + 2.28, y: 1.83, w: 0.34, h: 0.34, objectName: 'Flecha ' + (i + 1) });
    });
    tarjeta(s, 0.6, 5.15, 12.1, 1.55, 'Ecuaciones fondo');
    T(s, 'ECUACIÓN DE MOVIMIENTO', { x: 0.9, y: 5.32, w: 4.4, h: 0.28, fontSize: 11, bold: true, color: C.accent1, charSpacing: 2, objectName: 'Rótulo ecuación' });
    T(s, [{ text: 'M·ü + K·u = −M·1·a' }, { text: 'g', options: { subscript: true } }, { text: '(t)' }], { x: 0.9, y: 5.68, w: 4.6, h: 0.6, fontSize: 26, bold: true, color: C.text2, valign: 'middle', objectName: 'Ecuación de movimiento' });
    T(s, 'EN CADA PASO (DIFERENCIAS CENTRALES)', { x: 5.75, y: 5.32, w: 6.8, h: 0.28, fontSize: 11, bold: true, color: C.accent1, charSpacing: 2, objectName: 'Rótulo paso' });
    T(s, 'U(n+1) = 2·U(n) − U(n−1) + Δt²·M⁻¹·[F(n) − K·U(n)]', { x: 5.75, y: 5.62, w: 6.8, h: 0.45, fontSize: 18, bold: true, color: C.text2, valign: 'middle', objectName: 'Ecuación del paso' });
    T(s, [{ text: 'M es diagonal: lo único caro de cada paso es el producto ' }, { text: 'K·U', options: { bold: true, color: C.accent2 } }, { text: '.' }],
      { x: 5.75, y: 6.12, w: 6.8, h: 0.4, fontSize: 15, objectName: 'Nota M diagonal' });
    notas(s, 3);
  }

  // ================= 4 · EL PLAN =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Proyecto' });
    s.addText('03 · EL PLAN', { placeholder: 'kicker' });
    s.addText('Doce semanas, cinco fases, cinco hitos', { placeholder: 'title' });
    const F = [
      ['S5–S6', 'Fundamentos y modelo', 'Modelo físico, secuencial de referencia y simulador validado', 'H1 · Cumplido', C.accent3, 'hecho'],
      ['S7–S8', 'Memoria compartida', 'OpenMP y Go: varios hilos en un solo computador', 'H2 · Avance con demo', C.accent2, 'ahora'],
      ['S9–S11', 'Memoria distribuida', 'MPI: Cannon, SUMMA y el edificio repartido entre procesos', 'H3 · Simulación distribuida', C.accent1, ''],
      ['S12–S13', 'Híbrido MPI + CUDA', 'El K·U de cada proceso se calcula en la GPU', 'H4 · Simulador híbrido', C.accent6, ''],
      ['S14–S16', 'Evaluación', 'Escalabilidad, escenarios sísmicos e informe IEEE', 'H5 · Exposición final', C.accent5, ''],
    ];
    s.addShape(pres.shapes.LINE, { x: 0.9, y: 2.75, w: 11.6, h: 0, line: { color: C.text1, width: 1.5, transparency: 60 }, objectName: 'Eje de semanas' });
    F.forEach(([sem, fase, que, hito, col, estado], i) => {
      const x = 0.6 + i * 2.45, cx = x + 1.15;
      if (estado === 'ahora') {
        s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: cx - 0.8, y: 1.5, w: 1.6, h: 0.3, rectRadius: 0.08, fill: { color: C.accent2 }, line: { color: C.accent2, width: 0 }, objectName: 'Estamos aquí fondo' });
        T(s, 'ESTAMOS AQUÍ', { x: cx - 0.8, y: 1.5, w: 1.6, h: 0.3, fontSize: 11, bold: true, color: C.background1, align: 'center', valign: 'middle', charSpacing: 1, objectName: 'Estamos aquí' });
      }
      T(s, sem, { x: cx - 1.1, y: 1.88, w: 2.2, h: 0.6, fontSize: 26, bold: true, color: C.text2, align: 'center', valign: 'bottom', objectName: sem + ' semanas' });
      const d = estado === 'ahora' ? 0.42 : 0.34;
      s.addShape(pres.shapes.OVAL, { x: cx - d / 2, y: 2.75 - d / 2, w: d, h: d, fill: { color: col }, line: { color: C.background1, width: 2 }, objectName: sem + ' punto' });
      tarjeta(s, x, 3.1, 2.3, 2.35, sem + ' tarjeta');
      T(s, fase, { x: x + 0.15, y: 3.25, w: 2.0, h: 0.62, fontSize: 16, bold: true, color: C.text2, align: 'center', valign: 'middle', objectName: sem + ' fase' });
      T(s, que, { x: x + 0.15, y: 3.92, w: 2.0, h: 0.95, fontSize: 14, align: 'center', objectName: sem + ' qué' });
      const fondoHito = estado ? col : C.background1;
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: x + 0.15, y: 4.92, w: 2.0, h: 0.38, rectRadius: 0.08, fill: { color: fondoHito }, line: { color: estado ? col : C.text1, width: 0.75, transparency: estado ? 0 : 60 }, objectName: sem + ' hito fondo' });
      T(s, (estado === 'hecho' ? '✔ ' : '') + hito, { x: x + 0.15, y: 4.92, w: 2.0, h: 0.38, fontSize: 12, bold: true, color: estado ? C.background1 : C.text2, align: 'center', valign: 'middle', objectName: sem + ' hito' });
    });
    tarjeta(s, 0.6, 5.72, 12.1, 0.95, 'Alcance fondo');
    T(s, [
      { text: 'Alcance graduable: ', options: { bold: true, color: C.text2 } },
      { text: 'cada nivel ya es un entregable completo.', options: { breakLine: true } },
      { text: 'Mínimo (S8): ', options: { bold: true, color: C.accent2 } }, { text: 'OpenMP y Go  ·  ' },
      { text: 'Núcleo (S11): ', options: { bold: true, color: C.accent1 } }, { text: '+ MPI  ·  ' },
      { text: 'Completo (S13): ', options: { bold: true, color: C.accent6 } }, { text: '+ CUDA' },
    ], { x: 0.9, y: 5.72, w: 11.6, h: 0.95, fontSize: 16, valign: 'middle', objectName: 'Alcance graduable' });
    notas(s, 4);
  }

  // ================= 5 · FASE 1 =================
  {
    pres.addSection({ title: 'Fase 1' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Fase 1' });
    s.addText('04 · FASE 1 (S5–S6)', { placeholder: 'kicker' });
    s.addText('Primero correcto: el simulador secuencial validado', { placeholder: 'title' });
    tarjeta(s, 0.6, 1.6, 7.3, 5.1, 'Oscilador panel');
    T(s, 'Oscilador de 1 grado de libertad', { x: 0.9, y: 1.78, w: 6.7, h: 0.4, fontSize: 18, bold: true, color: C.text2, objectName: 'Oscilador título' });
    T(s, 'Una masa y un resorte: su movimiento tiene solución exacta conocida.', { x: 0.9, y: 2.18, w: 6.7, h: 0.32, fontSize: 14, color: GRIS, objectName: 'Oscilador subtítulo' });
    // Curva exacta u(t) = e^(−0.05 t)·cos(t) y puntos "simulados" encima
    const X0 = 1.0, AW = 6.5, Y0 = 3.95, AMP = 1.05, TMAX = 6 * 2 * Math.PI;
    const u = t => Math.exp(-0.05 * t) * Math.cos(t);
    const px = t => X0 + t / TMAX * AW, py = v => Y0 - v * AMP;
    s.addShape(pres.shapes.LINE, { x: X0, y: Y0, w: AW, h: 0, line: { color: C.text1, width: 0.75, transparency: 60 }, objectName: 'Eje tiempo' });
    const pts = [];
    for (let k = 0; k <= 240; k++) { const t = TMAX * k / 240; pts.push({ x: px(t) - X0, y: py(u(t)) - (Y0 - AMP) }); }
    s.addShape(pres.shapes.CUSTOM_GEOMETRY, { x: X0, y: Y0 - AMP, w: AW, h: 2 * AMP, points: pts, fill: { color: C.background2, transparency: 100 }, line: { color: C.text2, width: 2 }, objectName: 'Solución exacta' });
    for (let k = 0; k <= 48; k++) {
      const t = TMAX * k / 48, d = 0.1;
      s.addShape(pres.shapes.OVAL, { x: px(t) - d / 2, y: py(u(t)) - d / 2, w: d, h: d, fill: { color: C.accent2 }, line: { color: C.background1, width: 0.5 }, objectName: 'Simulación punto ' + k });
    }
    T(s, 'tiempo →', { x: 6.5, y: Y0 + 0.5, w: 1.0, h: 0.28, fontSize: 11, color: GRIS, align: 'right', objectName: 'Etiqueta tiempo' });
    s.addShape(pres.shapes.LINE, { x: 1.0, y: 5.35, w: 0.4, h: 0, line: { color: C.text2, width: 2 }, objectName: 'Leyenda exacta línea' });
    T(s, 'Solución exacta', { x: 1.5, y: 5.2, w: 2.2, h: 0.3, fontSize: 13, valign: 'middle', objectName: 'Leyenda exacta' });
    s.addShape(pres.shapes.OVAL, { x: 3.85, y: 5.29, w: 0.12, h: 0.12, fill: { color: C.accent2 }, line: { color: C.accent2, width: 0 }, objectName: 'Leyenda simulación punto' });
    T(s, 'Nuestra simulación (diferencias centrales)', { x: 4.1, y: 5.2, w: 3.6, h: 0.3, fontSize: 13, valign: 'middle', objectName: 'Leyenda simulación' });
    T(s, [{ text: 'Hito H1: ', options: { bold: true, color: C.accent3 } },
      { text: 'el periodo medido en la simulación es T = 2π·√(m/k), con un error de 10⁻⁵.' }],
    { x: 0.9, y: 5.75, w: 6.7, h: 0.7, fontSize: 15, valign: 'middle', objectName: 'Hito H1' });
    const stats = [
      ['70', C.accent3, 'pruebas automáticas', 'Todas OK: 47 del producto de matrices y 23 del simulador'],
      ['÷ 4', C.accent1, 'convergencia de 2.º orden', 'Δt a la mitad → el error cae 4 veces, como predice la teoría'],
      ['6×', C.accent2, 'resonancia a 1 Hz', 'El sismo con la frecuencia del edificio mueve el techo 6 veces más'],
    ];
    stats.forEach(([num, col, tit, det], i) => {
      const y = 1.6 + i * 1.77;
      tarjeta(s, 8.15, y, 4.55, 1.55, tit + ' fondo');
      T(s, num, { x: 8.35, y, w: 1.45, h: 1.55, fontSize: 40, bold: true, color: col, align: 'center', valign: 'middle', objectName: tit + ' cifra' });
      T(s, tit, { x: 9.9, y: y + 0.2, w: 2.65, h: 0.38, fontSize: 16, bold: true, color: C.text2, objectName: tit + ' título' });
      T(s, det, { x: 9.9, y: y + 0.6, w: 2.65, h: 0.85, fontSize: 13, color: C.text1, objectName: tit + ' detalle' });
    });
    notas(s, 5);
  }

  // ================= 6 · CÓDIGO 1: spmm_csr =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Fase 1' });
    s.addText('05 · NUESTRO CÓDIGO', { placeholder: 'kicker' });
    s.addText('Función clave 1: spmm_csr, el producto K·U', { placeholder: 'title' });
    T(s, 'CSR: solo se guardan los números distintos de cero', { x: 0.6, y: 1.6, w: 5.8, h: 0.4, fontSize: 17, bold: true, color: C.text2, objectName: 'CSR título' });
    const K = [[4, -1, 0, 0], [-1, 4, -1, 0], [0, -1, 4, -1], [0, 0, -1, 4]];
    const FILA = [C.accent1, C.accent5, C.accent6, C.accent4];
    const cx0 = 1.55, cy0 = 2.15, cs = 0.46;
    T(s, 'K =', { x: 0.6, y: cy0, w: 0.85, h: 4 * cs, fontSize: 20, bold: true, color: C.text2, valign: 'middle', align: 'right', objectName: 'K igual' });
    K.forEach((fila, i) => fila.forEach((v, j) => {
      const nz = v !== 0;
      s.addShape(pres.shapes.RECTANGLE, { x: cx0 + j * cs, y: cy0 + i * cs, w: cs, h: cs, fill: { color: nz ? FILA[i] : C.background1, transparency: nz ? 20 : 0 }, line: { color: 'D5DCE5', width: 0.75 }, objectName: `K ${i},${j} celda` });
      T(s, String(v), { x: cx0 + j * cs, y: cy0 + i * cs, w: cs, h: cs, fontSize: 14, bold: nz, color: nz ? C.background1 : 'B8C0CC', align: 'center', valign: 'middle', fontFace: MONO, objectName: `K ${i},${j}` });
    }));
    T(s, 'Cada color es una fila (un nodo). Los ceros no se guardan.', { x: 3.6, y: 2.25, w: 2.8, h: 1.4, fontSize: 14, color: C.text1, valign: 'middle', objectName: 'Explicación colores' });
    const val = [], col = [], filaDe = [];
    K.forEach((f, i) => f.forEach((v, j) => { if (v) { val.push(v); col.push(j); filaDe.push(i); } }));
    const ptr = [0, 2, 5, 8, 10];
    const arr = (nombre, datos, colores, y) => {
      T(s, nombre, { x: 0.6, y, w: 1.05, h: 0.4, fontSize: 13, bold: true, color: C.text2, fontFace: MONO, valign: 'middle', objectName: nombre + ' nombre' });
      datos.forEach((v, k) => {
        const x = 1.7 + k * 0.46;
        s.addShape(pres.shapes.RECTANGLE, { x, y, w: 0.46, h: 0.4, fill: { color: colores[k], transparency: colores[k] === C.background2 ? 0 : 20 }, line: { color: C.background1, width: 1 }, objectName: `${nombre} ${k} celda` });
        T(s, String(v), { x, y, w: 0.46, h: 0.4, fontSize: 13, bold: true, color: colores[k] === C.background2 ? C.text2 : C.background1, align: 'center', valign: 'middle', fontFace: MONO, objectName: `${nombre} ${k}` });
      });
    };
    arr('val', val, filaDe.map(i => FILA[i]), 4.2);
    arr('col_idx', col, filaDe.map(i => FILA[i]), 4.7);
    arr('row_ptr', ptr, ptr.map((p, i) => i < 4 ? FILA[i] : C.background2), 5.2);
    T(s, [{ text: 'Edificio de 100 × 400: ', options: { bold: true, color: C.text2 } },
      { text: 'K tiene 1 600 millones de casillas, pero solo ~200 mil son distintas de cero.' }],
    { x: 0.6, y: 5.85, w: 5.8, h: 0.75, fontSize: 15, valign: 'middle', objectName: 'Dato CSR' });

    const L = [
      'for (i = 0; i < n; i++) {',
      '  for (j = 0; j < k; j++) C[i][j] = 0;',
      '  for (t = row_ptr[i]; t < row_ptr[i+1]; t++) {',
      '    a = val[t];',
      '    u = U[col_idx[t]];',
      '    for (j = 0; j < k; j++)',
      '      C[i][j] += a * u[j];',
      '  }',
      '}',
    ];
    const cod = codigo(s, 6.65, 1.6, 6.05, L, { rotulo: 'C · src/seq.c (simplificado)', nombre: 'Código spmm_csr' });
    [[1, 0], [2, 2], [3, 5]].forEach(([n, li]) => marcador(s, n, 12.28, cod.yLinea(li), C.accent2, 'Código'));
    const EXP = [
      'Cada fila de K es un nodo: calcula la fuerza sobre ese nodo.',
      'Solo recorre sus vecinos (≤ 5), no los n nodos del edificio.',
      'Recorre los k sismos: un producto mueve todos los escenarios a la vez.',
    ];
    EXP.forEach((t, i) => {
      const y = 1.6 + cod.alto + 0.22 + i * 0.6;
      numero(s, i + 1, 6.65, y + 0.06, 0.4, C.accent2, 'Explicación ' + (i + 1));
      T(s, t, { x: 7.2, y, w: 5.5, h: 0.52, fontSize: 15, valign: 'middle', objectName: 'Explicación ' + (i + 1) + ' texto' });
    });
    notas(s, 6);
  }

  // ================= 7 · CÓDIGO 2: sim_run =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Fase 1' });
    s.addText('05 · NUESTRO CÓDIGO', { placeholder: 'kicker' });
    s.addText('Función clave 2: sim_run, el bucle en el tiempo', { placeholder: 'title' });
    const L = [
      'for (paso = 0; paso < N; paso++) {',
      '  cfg->spmm(K, U, KU);',
      '  for (i = 0; i < n; i++)',
      '    for (s = 0; s < k; s++)',
      '      Un[i][s] = (2·U[i][s]/dt² − c2·Up[i][s]',
      '                  − ag[paso][s] − KU[i][s]/m[i]) / c1;',
      '  picos(Un);',
      '  rotar(Up, U, Un);',
      '}',
    ];
    const cod = codigo(s, 0.6, 1.6, 7.15, L, { rotulo: 'C · src/sim.c (simplificado)', nombre: 'Código sim_run' });
    [[1, 1], [2, 2], [3, 6], [4, 7]].forEach(([n, li]) => marcador(s, n, 7.2, cod.yLinea(li), C.accent2, 'Paso'));
    const PASOS = ['K·U: el producto de matrices', 'Nuevo desplazamiento de cada nodo', 'Desplazamiento y deriva máximos', 'Rotar punteros: no se copia nada'];
    const yP = 1.6 + cod.alto + 0.3;
    PASOS.forEach((t, i) => {
      const x = 0.6 + (i % 2) * 3.65, y = yP + Math.floor(i / 2) * 0.62;
      numero(s, i + 1, x, y, 0.4, C.accent2, 'Paso explicación ' + (i + 1));
      T(s, t, { x: x + 0.52, y: y - 0.06, w: 3.0, h: 0.52, fontSize: 15, valign: 'middle', objectName: 'Paso ' + (i + 1) + ' texto' });
    });
    // El enchufe
    tarjeta(s, 8.1, 1.6, 4.6, 5.1, 'Enchufe panel');
    circuloIcono(s, ic.plug, 8.35, 1.8, 0.7, C.accent2, 'Enchufe');
    T(s, 'Un enchufe para cada versión', { x: 9.2, y: 1.8, w: 3.4, h: 0.7, fontSize: 18, bold: true, color: C.text2, valign: 'middle', objectName: 'Enchufe título' });
    T(s, [{ text: 'cfg->spmm', options: { fontFace: MONO, bold: true, color: C.accent2 } },
      { text: ' es un puntero a función: el simulador no sabe qué versión de K·U está usando.' }],
    { x: 8.4, y: 2.65, w: 4.1, h: 0.9, fontSize: 14, objectName: 'Enchufe explicación' });
    const V = [
      ['spmm_csr', 'S6 · secuencial', true],
      ['spmm_csr_omp', 'S7 · OpenMP', true],
      ['versión MPI', 'S9–S11 · procesos', false],
      ['versión CUDA', 'S12–S13 · GPU', false],
    ];
    V.forEach(([nom, cuando, listo], i) => {
      const y = 3.7 + i * 0.6;
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 8.4, y, w: 4.05, h: 0.48, rectRadius: 0.08, fill: { color: listo ? C.background1 : C.background2 }, line: { color: listo ? C.accent3 : C.text1, width: 1, dashType: listo ? 'solid' : 'dash', transparency: listo ? 0 : 55 }, objectName: nom + ' fondo' });
      T(s, nom, { x: 8.55, y, w: 1.85, h: 0.48, fontSize: 13, bold: true, fontFace: MONO, color: listo ? C.text2 : GRIS, valign: 'middle', objectName: nom + ' nombre' });
      T(s, cuando, { x: 10.4, y, w: 1.6, h: 0.48, fontSize: 13, color: listo ? C.text1 : GRIS, valign: 'middle', objectName: nom + ' cuándo' });
      if (listo) s.addImage({ data: ic.checkV, x: 12.08, y: y + 0.12, w: 0.24, h: 0.24, objectName: nom + ' listo' });
    });
    T(s, 'Cambiar de versión es cambiar una línea; la verificación es siempre la misma.', { x: 8.4, y: 6.15, w: 4.1, h: 0.45, fontSize: 13, italic: true, color: C.text2, objectName: 'Enchufe conclusión' });
    notas(s, 7);
  }

  // ================= 8 · LÍNEA BASE Y AMDAHL =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Fase 1' });
    s.addText('06 · MEDIR ANTES DE PARALELIZAR', { placeholder: 'kicker' });
    s.addText('La línea base T(1) y la ley de Amdahl', { placeholder: 'title' });
    // Panel izquierdo: orden de bucles
    tarjeta(s, 0.6, 1.6, 5.95, 5.1, 'Bucles panel');
    T(s, 'El orden de los bucles importa', { x: 0.9, y: 1.78, w: 5.4, h: 0.4, fontSize: 18, bold: true, color: C.text2, objectName: 'Bucles título' });
    T(s, 'C = A·B con n = 1024, un solo hilo', { x: 0.9, y: 2.18, w: 5.4, h: 0.32, fontSize: 14, color: GRIS, objectName: 'Bucles subtítulo' });
    const B = [['naive (i-j-k)', 1.487, C.text1], ['ikj', 0.474, C.accent1], ['por bloques', 0.445, C.accent1]];
    B.forEach(([nom, t, col], i) => {
      const y = 2.75 + i * 0.62, w = t / 1.487 * 2.95;
      T(s, nom, { x: 0.9, y, w: 1.55, h: 0.45, fontSize: 15, valign: 'middle', objectName: nom + ' etiqueta' });
      s.addShape(pres.shapes.RECTANGLE, { x: 2.5, y, w, h: 0.45, fill: { color: col, transparency: i === 0 ? 55 : 0 }, line: { color: col, width: 0, transparency: i === 0 ? 55 : 0 }, objectName: nom + ' barra' });
      T(s, (t.toFixed(2) + ' s').replace('.', ','), { x: 2.6 + w, y, w: 0.9, h: 0.45, fontSize: 15, bold: true, color: C.text2, valign: 'middle', objectName: nom + ' valor' });
    });
    T(s, '3×', { x: 0.9, y: 4.7, w: 1.3, h: 0.9, fontSize: 44, bold: true, color: C.accent2, valign: 'middle', objectName: 'Tres veces' });
    T(s, 'más rápido sin ningún hilo extra: es la memoria caché.', { x: 2.2, y: 4.7, w: 4.1, h: 0.9, fontSize: 15, valign: 'middle', objectName: 'Tres veces texto' });
    T(s, [{ text: 'Por eso T(1) es siempre la ' }, { text: 'mejor', options: { bold: true } }, { text: ' versión secuencial.' }],
      { x: 0.9, y: 5.8, w: 5.4, h: 0.6, fontSize: 16, color: C.text2, valign: 'middle', objectName: 'Conclusión T1' });
    // Panel derecho: Amdahl
    tarjeta(s, 6.75, 1.6, 5.95, 5.1, 'Amdahl panel');
    T(s, 'Ley de Amdahl', { x: 7.05, y: 1.78, w: 5.4, h: 0.4, fontSize: 18, bold: true, color: C.text2, objectName: 'Amdahl título' });
    T(s, 'Simulador secuencial, edificio de 100 × 400 (40 100 nodos)', { x: 7.05, y: 2.18, w: 5.4, h: 0.32, fontSize: 14, color: GRIS, objectName: 'Amdahl subtítulo' });
    const W = 5.35, f = 0.47;
    s.addShape(pres.shapes.RECTANGLE, { x: 7.05, y: 2.8, w: W * f, h: 0.75, fill: { color: C.accent2 }, line: { color: C.background1, width: 1.5 }, objectName: 'Fracción K·U' });
    s.addShape(pres.shapes.RECTANGLE, { x: 7.05 + W * f, y: 2.8, w: W * (1 - f), h: 0.75, fill: { color: C.accent1 }, line: { color: C.background1, width: 1.5 }, objectName: 'Fracción resto' });
    T(s, 'K·U  47 %', { x: 7.05, y: 2.8, w: W * f, h: 0.75, fontSize: 16, bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: 'Fracción K·U texto' });
    T(s, 'actualizar U y picos  53 %', { x: 7.05 + W * f, y: 2.8, w: W * (1 - f), h: 0.75, fontSize: 14, bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: 'Fracción resto texto' });
    T(s, 'S ≤ 1 / (1 − f) = 1 / 0,53 ≈ 1,9×', { x: 7.05, y: 3.8, w: 5.35, h: 0.6, fontSize: 24, bold: true, color: C.text2, align: 'center', valign: 'middle', objectName: 'Fórmula Amdahl' });
    T(s, 'Si solo se paraleliza K·U, ni con infinitos hilos se pasa de 1,9×.', { x: 7.05, y: 4.45, w: 5.35, h: 0.6, fontSize: 15, align: 'center', valign: 'middle', objectName: 'Amdahl explicación' });
    tarjeta(s, 7.05, 5.3, 5.35, 1.1, 'Decisión fondo', C.accent3);
    T(s, 'Decisión para S7: paralelizar también la actualización de U y los picos', { x: 7.3, y: 5.3, w: 4.85, h: 1.1, fontSize: 16, bold: true, color: C.background1, valign: 'middle', objectName: 'Decisión S7' });
    notas(s, 8);
  }

  // ================= 9 · OPENMP =================
  {
    pres.addSection({ title: 'Semana 7' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Semana 7' });
    s.addText('07 · S7 · OPENMP', { placeholder: 'kicker' });
    s.addText('OpenMP: cada hilo calcula sus propias filas', { placeholder: 'title' });
    T(s, 'K·U: filas (nodos) por hilo', { x: 0.6, y: 1.6, w: 2.6, h: 0.6, fontSize: 15, bold: true, color: C.text2, objectName: 'Reparto K·U título' });
    for (let h = 0; h < 4; h++) {
      s.addShape(pres.shapes.RECTANGLE, { x: 0.8, y: 2.3 + h * 0.75, w: 1.9, h: 0.75, fill: { color: HILO[h] }, line: { color: C.background1, width: 1.5 }, objectName: `K·U filas hilo ${h}` });
      T(s, `hilo ${h}`, { x: 0.8, y: 2.3 + h * 0.75, w: 1.9, h: 0.75, fontSize: 14, bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: `K·U etiqueta hilo ${h}` });
    }
    T(s, 'Producto denso: bloques de 64 × 64', { x: 3.25, y: 1.6, w: 3.1, h: 0.6, fontSize: 15, bold: true, color: C.text2, objectName: 'Reparto denso título' });
    for (let bi = 0; bi < 4; bi++) for (let bj = 0; bj < 4; bj++) {
      s.addShape(pres.shapes.RECTANGLE, { x: 3.35 + bj * 0.75, y: 2.3 + bi * 0.75, w: 0.75, h: 0.75, fill: { color: HILO[bi] }, line: { color: C.background1, width: 1.5 }, objectName: `Bloque ${bi},${bj}` });
    }
    T(s, 'C', { x: 0.8, y: 5.35, w: 1.9, h: 0.35, fontSize: 14, bold: true, color: C.text2, align: 'center', objectName: 'Etiqueta C filas' });
    T(s, 'C (16 bloques, 4 por hilo)', { x: 3.35, y: 5.35, w: 3.0, h: 0.35, fontSize: 14, bold: true, color: C.text2, align: 'center', objectName: 'Etiqueta C bloques' });
    T(s, 'Cada hilo escribe solo en su parte de C: nunca dos hilos en la misma casilla.', { x: 0.6, y: 5.85, w: 5.8, h: 0.75, fontSize: 15, italic: true, color: C.text2, valign: 'middle', objectName: 'Regla OpenMP' });
    const L = [
      '#pragma omp parallel for schedule(static)',
      'for (int i = 0; i < n; i++) {',
      '    /* fila i de C: solo este hilo la escribe */',
      '}',
    ];
    const cod = codigo(s, 6.75, 1.6, 5.95, L, { rotulo: 'C · src/omp/matmul_omp.c', nombre: 'Código OpenMP' });
    const R = [
      [ic.unlock, 'Sin condiciones de carrera', 'Dos hilos nunca escriben la misma fila: no hace falta critical, atomic ni candados.', C.accent3],
      [ic.equals, 'Idéntico bit a bit', 'Mismo orden de las sumas que el secuencial: 51 pruebas con 1, 2, 3, 4 y 8 hilos.', C.accent1],
      [ic.capas, 'Todo el paso, no solo K·U', 'sim_run_omp reparte también la actualización de U y los picos, con una barrera entre ambos.', C.accent2],
    ];
    R.forEach(([img, t, d, col], i) => {
      const y = 1.6 + cod.alto + 0.3 + i * 1.08;
      circuloIcono(s, img, 6.8, y + 0.05, 0.75, col, t);
      T(s, t, { x: 7.75, y, w: 4.95, h: 0.38, fontSize: 17, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, d, { x: 7.75, y: y + 0.4, w: 4.95, h: 0.62, fontSize: 14, objectName: t + ' texto' });
    });
    notas(s, 9);
  }

  // ================= 10 · GO =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Semana 7' });
    s.addText('08 · S7 · GO', { placeholder: 'kicker' });
    s.addText('Go: un worker pool de goroutines', { placeholder: 'title' });
    // Diagrama: main -> canal -> goroutines -> C
    tarjeta(s, 0.6, 1.6, 5.95, 3.55, 'Diagrama fondo');
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 0.8, y: 2.95, w: 0.95, h: 0.75, rectRadius: 0.08, fill: { color: C.text2 }, line: { color: C.text2, width: 0 }, objectName: 'main' });
    T(s, 'main', { x: 0.8, y: 2.95, w: 0.95, h: 0.75, fontSize: 15, bold: true, color: C.background1, align: 'center', valign: 'middle', fontFace: MONO, objectName: 'main texto' });
    s.addImage({ data: ic.arrow, x: 1.8, y: 3.17, w: 0.3, h: 0.3, objectName: 'Flecha main canal' });
    T(s, 'canal de tareas', { x: 2.15, y: 2.5, w: 1.75, h: 0.35, fontSize: 13, bold: true, color: C.text2, align: 'center', objectName: 'Canal rótulo' });
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 2.15, y: 2.95, w: 1.75, h: 0.75, rectRadius: 0.3, fill: { color: C.background1 }, line: { color: C.text2, width: 1.25 }, objectName: 'Canal' });
    ['0–63', '64–127', '…'].forEach((t, k) => {
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 2.25 + k * 0.53, y: 3.12, w: 0.5, h: 0.41, rectRadius: 0.05, fill: { color: C.accent4, transparency: 20 }, line: { color: C.accent4, width: 0 }, objectName: 'Tarea ' + k });
      T(s, t, { x: 2.25 + k * 0.53, y: 3.12, w: 0.5, h: 0.41, fontSize: 9, bold: true, color: C.text2, align: 'center', valign: 'middle', fontFace: MONO, objectName: 'Tarea texto ' + k });
    });
    const gy = [1.95, 2.6, 3.25, 3.9];
    gy.forEach((y, k) => {
      ln(s, 3.9, 3.32, 4.25, y + 0.25, 'Canal a G' + (k + 1), { width: 1.25 });
      s.addShape(pres.shapes.OVAL, { x: 4.25, y, w: 0.5, h: 0.5, fill: { color: HILO[k] }, line: { color: C.background1, width: 1.5 }, objectName: 'Goroutine ' + (k + 1) });
      T(s, 'G' + (k + 1), { x: 4.25, y, w: 0.5, h: 0.5, fontSize: 12, bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: 'Goroutine texto ' + (k + 1) });
      ln(s, 4.75, y + 0.25, 5.4, 2.2 + k * 0.55 + 0.27, 'G' + (k + 1) + ' a C', { width: 1.25 });
      s.addShape(pres.shapes.RECTANGLE, { x: 5.4, y: 2.2 + k * 0.55, w: 0.85, h: 0.55, fill: { color: HILO[k] }, line: { color: C.background1, width: 1.5 }, objectName: 'C filas ' + (k + 1) });
    });
    T(s, 'C', { x: 5.4, y: 1.8, w: 0.85, h: 0.35, fontSize: 14, bold: true, color: C.text2, align: 'center', objectName: 'C rótulo' });
    T(s, 'trabajadores', { x: 3.75, y: 1.65, w: 1.3, h: 0.3, fontSize: 12, bold: true, color: C.text2, align: 'center', objectName: 'Trabajadores rótulo' });
    numero(s, 1, 5.02, 1.65, 0.3, C.accent2, 'Diagrama 1');
    numero(s, 2, 2.88, 3.8, 0.3, C.accent2, 'Diagrama 2');
    numero(s, 3, 1.82, 2.75, 0.3, C.accent2, 'Diagrama 3');
    s.addImage({ data: ic.reloj, x: 0.85, y: 4.55, w: 0.3, h: 0.3, objectName: 'WaitGroup ícono' });
    T(s, [{ text: 'sync.WaitGroup', options: { fontFace: MONO, bold: true } }, { text: ': main espera a que terminen todos' }],
      { x: 1.25, y: 4.5, w: 4.6, h: 0.4, fontSize: 13, valign: 'middle', objectName: 'WaitGroup' });
    numero(s, 4, 5.95, 4.55, 0.3, C.accent2, 'Diagrama 4');
    // Código
    const L = [
      'tareas := make(chan tarea)',
      'var wg sync.WaitGroup',
      'for w := 0; w < p; w++ {',
      '    wg.Add(1)',
      '    go func() {',
      '        defer wg.Done()',
      '        for t := range tareas {',
      '            calcular(t.ini, t.fin)',
      '        }',
      '    }()',
      '}',
      'for ini := 0; ini < n; ini += f { tareas <- … }',
      'close(tareas); wg.Wait()',
    ];
    const cod = codigo(s, 6.75, 1.6, 5.95, L, { tam: 13, paso: 18.5, rotulo: 'Go · go/paralelo.go (simplificado)', nombre: 'Código Go' });
    [[1, 2, 12.25], [2, 6, 12.25], [3, 11, 12.25], [4, 12, 11.85]].forEach(([n, li, x]) => marcador(s, n, x, cod.yLinea(li), C.accent2, 'Go'));
    // Tabla OpenMP vs Go
    const hdr = t => ({ text: t, options: { bold: true, color: C.background1, fill: { color: C.text2 }, align: 'center' } });
    const fila = (a, b, c, d, k) => [a, b, c, d].map((t, j) => ({ text: t, options: { bold: j === 0, color: j === 0 ? C.text2 : C.text1, align: j === 0 ? 'center' : 'left', fill: { color: k % 2 ? C.background1 : C.background2 } } }));
    s.addTable([
      [hdr(''), hdr('¿Quién crea los hilos?'), hdr('¿Cómo se reparte?'), hdr('¿Cómo se espera?')],
      fila('OpenMP', 'El compilador', 'Estático: trozos iguales', 'Barrera implícita', 0),
      fila('Go', 'Nosotros (go func)', 'Dinámico: el canal', 'sync.WaitGroup', 1),
    ], { x: 0.6, y: 5.4, w: 5.95, colW: [1.0, 1.75, 1.75, 1.45], rowH: [0.42, 0.42, 0.42], fontSize: 12, fontFace: THEME.bodyFontFace, border: { type: 'solid', pt: 0.75, color: 'D5DCE5' }, margin: [0, 0.08, 0, 0.08], valign: 'middle', objectName: 'Tabla OpenMP vs Go' });
    T(s, 'Mismo generador que en C: Go y C dan resultados idénticos.', { x: 6.75, y: 1.6 + cod.alto + 0.2, w: 5.95, h: 0.4, fontSize: 14, italic: true, color: C.text2, objectName: 'Go igual a C' });
    notas(s, 10);
  }

  // ================= 11 · RESULTADOS =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Semana 7' });
    s.addText('09 · S7 · RESULTADOS', { placeholder: 'kicker' });
    s.addText('Speedup medido con 2, 4 y 8 hilos', { placeholder: 'title' });
    tarjeta(s, 0.6, 1.6, 8.05, 5.1, 'Gráfico fondo');
    T(s, 'S(p) = T(1) / T(p)   ·   T(1) = la mejor versión secuencial', { x: 0.9, y: 1.75, w: 5.2, h: 0.35, fontSize: 14, bold: true, color: C.text2, objectName: 'Gráfico título' });
    const PS = [['p = 2', 60], ['p = 4', 30], ['p = 8', 0]];
    PS.forEach(([t, tr], k) => {
      const x = 6.25 + k * 0.78;
      s.addShape(pres.shapes.RECTANGLE, { x, y: 1.83, w: 0.2, h: 0.2, fill: { color: C.accent1, transparency: tr }, line: { color: C.accent1, width: 0, transparency: tr }, objectName: 'Leyenda ' + t });
      T(s, t, { x: x + 0.25, y: 1.75, w: 0.55, h: 0.35, fontSize: 12, valign: 'middle', objectName: 'Leyenda texto ' + t });
    });
    const G = [RES.denso, RES.ku, RES.simKU, RES.simTodo, RES.goDenso];
    const XB = 1.3, BASE = 5.75, UNI = 0.55, GW = 1.45;   // 6 unidades -> 3.3 in
    for (let v = 1; v <= 6; v++) {
      s.addShape(pres.shapes.LINE, { x: XB - 0.1, y: BASE - v * UNI, w: 7.15, h: 0, line: { color: C.text1, width: 0.5, transparency: 85 }, objectName: 'Rejilla ' + v });
      T(s, v + '×', { x: 0.75, y: BASE - v * UNI - 0.13, w: 0.4, h: 0.26, fontSize: 11, color: GRIS, align: 'right', objectName: 'Eje ' + v });
    }
    s.addShape(pres.shapes.LINE, { x: XB - 0.1, y: BASE, w: 7.15, h: 0, line: { color: C.text1, width: 1 }, objectName: 'Línea base' });
    G.forEach((g, i) => {
      const x0 = XB + i * GW;
      const col = g === RES.simKU ? C.accent2 : (g === RES.goDenso ? C.accent5 : C.accent1);
      g.s.forEach((v, k) => {
        const h = v * UNI, x = x0 + 0.08 + k * 0.42;
        s.addShape(pres.shapes.RECTANGLE, { x, y: BASE - h, w: 0.38, h, fill: { color: col, transparency: PS[k][1] }, line: { color: col, width: 0, transparency: PS[k][1] }, objectName: `${g.nombre} p${k} barra` });
        const dentro = g === RES.simKU;   // ahí arriba va la línea de Amdahl: la etiqueta va dentro de la barra
        T(s, v.toFixed(1).replace('.', ','), { x: x - 0.1, y: dentro ? BASE - h + 0.06 : BASE - h - 0.27, w: 0.58, h: 0.25, fontSize: 11, bold: k === 2, color: dentro ? C.background1 : C.text2, align: 'center', objectName: `${g.nombre} p${k} valor` });
      });
      T(s, g.nombre, { x: x0 - 0.05, y: BASE + 0.08, w: GW - 0.05, h: 0.55, fontSize: 12, color: C.text2, align: 'center', objectName: g.nombre + ' etiqueta' });
    });
    // Límite de Amdahl sobre "solo K·U"
    const xa = XB + 2 * GW;
    s.addShape(pres.shapes.LINE, { x: xa, y: BASE - RES.amdahl8 * UNI, w: 1.36, h: 0, line: { color: C.accent2, width: 1.5, dashType: 'dash' }, objectName: 'Límite Amdahl' });
    T(s, 'límite de Amdahl\ncon 8 hilos: 1,7×', { x: xa - 0.1, y: BASE - RES.amdahl8 * UNI - 0.5, w: 1.55, h: 0.45, fontSize: 11, italic: true, color: C.accent2, align: 'center', valign: 'bottom', objectName: 'Límite Amdahl texto' });
    T(s, 'i5-11400F (6 núcleos / 12 hilos) · mediana de 5 repeticiones', { x: 0.9, y: 6.38, w: 7.5, h: 0.26, fontSize: 11, color: GRIS, objectName: 'Nota máquina' });
    // Cifras
    const ST = [
      [RES.denso.s[2].toFixed(1).replace('.', ',') + '×', C.accent1, 'producto denso, 8 hilos', `eficiencia ${RES.eDenso8.toFixed(2).replace('.', ',')}`],
      [RES.simTodo.s[2].toFixed(1).replace('.', ',') + '×', C.accent3, 'simulador completo', `solo K·U: ${RES.simKU.s[2].toFixed(1).replace('.', ',')}× (Amdahl)`],
      [RES.goDenso.s[2].toFixed(1).replace('.', ',') + '×', C.accent5, 'Go, 8 goroutines', 'parte más lento: en tiempo empata con C'],
    ];
    ST.forEach(([num, col, tit, det], i) => {
      const y = 1.6 + i * 1.22;
      tarjeta(s, 8.85, y, 3.85, 1.07, tit + ' fondo');
      T(s, num, { x: 8.95, y, w: 1.4, h: 1.07, fontSize: 30, bold: true, color: col, align: 'center', valign: 'middle', objectName: tit + ' cifra' });
      T(s, tit, { x: 10.4, y: y + 0.15, w: 2.2, h: 0.36, fontSize: 15, bold: true, color: C.text2, objectName: tit + ' título' });
      T(s, det, { x: 10.4, y: y + 0.52, w: 2.25, h: 0.5, fontSize: 12, color: C.text1, objectName: tit + ' detalle' });
    });
    tarjeta(s, 8.85, 5.28, 3.85, 1.42, 'Hallazgo fondo', C.accent4, 75);
    T(s, [{ text: 'T(1) honesto: ', options: { bold: true, color: C.text2 } },
      { text: 'con 1 hilo, OpenMP ya ganaba 1,3× en K·U por limpiar cada fila en caché. Esa mejora se pasó también al secuencial.' }],
    { x: 9.05, y: 5.38, w: 3.5, h: 1.22, fontSize: 13, valign: 'middle', objectName: 'Hallazgo T1' });
    notas(s, 11);
  }

  // ================= 12 · CIERRE =================
  {
    pres.addSection({ title: 'Cierre' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Cierre' });
    s.addText('10 · CIERRE', { placeholder: 'kicker' });
    s.addText('Lo aprendido y lo que sigue', { placeholder: 'title' });
    tarjeta(s, 0.6, 1.6, 5.95, 3.95, 'Aprendido fondo');
    circuloIcono(s, ic.idea, 0.9, 1.8, 0.7, C.accent4, 'Aprendido');
    T(s, 'Lo aprendido', { x: 1.8, y: 1.8, w: 4.4, h: 0.7, fontSize: 22, bold: true, color: C.text2, valign: 'middle', objectName: 'Aprendido título' });
    const A = ['Primero correcto, después rápido: 121 pruebas automáticas', 'Medir contra la mejor versión secuencial', 'Amdahl manda: hay que paralelizar todo el paso', 'K·U lo limita la memoria, no la CPU'];
    T(s, A.map((t, k) => ({ text: t, options: { bullet: { indent: 16 }, breakLine: k < A.length - 1 } })),
      { x: 1.0, y: 2.75, w: 5.35, h: 2.6, fontSize: 17, paraSpaceAfter: 14, objectName: 'Aprendido lista' });
    tarjeta(s, 6.75, 1.6, 5.95, 3.95, 'Sigue fondo');
    circuloIcono(s, ic.cohete, 7.05, 1.8, 0.7, C.accent2, 'Sigue');
    T(s, 'Lo que sigue', { x: 7.95, y: 1.8, w: 4.4, h: 0.7, fontSize: 22, bold: true, color: C.text2, valign: 'middle', objectName: 'Sigue título' });
    const SIG = [
      ['S8', 'Hito H2: avance con demo en paralelo', C.accent2],
      ['S9–S11', 'MPI: Cannon, SUMMA y el edificio repartido', C.accent1],
      ['S12–S13', 'CUDA: K·U en la GPU (GTX 1660 SUPER)', C.accent6],
      ['S14–S16', 'Escalabilidad, escenarios e informe IEEE', C.accent5],
    ];
    SIG.forEach(([sem, t, col], k) => {
      const y = 2.75 + k * 0.67;
      T(s, sem, { x: 7.05, y, w: 1.2, h: 0.55, fontSize: 16, bold: true, color: col, valign: 'middle', objectName: sem + ' semana' });
      T(s, t, { x: 8.3, y, w: 4.25, h: 0.55, fontSize: 15, valign: 'middle', objectName: sem + ' texto' });
    });
    tarjeta(s, 0.6, 5.78, 12.1, 0.92, 'Frase final fondo', C.text2);
    T(s, [{ text: 'En una frase: ', options: { bold: true, color: C.accent4 } },
      { text: 'el mismo simulador, cada vez en más núcleos, siempre con el mismo resultado.', options: { color: C.background1 } }],
    { x: 0.9, y: 5.78, w: 11.6, h: 0.92, fontSize: 19, valign: 'middle', objectName: 'Frase final' });
    notas(s, 12);
  }

  // ================= 13 · GRACIAS =================
  {
    const s = pres.addSlide({ masterName: 'FINAL', sectionTitle: 'Cierre' });
    s.addText('MUCHAS GRACIAS', { placeholder: 'title' });
    notas(s, 13);
  }

  const out = process.argv[2] || 'Simulacion_Sismica.pptx';
  await pres.writeFile({ fileName: out });
  await applyTheme(out, THEME);
  console.log('ok', out);
})().catch(e => { console.error(e); process.exit(1); });
