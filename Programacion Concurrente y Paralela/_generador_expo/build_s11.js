// Presentación del avance S8–S11 (MPI, Hito H3). Mismo estilo que build.js (S7):
// portada oscura, láminas claras con rótulo + título, tarjetas, íconos en círculos.
// Los números salen de RES (abajo), medidos en DESKTOP-62M3KIP el 2026-11-05
// (matmul-paralelo/docs/GUIA_S9_S11.md, sección 6). Si se vuelve a medir, se
// editan ahí y se regenera:
//   node build_s11.js ../Exposicion/Avance_S8_S11_MPI.pptx
const path = require('path');
const pptxgen = require('pptxgenjs');
const React = require('react');
const ReactDOMServer = require('react-dom/server');
const sharp = require('sharp');
const fa = require('react-icons/fa');
const { applyTheme } = require(process.env.APPLY_THEME);

const CAPT = path.join(__dirname, '..', 'Exposicion', 'capturas_s11');

// ---- Mediciones (DESKTOP-62M3KIP, Core Ultra 5 235U, 14 hilos; mediana de 5) ----
const RES = {
  maquina: 'Laptop Core Ultra 5 235U (2 núcleos rápidos + 10 eficientes, 14 hilos)',
  denso1024: {     // S(p), T(1) = 0.298 s (seq_ikj)
    cannon: { 4: 2.58, 9: 3.56, 16: 2.32 }, summa: { 2: 1.90, 4: 2.63, 8: 3.57, 9: 3.55, 16: 2.06 },
    pasiva16: { cannon: 3.96, summa: 3.97 },
  },
  com2048: {       // [mensajes, MB] durante el cálculo, todos los procesos
    cannon: { 4: [12, 100.7], 9: [48, 179.1], 16: [120, 251.7] },
    summa: { 4: [8, 67.1], 9: [36, 134.3], 16: [96, 201.3] },
  },
  sim: {           // S(p) del simulador, reparto por nnz, 400 pasos, 8 sismos
    ps: [2, 4, 8, 9, 16],
    '100 × 400': [1.21, 2.32, 3.57, 3.71, 0.64],
    '50 × 200': [1.23, 2.04, 3.64, 3.96, 0.26],
    '20 × 50': [1.29, 2.19, 3.50, 3.38, 0.03],
    pasiva16: { '100 × 400': 2.88, '50 × 200': 3.04, '20 × 50': 1.64 },
    omp8: 2.06,    // sim_omp con 8 hilos, 100 × 400, misma laptop (15/10)
    comm8: { '100 × 400': 28, '20 × 50': 51 },
  },
  pruebas: [[1, 33], [2, 29], [4, 34], [6, 29], [9, 34], [16, 34]],
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
pres.title = 'Avance S8–S11: simulación sísmica con MPI';
pres.theme = { headFontFace: THEME.headFontFace, bodyFontFace: THEME.bodyFontFace };
const C = pres.SchemeColor;
const PROC = [C.accent1, C.accent5, C.accent4, C.accent6];   // color de cada proceso (0..3)
const PROC_HEX = [HEX.accent1, HEX.accent5, HEX.accent4, HEX.accent6];

const FOOT = 'Avance S8–S11 · Memoria distribuida con MPI   |   Programación Concurrente y Paralela · UNMSM-FISI · 2026-2';

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
  background: { color: C.text2 },
  objects: [
    { placeholder: { options: { name: 'title', type: 'title', x: 0.7, y: 0.6, w: 12, h: 1.0, fontSize: 40, bold: true, color: C.background1, valign: 'middle', align: 'left', margin: 0 }, text: '' } },
  ],
});

// ---------- utilidades (las mismas de build.js) ----------
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
  T(slide, String(n), { x, y, w: d, h: d, fontSize: d > 0.6 ? 26 : (d < 0.3 ? 11 : 14), bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: nombre + ' número' });
}
const ln = (slide, x1, y1, x2, y2, nombre, line) => slide.addShape(pres.shapes.LINE, {
  x: Math.min(x1, x2), y: Math.min(y1, y2), w: Math.abs(x2 - x1), h: Math.abs(y2 - y1),
  flipV: (y2 < y1) !== (x2 < x1), line: { color: C.text2, width: 1.5, ...line }, objectName: nombre,
});
const COD = { texto: 'E8EEF6', clave: 'F2B134', fn: '7FD1F7', coment: '8FA3BF' };
function codigo(slide, x, y, w, lineas, { tam = 13, paso = 18, rotulo = '', nombre = 'Código' } = {}) {
  const padX = 0.28, padTop = rotulo ? 0.5 : 0.22, alto = padTop + lineas.length * paso / 72 + 0.2;
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h: alto, rectRadius: 0.1, fill: { color: C.text2 }, line: { color: C.text2, width: 0 }, objectName: nombre + ' fondo' });
  if (rotulo) T(slide, rotulo, { x: x + padX, y: y + 0.15, w: w - 2 * padX, h: 0.26, fontSize: 11, color: COD.coment, fontFace: MONO, objectName: nombre + ' rótulo' });
  const runs = [];
  lineas.forEach((l, i) => {
    const com = l.indexOf('//');
    const parts = [];
    const body = com >= 0 ? l.slice(0, com) : l;
    body.split(/(\bMPI_\w+\b)/).forEach(p => { if (p) parts.push({ text: p, options: { color: /^MPI_/.test(p) ? COD.fn : COD.texto } }); });
    if (com >= 0) parts.push({ text: l.slice(com), options: { color: COD.coment, italic: true } });
    if (!parts.length) parts.push({ text: ' ', options: { color: COD.texto } });
    if (i < lineas.length - 1) parts[parts.length - 1].options.breakLine = true;
    runs.push(...parts);
  });
  T(slide, runs, { x: x + padX, y: y + padTop, w: w - 2 * padX, h: lineas.length * paso / 72 + 0.05, fontSize: tam, fontFace: MONO, lineSpacing: paso, objectName: nombre });
  return alto;
}
function captura(slide, archivo, x, y, w, nombre, recorte) {
  // recorte = { y0, y1, x0, x1 }: fracción de la imagen que se muestra (una parte de la consola).
  // w es el ancho de la parte visible; devuelve su alto.
  const buf = require('fs').readFileSync(path.join(CAPT, archivo));
  const W = buf.readUInt32BE(16), H = buf.readUInt32BE(20);
  const r = Object.assign({ y0: 0, y1: 1, x0: 0, x1: 1 }, recorte || {});
  const fullW = w / (r.x1 - r.x0), fullH = fullW * H / W;
  const h = fullH * (r.y1 - r.y0);
  slide.addImage({ path: path.join(CAPT, archivo), x, y, w: fullW, h: fullH,
    sizing: { type: 'crop', x: fullW * r.x0, y: fullH * r.y0, w, h }, objectName: nombre });
  slide.addShape(pres.shapes.RECTANGLE, { x, y, w, h, fill: { color: C.background1, transparency: 100 }, line: { color: C.text1, width: 0.75, transparency: 50 }, objectName: nombre + ' marco' });
  return h;
}

(async () => {
  const ic = {
    check: await icono(fa.FaCheck), red: await icono(fa.FaNetworkWired), capas: await icono(fa.FaLayerGroup),
    bolt: await icono(fa.FaBolt), cpu: await icono(fa.FaMicrochip), edificio: await icono(fa.FaBuilding),
    exchange: await icono(fa.FaExchangeAlt), equals: await icono(fa.FaEquals), clock: await icono(fa.FaHourglassHalf),
    arrow: await icono(fa.FaArrowRight, HEX.dk2), rocket: await icono(fa.FaRocket), balance: await icono(fa.FaBalanceScale),
    terminal: await icono(fa.FaTerminal), checkV: await icono(fa.FaCheck, HEX.accent3),
  };

  // ================= 1 · PORTADA =================
  {
    pres.addSection({ title: 'Inicio' });
    const s = pres.addSlide({ masterName: 'PORTADA', sectionTitle: 'Inicio' });
    T(s, 'UNIVERSIDAD NACIONAL MAYOR DE SAN MARCOS', { x: 0.7, y: 0.55, w: 7.5, h: 0.3, fontSize: 12, color: C.accent4, charSpacing: 3, objectName: 'Universidad' });
    T(s, 'Facultad de Ingeniería de Sistemas e Informática', { x: 0.7, y: 0.85, w: 7.5, h: 0.3, fontSize: 12, color: C.background1, objectName: 'Facultad' });
    s.addText([
      { text: 'Avance S8 a S11:', options: { breakLine: true } },
      { text: 'MEMORIA DISTRIBUIDA', options: { fontSize: 50, bold: true, color: C.accent4, breakLine: true } },
      { text: 'con MPI', options: { fontSize: 36 } },
    ], { placeholder: 'title' });
    T(s, 'Simulación paralela de la respuesta sísmica · Hito H3', { x: 0.7, y: 4.25, w: 7.7, h: 0.45, fontSize: 18, italic: true, color: C.background1, objectName: 'Lema' });
    T(s, [
      { text: 'Curso: ', options: { color: C.accent4 } }, { text: 'Programación Concurrente y Paralela · Proyecto 2', options: { breakLine: true } },
      { text: 'Docente: ', options: { color: C.accent4 } }, { text: '[nombre del docente]', options: { breakLine: true } },
      { text: 'Equipo: ', options: { color: C.accent4 } }, { text: 'Maylle Poma, Fran Diego · [integrantes]' },
    ], { x: 0.7, y: 5.95, w: 7.5, h: 0.85, fontSize: 14, color: C.background1, objectName: 'Datos del curso' });
    // Emblema: malla 3x3 de procesos
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) {
      const x = 9.0 + j * 1.2, y = 1.45 + i * 1.2;
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w: 1.0, h: 1.0, rectRadius: 0.1, fill: { color: (i + j) % 2 ? C.accent1 : C.accent5, transparency: 20 }, line: { color: C.background1, width: 0 }, objectName: `Emblema P${i}${j}` });
      T(s, `P${i * 3 + j}`, { x, y, w: 1.0, h: 1.0, fontSize: 20, bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: `Emblema P${i}${j} texto` });
    }
    T(s, '9 procesos, 9 memorias:\nlo que no tengo, me lo mandan', { x: 8.6, y: 5.25, w: 4.4, h: 0.8, fontSize: 14, color: C.background1, align: 'center', objectName: 'Emblema texto' });
    s.addNotes([
      'IDEA: Presentar el avance de las semanas 8 a 11: pasamos de memoria compartida (OpenMP) a memoria distribuida (MPI) y cumplimos el Hito H3.',
      '',
      'QUÉ DECIR: "En S7 mostramos OpenMP: varios hilos que comparten la memoria. Hoy mostramos el siguiente nivel: varios PROCESOS, cada uno con su propia memoria, que solo se comunican con mensajes. Es como funciona un clúster."',
      '• Señala la malla de 9 procesos: cada cuadro es un proceso con su memoria.',
      '',
      'Tiempo sugerido: 0.5 min',
    ].join('\n'));
  }

  // ================= 2 · DÓNDE ESTAMOS =================
  {
    pres.addSection({ title: 'Avance' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Avance' });
    s.addText('01 · DÓNDE ESTAMOS', { placeholder: 'kicker' });
    s.addText('Tres hitos cumplidos: H1, H2 y ahora H3', { placeholder: 'title' });
    const F = [
      ['S5–S6', 'Fundamentos', 'H1', 'hecho', C.accent3],
      ['S7–S8', 'Memoria compartida', 'H2', 'hecho', C.accent3],
      ['S9–S11', 'Memoria distribuida', 'H3', 'ahora', C.accent2],
      ['S12–S13', 'MPI + CUDA y clúster', 'H4', '', C.accent6],
      ['S14–S16', 'Evaluación', 'H5', '', C.accent5],
    ];
    ln(s, 0.9, 2.55, 12.4, 2.55, 'Eje', { color: C.text1, width: 1.5, transparency: 60 });
    F.forEach(([sem, fase, hito, estado, col], i) => {
      const cx = 1.75 + i * 2.45;
      if (estado === 'ahora') {
        s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: cx - 0.85, y: 1.5, w: 1.7, h: 0.3, rectRadius: 0.08, fill: { color: C.accent2 }, line: { color: C.accent2, width: 0 }, objectName: 'Hoy fondo' });
        T(s, 'HOY', { x: cx - 0.85, y: 1.5, w: 1.7, h: 0.3, fontSize: 11, bold: true, color: C.background1, align: 'center', valign: 'middle', charSpacing: 2, objectName: 'Hoy' });
      }
      T(s, sem, { x: cx - 1.1, y: 1.85, w: 2.2, h: 0.45, fontSize: 22, bold: true, color: C.text2, align: 'center', valign: 'bottom', objectName: sem });
      const d = estado === 'ahora' ? 0.42 : 0.32;
      s.addShape(pres.shapes.OVAL, { x: cx - d / 2, y: 2.55 - d / 2, w: d, h: d, fill: { color: estado ? col : C.background2 }, line: { color: estado ? col : C.text1, width: 1.5, transparency: estado ? 0 : 50 }, objectName: sem + ' punto' });
      T(s, fase, { x: cx - 1.1, y: 2.9, w: 2.2, h: 0.35, fontSize: 14, color: C.text2, align: 'center', objectName: sem + ' fase' });
      T(s, (estado ? '✔ ' : '') + hito, { x: cx - 1.1, y: 3.22, w: 2.2, h: 0.35, fontSize: 14, bold: true, color: estado ? col : GRIS, align: 'center', objectName: sem + ' hito' });
    });
    const stats = [
      ['34', C.accent3, 'pruebas OK con 16 procesos', 'La suite MPI pasa con 1, 2, 4, 6, 9 y 16 procesos'],
      ['0', C.accent1, 'bits distintos', 'El simulador con MPI da exactamente lo mismo que el secuencial'],
      ['4.0×', C.accent2, 'más rápido con 9 procesos', 'Simulador de un edificio de 50 × 200 (10 050 nodos)'],
      ['2', C.accent6, 'algoritmos densos', 'Cannon (S9) y SUMMA (S10), con mensajes y bytes medidos'],
    ];
    stats.forEach(([num, col, tit, det], i) => {
      const x = 0.6 + i * 3.08;
      tarjeta(s, x, 4.0, 2.86, 2.65, tit + ' fondo');
      T(s, num, { x: x + 0.2, y: 4.15, w: 2.46, h: 0.95, fontSize: 44, bold: true, color: col, valign: 'middle', objectName: tit + ' cifra' });
      T(s, tit, { x: x + 0.2, y: 5.12, w: 2.46, h: 0.62, fontSize: 16, bold: true, color: C.text2, objectName: tit + ' título' });
      T(s, det, { x: x + 0.2, y: 5.75, w: 2.46, h: 0.8, fontSize: 13, objectName: tit + ' detalle' });
    });
    s.addNotes([
      'IDEA: Ubicar al profesor en el cronograma y adelantar los 4 resultados de hoy.',
      '',
      'QUÉ DECIR: "H1 fue el simulador validado y H2 OpenMP con la demo. Hoy cerramos la fase de memoria distribuida, semanas 9 a 11, con el Hito H3: la simulación con MPI da los mismos desplazamientos que la secuencial con 1, 4, 9 y 16 procesos. Por ahora todo corre en una sola laptop; en S12 y S13, además del híbrido con CUDA, lo llevamos a un clúster de varias computadoras."',
      '• Recorre las 4 cifras de izquierda a derecha: verificación, exactitud, rapidez y los dos algoritmos del caso denso.',
      '• La de S8 (informe de avance y diseño del reparto) la cubre la siguiente lámina.',
      '',
      'Tiempo sugerido: 1 min',
    ].join('\n'));
  }

  // ================= 3 · S8 DISEÑO DEL REPARTO =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Avance' });
    s.addText('02 · SEMANA 8 · DISEÑO DEL REPARTO', { placeholder: 'kicker' });
    s.addText('Antes de programar: qué guarda cada proceso', { placeholder: 'title' });
    // Panel denso
    tarjeta(s, 0.6, 1.6, 5.95, 5.1, 'Denso panel');
    T(s, 'Matrices densas: bloques en una malla 2D', { x: 0.9, y: 1.78, w: 5.4, h: 0.4, fontSize: 18, bold: true, color: C.text2, objectName: 'Denso título' });
    const G0x = 1.0, G0y = 2.4, cell = 0.82;
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) {
      s.addShape(pres.shapes.RECTANGLE, { x: G0x + j * cell, y: G0y + i * cell, w: cell - 0.06, h: cell - 0.06, fill: { color: [C.accent1, C.accent5, C.accent6][(i + j) % 3], transparency: 15 }, line: { color: C.background1, width: 0 }, objectName: `Bloque ${i}${j}` });
      T(s, `A${i}${j}`, { x: G0x + j * cell, y: G0y + i * cell, w: cell - 0.06, h: cell - 0.06, fontSize: 14, bold: true, color: C.background1, align: 'center', valign: 'middle', objectName: `Bloque ${i}${j} texto` });
    }
    T(s, [
      { text: 'El proceso (i, j) guarda el bloque (i, j) de A, B y C.', options: { breakLine: true } },
      { text: 'Necesita una fila de bloques de A y una columna de B: ', options: {} },
      { text: '2n²/√p datos', options: { bold: true } }, { text: ' en vez de n² con un reparto por filas.' },
    ], { x: 3.65, y: 2.4, w: 2.75, h: 2.4, fontSize: 14, paraSpaceAfter: 6, objectName: 'Denso explicación' });
    T(s, [{ text: 'Si n no es múltiplo de q: ', options: { bold: true, color: C.text2 } }, { text: 'se rellena con ceros (no cambia C).' }],
      { x: 0.9, y: 5.05, w: 5.4, h: 0.6, fontSize: 14, objectName: 'Denso relleno' });
    T(s, [{ text: 'Cannon (S9) ', options: { bold: true, color: C.accent1 } }, { text: 'y ' }, { text: 'SUMMA (S10) ', options: { bold: true, color: C.accent2 } }, { text: 'usan este reparto.' }],
      { x: 0.9, y: 5.75, w: 5.4, h: 0.6, fontSize: 15, objectName: 'Denso algoritmos' });
    // Panel edificio
    tarjeta(s, 6.75, 1.6, 5.95, 5.1, 'Edificio panel');
    T(s, 'Edificio: franjas de pisos', { x: 7.05, y: 1.78, w: 5.4, h: 0.4, fontSize: 18, bold: true, color: C.text2, objectName: 'Edificio título' });
    const B0x = 7.25, B0y = 2.35, fh = 0.34, nodos = 5, dx = 0.48;
    const franja = p => (p >= 9 ? 3 : p >= 6 ? 2 : p >= 3 ? 1 : 0);
    for (let piso = 10; piso >= 1; piso--) {
      const y = B0y + (10 - piso) * fh, q = franja(piso);
      s.addShape(pres.shapes.RECTANGLE, { x: B0x - 0.1, y: y - 0.02, w: (nodos - 1) * dx + 0.32, h: fh, fill: { color: PROC[q], transparency: 80 }, line: { color: PROC[q], width: 0, transparency: 100 }, objectName: `Franja piso ${piso}` });
      ln(s, B0x + 0.06, y + 0.12, B0x + 0.06 + (nodos - 1) * dx, y + 0.12, `Losa ${piso}`, { color: C.text1, width: 1 });
      for (let c = 0; c < nodos; c++) {
        const fantasma = [3, 6, 9].includes(piso + 1) || [3, 6, 9].includes(piso);
        s.addShape(pres.shapes.OVAL, { x: B0x + c * dx, y: y + 0.06, w: 0.12, h: 0.12, fill: { color: PROC[q] }, line: { color: C.background1, width: 0.5 }, objectName: `Nodo ${piso}-${c}` });
      }
    }
    [[0, '1–2'], [1, '3–5'], [2, '6–8'], [3, '9–10']].forEach(([q, pisos]) => {
      const yTop = { 0: 8, 1: 5, 2: 2, 3: 0 }[q], n = { 0: 2, 1: 3, 2: 3, 3: 2 }[q];
      T(s, `proceso ${q}: pisos ${pisos}`, { x: B0x + (nodos - 1) * dx + 0.35, y: B0y + yTop * fh + (n * fh - 0.3) / 2, w: 2.0, h: 0.3, fontSize: 12, bold: true, color: PROC[q], valign: 'middle', objectName: `Etiqueta proceso ${q}` });
    });
    [6, 2].forEach(piso => {   // fantasmas del proceso 1: el piso de arriba (6) y el de abajo (2)
      const y = B0y + (10 - piso) * fh;
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: B0x - 0.08, y: y + 0.0, w: (nodos - 1) * dx + 0.28, h: 0.24, rectRadius: 0.05, fill: { color: C.accent2, transparency: 100 }, line: { color: C.accent2, width: 1.75, dashType: 'dash' }, objectName: `Fantasmas piso ${piso}` });
    });
    ln(s, B0x - 0.2, B0y + 10 * fh + 0.02, B0x + (nodos - 1) * dx + 0.32, B0y + 10 * fh + 0.02, 'Suelo', { color: C.text1, width: 3 });
    T(s, [
      { text: 'Cada nodo solo depende de sus vecinos. Para su K·U, el proceso 1 necesita ', options: {} },
      { text: 'el piso de abajo y el de arriba', options: { bold: true } },
      { text: ': sus ' }, { text: 'nodos fantasma', options: { bold: true, color: C.accent2 } }, { text: ' (recuadros rojos).' },
    ], { x: 7.05, y: 5.9, w: 5.45, h: 0.7, fontSize: 14, objectName: 'Edificio explicación' });
    s.addNotes([
      'IDEA: S8 tenía como tarea diseñar el reparto. En memoria distribuida lo primero es decidir qué datos tiene cada proceso, porque lo que no tiene hay que mandárselo.',
      '',
      '• Panel izquierdo (denso): la malla 2D. "Cada proceso guarda un bloque. Para calcular su bloque de C necesita una fila de bloques de A y una columna de B. Con 16 procesos eso es 4 veces menos datos que repartir por filas."',
      '• Panel derecho (edificio): las franjas de colores. "La matriz K es dispersa: cada nodo solo se conecta con sus vecinos. Si a cada proceso le doy una franja de pisos, solo necesita la fila de nodos de abajo y la de arriba: los nodos fantasma."',
      '',
      'SI TE PREGUNTAN por qué no se reparte el edificio en 2D: con ≤ 16 procesos y 100 pisos, cada franja tiene al menos 6 pisos; el 2D complica el código y queda como mejora (está en docs/DISENO_REPARTO_MPI.md).',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 4 · S9 CANNON =================
  {
    pres.addSection({ title: 'Caso denso' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Caso denso' });
    s.addText('03 · SEMANA 9 · CANNON', { placeholder: 'kicker' });
    s.addText('Cannon: los bloques rotan entre vecinos', { placeholder: 'title' });
    T(s, 'Malla 3 × 3 después del sesgo inicial', { x: 0.6, y: 1.62, w: 5.4, h: 0.35, fontSize: 15, bold: true, color: C.text2, objectName: 'Malla título' });
    const g0x = 0.75, g0y = 2.1, cw = 1.62, chh = 1.15;
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) {
      const k = (i + j) % 3;
      tarjeta(s, g0x + j * cw, g0y + i * chh, cw - 0.12, chh - 0.12, `Proceso ${i}${j}`, C.background2);
      T(s, `(${i},${j})`, { x: g0x + j * cw + 0.1, y: g0y + i * chh + 0.06, w: 1.2, h: 0.28, fontSize: 11, color: GRIS, objectName: `Proceso ${i}${j} coord` });
      T(s, [{ text: `A${i}${k}`, options: { color: C.accent1, bold: true } }, { text: ' · ', options: {} }, { text: `B${k}${j}`, options: { color: C.accent2, bold: true } }],
        { x: g0x + j * cw, y: g0y + i * chh + 0.35, w: cw - 0.12, h: 0.5, fontSize: 18, align: 'center', valign: 'middle', objectName: `Proceso ${i}${j} bloques` });
    }
    T(s, '← A rota', { x: g0x + 3 * cw + 0.0, y: g0y + 1.5 * chh - 0.17, w: 1.0, h: 0.3, fontSize: 13, bold: true, color: C.accent1, objectName: 'A rota' });
    T(s, 'B rota ↑', { x: g0x + 1.5 * cw - 0.6, y: g0y + 3 * chh + 0.02, w: 1.2, h: 0.3, fontSize: 13, bold: true, color: C.accent2, align: 'center', objectName: 'B rota' });
    T(s, 'Los índices internos coinciden: cada proceso ya puede multiplicar.', { x: 0.75, y: 5.95, w: 5.6, h: 0.6, fontSize: 14, italic: true, color: C.text2, objectName: 'Malla nota' });
    // Pasos
    const P = [
      ['Sesgo inicial', 'La fila i de A rota i lugares a la izquierda; la columna j de B, j lugares hacia arriba.'],
      ['Multiplicar', 'C_local += A_local · B_local (producto ikj del bloque).'],
      ['Rotar', 'A una posición a la izquierda y B una hacia arriba. Repetir q veces.'],
    ];
    P.forEach(([t, d], i) => {
      const y = 1.65 + i * 0.98;
      numero(s, i + 1, 6.85, y + 0.05, 0.5, [C.accent5, C.accent1, C.accent2][i], t);
      T(s, t, { x: 7.5, y, w: 5.2, h: 0.35, fontSize: 17, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, d, { x: 7.5, y: y + 0.36, w: 5.2, h: 0.55, fontSize: 14, objectName: t + ' detalle' });
    });
    codigo(s, 6.85, 4.65, 5.85, [
      'shift(a, grid, 1, -fila);      // sesgo de A',
      'shift(b, grid, 0, -columna);   // sesgo de B',
      'for (s = 0; s < q; s++) {',
      '  local_gemm(a, b, c);',
      '  shift(a, grid, 1, -1);  shift(b, grid, 0, -1);',
      '}   // shift = MPI_Sendrecv_replace',
    ], { rotulo: 'src/mpi/cannon.c (simplificado)', nombre: 'Código Cannon', tam: 12, paso: 16 });
    s.addNotes([
      'IDEA: Explicar Cannon con la malla: después del sesgo inicial, cada proceso tiene un bloque de A y uno de B cuyos índices internos coinciden.',
      '',
      '• Malla: lee una casilla, por ejemplo (1,2): tiene A10 y B02. "El 0 de en medio coincide: puede multiplicar."',
      '• Pasos 1-3: sesgo, multiplicar, rotar; q veces.',
      '• Código: "MPI_Sendrecv_replace manda y recibe en la misma llamada sobre el mismo búfer: todos rotan a la vez, sin interbloqueos y sin memoria extra."',
      '',
      'SI TE PREGUNTAN: ¿Funciona con cualquier p? No: necesita p = q² (1, 4, 9, 16) y una malla periódica (MPI_Cart_create con periods = 1). Por eso en S10 hicimos SUMMA.',
      'Verificación: dentro de ε = 1e-9 contra el producto naive (error real ~1e-15: cambia el orden de las sumas).',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 5 · S10 SUMMA =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Caso denso' });
    s.addText('04 · SEMANA 10 · SUMMA', { placeholder: 'kicker' });
    s.addText('SUMMA: difundir en vez de rotar', { placeholder: 'title' });
    const g0x = 0.75, g0y = 1.85, cw = 1.25, chh = 1.0;
    T(s, 'Paso t = 1 en una malla 3 × 3', { x: 0.6, y: 1.45, w: 5, h: 0.32, fontSize: 15, bold: true, color: C.text2, objectName: 'SUMMA malla título' });
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) {
      const duenoA = j === 1, duenoB = i === 1;
      const col = duenoA && duenoB ? C.accent6 : duenoA ? C.accent1 : duenoB ? C.accent2 : C.background2;
      tarjeta(s, g0x + j * cw, g0y + i * chh, cw - 0.12, chh - 0.12, `S proceso ${i}${j}`, col, duenoA || duenoB ? 15 : 0);
      T(s, `(${i},${j})`, { x: g0x + j * cw, y: g0y + i * chh, w: cw - 0.12, h: chh - 0.12, fontSize: 14, bold: duenoA || duenoB, color: duenoA || duenoB ? C.background1 : C.text1, align: 'center', valign: 'middle', objectName: `S proceso ${i}${j} texto` });
    }
    T(s, [
      { text: '■ ', options: { color: C.accent1 } }, { text: 'columna 1: difunde su panel de A a toda su fila', options: { breakLine: true } },
      { text: '■ ', options: { color: C.accent2 } }, { text: 'fila 1: difunde su panel de B a toda su columna', options: { breakLine: true } },
      { text: '■ ', options: { color: C.accent6 } }, { text: '(1,1) hace las dos cosas' },
    ], { x: 0.75, y: 4.95, w: 4.9, h: 0.8, fontSize: 13, objectName: 'SUMMA leyenda' });
    codigo(s, 0.6, 5.8, 5.05, [
      'MPI_Bcast(panelA, ..., dueno_A, fila);',
      'MPI_Bcast(panelB, ..., dueno_B, columna);',
      'local_gemm(panelA, panelB, c);',
    ], { nombre: 'Código SUMMA', tam: 12, paso: 16 });
    // tabla comparativa cualitativa
    const filas = [
      ['', 'Cannon', 'SUMMA'],
      ['Procesos', 'p = q² (1, 4, 9, 16)', 'cualquier p (2, 6, 8...)'],
      ['Malla', 'q × q periódica', 'pr × pc'],
      ['Mensajes', 'punto a punto con vecinos', 'difusión en árbol (MPI_Bcast)'],
      ['Sesgo inicial', 'sí (mensajes extra)', 'no'],
      ['Memoria extra', 'ninguna', 'dos paneles'],
      ['Verificado', '✔ 1, 4, 9 y 16', '✔ 1, 2, 4, 6, 9 y 16'],
    ];
    s.addTable(filas.map((f, r) => f.map((c, k) => ({
      text: c, options: {
        bold: r === 0 || k === 0, color: r === 0 ? HEX.lt1 : (k === 0 ? HEX.dk2 : HEX.dk1),
        fill: { color: r === 0 ? (k === 1 ? HEX.accent1 : k === 2 ? HEX.accent2 : HEX.dk2) : (r % 2 ? HEX.lt2 : 'FFFFFF') },
        fontSize: 14, valign: 'middle', margin: [0.04, 0.1, 0.04, 0.1],
      },
    }))), { x: 6.0, y: 1.55, w: 6.7, colW: [1.7, 2.4, 2.6], rowH: 0.52, border: { type: 'solid', color: 'FFFFFF', pt: 1 }, objectName: 'Tabla Cannon SUMMA' });
    T(s, [{ text: 'SUMMA es el algoritmo de bibliotecas como ScaLAPACK: ', options: { bold: true, color: C.text2 } }, { text: 'no exige una malla cuadrada.' }],
      { x: 6.0, y: 5.4, w: 6.7, h: 0.6, fontSize: 15, objectName: 'SUMMA nota' });
    s.addNotes([
      'IDEA: SUMMA calcula lo mismo que Cannon pero los bloques no cambian de dueño: en cada paso alguien DIFUNDE un panel a su fila o a su columna.',
      '',
      '• Malla: "En el paso 1, la columna 1 (azul) manda su panel de A a toda su fila, y la fila 1 (naranja) su panel de B a toda su columna. Cada proceso suma A_panel·B_panel a su C."',
      '• Tabla: las diferencias. Recalca la primera fila: SUMMA funciona con 2, 6 u 8 procesos; Cannon no.',
      '',
      'SI TE PREGUNTAN por qué MPI_Bcast es eficiente: MPI lo hace en árbol (log p pasos), no mandando uno por uno.',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 6 · S10 TABLA COMPARATIVA =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Caso denso' });
    s.addText('05 · SEMANA 10 · TABLA COMPARATIVA DEL CASO DENSO', { placeholder: 'kicker' });
    s.addText('Mismo tiempo, pero Cannon mueve más datos', { placeholder: 'title' });
    const d = RES.denso1024, c = RES.com2048;
    const head = ['p', 'Malla', 'Cannon S(p)', 'SUMMA S(p)', 'Cannon: mensajes / MB', 'SUMMA: mensajes / MB'];
    const rows = [
      ['2', '1×2', '—', d.summa[2].toFixed(2), '—', '—'],
      ['4', '2×2', d.cannon[4].toFixed(2), d.summa[4].toFixed(2), `${c.cannon[4][0]} / ${c.cannon[4][1].toFixed(0)}`, `${c.summa[4][0]} / ${c.summa[4][1].toFixed(0)}`],
      ['8', '2×4', '—', d.summa[8].toFixed(2), '—', '—'],
      ['9', '3×3', d.cannon[9].toFixed(2), d.summa[9].toFixed(2), `${c.cannon[9][0]} / ${c.cannon[9][1].toFixed(0)}`, `${c.summa[9][0]} / ${c.summa[9][1].toFixed(0)}`],
      ['16*', '4×4', d.pasiva16.cannon.toFixed(2), d.pasiva16.summa.toFixed(2), `${c.cannon[16][0]} / ${c.cannon[16][1].toFixed(0)}`, `${c.summa[16][0]} / ${c.summa[16][1].toFixed(0)}`],
    ];
    s.addTable([head, ...rows].map((f, r) => f.map((v, k) => ({
      text: v, options: {
        bold: r === 0 || k === 0, color: r === 0 ? HEX.lt1 : HEX.dk1, align: k < 2 ? 'center' : 'right',
        fill: { color: r === 0 ? HEX.dk2 : (r % 2 ? HEX.lt2 : 'FFFFFF') }, fontSize: r === 0 ? 14 : 15, valign: 'middle', margin: [0.04, 0.12, 0.04, 0.12],
      },
    }))), { x: 0.6, y: 1.55, w: 8.1, colW: [0.6, 0.8, 1.4, 1.4, 1.95, 1.95], rowH: 0.5, border: { type: 'solid', color: 'FFFFFF', pt: 1 }, objectName: 'Tabla comparativa' });
    T(s, [
      { text: 'S(p) con n = 1024 (T(1) = 0.298 s). Mensajes y MB recibidos durante el cálculo con n = 2048, sumando todos los procesos.', options: { breakLine: true } },
      { text: '* 16 procesos en una laptop de 14 hilos, con espera pasiva (lámina 10).' },
    ], { x: 0.6, y: 4.7, w: 8.1, h: 0.7, fontSize: 12, color: GRIS, objectName: 'Tabla nota' });
    tarjeta(s, 0.6, 5.55, 8.1, 1.1, 'Fórmula fondo');
    T(s, [{ text: 'Lo medido = la fórmula. ', options: { bold: true, color: C.text2 } },
      { text: 'Mensajes con malla q × q: SUMMA 2·p·(q−1); Cannon lo mismo + el sesgo inicial. Con p = 16: 2·16·3 = 96 y 96 + 24 = 120.' }],
    { x: 0.85, y: 5.55, w: 7.65, h: 1.1, fontSize: 14, valign: 'middle', objectName: 'Fórmula' });
    const ins = [
      [ic.clock, 'Casi el mismo tiempo', 'Con malla cuadrada los dos hacen q pasos con bloques del mismo tamaño.', C.accent1],
      [ic.exchange, 'Cannon: +25 a +50 % de datos', 'Por el sesgo inicial: 24 mensajes extra con 16 procesos (120 contra 96, como predice la fórmula).', C.accent2],
      [ic.capas, 'SUMMA corre con 2 y 8', 'Con 8 procesos (malla 2×4) llega a 3.57×.', C.accent3],
    ];
    ins.forEach(([img, t, dd, col], i) => {
      const y = 1.55 + i * 1.75;
      circuloIcono(s, img, 9.05, y, 0.75, col, t);
      T(s, t, { x: 9.95, y, w: 2.8, h: 0.36, fontSize: 16, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, dd, { x: 9.95, y: y + 0.42, w: 2.8, h: 1.1, fontSize: 13, objectName: t + ' detalle' });
    });
    s.addNotes([
      'IDEA: El entregable de S10 es esta tabla: Cannon contra SUMMA en tiempo, mensajes y bytes.',
      '',
      '• Columnas de S(p): "Con malla cuadrada tardan prácticamente lo mismo: 3.56 contra 3.55 con 9 procesos."',
      '• Columnas de mensajes: "Cannon recibe más datos por el sesgo inicial. Con 16 procesos son 120 mensajes contra 96: 2·p·(q−1) = 96 en los dos, más 24 del sesgo en Cannon. Lo medido coincide con la fórmula."',
      '• "SUMMA además corre con 2 y 8 procesos."',
      '• La captura es la salida real del programa de medición.',
      '',
      'SI TE PREGUNTAN por qué el speedup no llega a p: esta laptop tiene 2 núcleos rápidos y 10 eficientes; con 9 procesos casi todos van en núcleos lentos y comparten la RAM. Además el tiempo incluye repartir A y B desde el proceso 0 y juntar C.',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 7 · S11 SIMULACIÓN DISTRIBUIDA =================
  {
    pres.addSection({ title: 'Simulador' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Simulador' });
    s.addText('06 · SEMANA 11 · SIMULACIÓN DISTRIBUIDA', { placeholder: 'kicker' });
    s.addText('Cada proceso avanza su franja de pisos', { placeholder: 'title' });
    const pasos = [
      ['Actualizar U', 'U(n+1) de MIS nodos: solo datos locales', C.accent1],
      ['Intercambiar fronteras', 'MPI_Isend / MPI_Irecv con el vecino de abajo y el de arriba', C.accent2],
      ['Picos y deriva', 'La deriva usa el nodo de abajo (puede ser fantasma)', C.accent5],
      ['K·U local', 'Mis filas de K, con los fantasmas recién recibidos', C.accent6],
    ];
    T(s, 'Un paso de tiempo, en cada proceso', { x: 0.6, y: 1.55, w: 6.5, h: 0.35, fontSize: 16, bold: true, color: C.text2, objectName: 'Paso título' });
    pasos.forEach(([t, dd, col], i) => {
      const y = 2.05 + i * 1.08;
      tarjeta(s, 0.6, y, 6.3, 0.92, t + ' fondo');
      numero(s, i + 1, 0.8, y + 0.2, 0.52, col, t);
      T(s, t, { x: 1.5, y: y + 0.1, w: 5.2, h: 0.34, fontSize: 16, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, dd, { x: 1.5, y: y + 0.45, w: 5.3, h: 0.4, fontSize: 13, objectName: t + ' detalle' });
    });
    T(s, [{ text: 'Un solo intercambio por paso ', options: { bold: true, color: C.accent2 } }, { text: 'sirve para la deriva y para el K·U siguiente.' }],
      { x: 0.6, y: 6.4, w: 6.3, h: 0.35, fontSize: 14, objectName: 'Un intercambio' });
    // tabla de la demo
    const hc7 = captura(s, 'c04_demo_reparto.png', 7.2, 1.6, 5.5, 'Captura reparto', { y0: 0.80, y1: 0.975, x0: 0, x1: 0.8 });
    T(s, 'Salida real de la demo con 4 procesos (10 pisos × 4 vanos)', { x: 7.2, y: 1.6 + hc7 + 0.08, w: 5.5, h: 0.3, fontSize: 11, color: GRIS, objectName: 'Captura reparto pie' });
    const ideas = [
      ['Franjas contiguas', 'Proceso 1: nodos 13 a 24 (pisos 3 a 5). 10 fantasmas, 2 vecinos.'],
      ['Balanceo por nnz', 'Por filas: 50 y 60 nnz (desbalance 1.09). Por nnz: 55 cada uno (1.00).'],
      ['Poco tráfico', 'Siempre 2 filas de nodos por proceso y por paso, sea cual sea el tamaño del edificio.'],
    ];
    ideas.forEach(([t, dd], i) => {
      const y = 3.3 + i * 1.12;
      s.addImage({ data: ic.checkV, x: 7.25, y: y + 0.05, w: 0.3, h: 0.3, objectName: t + ' check' });
      T(s, t, { x: 7.7, y, w: 5.0, h: 0.35, fontSize: 16, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, dd, { x: 7.7, y: y + 0.37, w: 5.0, h: 0.65, fontSize: 13, objectName: t + ' detalle' });
    });
    s.addNotes([
      'IDEA: Cómo funciona el simulador con MPI: cada proceso tiene su franja de pisos y en cada paso solo habla con sus 2 vecinos.',
      '',
      '• Recorre los 4 pasos. En el 2: "MPI_Isend e MPI_Irecv no bloquean: lanzo los mensajes a mis 2 vecinos y espero todo junto con MPI_Waitall."',
      '• Captura (arriba a la derecha): "Esta es la salida real de la demo. El proceso 1 tiene los nodos 13 a 24, que son parte del piso 3, el 4 y parte del 5. Necesita 10 fantasmas: 5 de abajo y 5 de arriba."',
      '• Balanceo: "El costo de K·U es proporcional a los no nulos, no a los nodos. El piso 1 tiene menos no nulos porque abajo está el suelo; repartiendo por nnz quedan 55 exactos en cada proceso."',
      '',
      'SI TE PREGUNTAN por qué un solo intercambio basta: después de actualizar U, el intercambio trae los fantasmas nuevos; con ellos se calcula la deriva Y el K·U del paso siguiente.',
      'Tiempo sugerido: 2 min',
    ].join('\n'));
  }

  // ================= 8 · H3 VERIFICACIÓN =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Simulador' });
    s.addText('07 · HITO H3 · VERIFICACIÓN', { placeholder: 'kicker' });
    s.addText('Idéntico al secuencial, bit a bit, con 1 a 16 procesos', { placeholder: 'title' });
    RES.pruebas.forEach(([p, n], i) => {
      const x = 0.6 + (i % 3) * 2.1, y = 1.6 + Math.floor(i / 3) * 1.55;
      const h3 = [1, 4, 9, 16].includes(p);
      tarjeta(s, x, y, 1.95, 1.4, `p ${p} fondo`, h3 ? C.accent3 : C.background2, h3 ? 0 : 0);
      T(s, `p = ${p}`, { x: x + 0.15, y: y + 0.12, w: 1.65, h: 0.45, fontSize: 22, bold: true, color: h3 ? C.background1 : C.text2, objectName: `p ${p}` });
      T(s, `${n} pruebas OK\n0 fallas`, { x: x + 0.15, y: y + 0.62, w: 1.65, h: 0.65, fontSize: 14, color: h3 ? C.background1 : C.text1, objectName: `p ${p} pruebas` });
    });
    T(s, [{ text: '■ ', options: { color: C.accent3 } }, { text: 'los que pide el criterio del H3 (1, 4, 9 y 16). 2 y 6 prueban repartos disparejos y SUMMA con malla rectangular.' }],
      { x: 0.6, y: 4.75, w: 6.2, h: 0.65, fontSize: 13, objectName: 'Leyenda H3' });
    tarjeta(s, 0.6, 5.5, 6.2, 1.2, 'Por qué bit a bit fondo');
    T(s, [{ text: '¿Por qué idéntico y no "parecido"? ', options: { bold: true, color: C.text2 } },
      { text: 'Cada fila de K·U la calcula un solo proceso, con sus sumas en el mismo orden que el secuencial. Si cambiara un bit, sería un error de reparto o un fantasma desactualizado.' }],
    { x: 0.85, y: 5.55, w: 5.75, h: 1.1, fontSize: 13, valign: 'middle', objectName: 'Por qué bit a bit' });
    const hc8 = captura(s, 'c06_identicos.png', 7.1, 1.6, 5.6, 'Captura archivos idénticos', { y0: 0.0, y1: 0.58, x0: 0, x1: 0.82 });
    T(s, 'La demo con 16 procesos escribe los mismos CSV, byte por byte, que la secuencial (fc /b)', { x: 7.1, y: 1.6 + hc8 + 0.08, w: 5.6, h: 0.6, fontSize: 12, color: GRIS, objectName: 'Captura idénticos pie' });
    tarjeta(s, 7.1, 5.5, 5.6, 1.2, 'Nota densos fondo');
    T(s, [{ text: 'Cannon y SUMMA: ', options: { bold: true, color: C.text2 } }, { text: 'dentro de ε = 1e-9 (error real ~1e-15): ahí el orden de las sumas sí cambia.' }],
      { x: 7.35, y: 5.5, w: 5.15, h: 1.2, fontSize: 14, valign: 'middle', objectName: 'Nota densos' });
    s.addNotes([
      'IDEA: El criterio del H3 dice "la simulación con MPI da los mismos desplazamientos que la secuencial con 1, 4, 9 y 16 procesos". Lo cumplimos con el criterio más fuerte posible: idéntico bit a bit.',
      '',
      '• Tarjetas verdes: los 4 del criterio. "Además probamos con 2 y 6 procesos, que reparten de forma dispareja."',
      '• Captura: "Corremos la demo secuencial y la de 16 procesos y comparamos los archivos con fc /b: no hay ninguna diferencia, ni en la respuesta por piso, ni en el mapa de derivas, ni en la historia del techo."',
      '',
      'SI TE PREGUNTAN qué prueba la suite: Cannon y SUMMA contra el producto naive (5 casos, incluidos tamaños que no son múltiplo de q y matrices rectangulares), el reparto, K·U distribuido en 3 matrices, el simulador (con sismo, vibración libre y un edificio con más procesos que pisos) y los errores.',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 9 · RESULTADOS SIMULADOR =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Simulador' });
    s.addText('08 · RESULTADOS', { placeholder: 'kicker' });
    s.addText('Hasta 4× más rápido con 9 procesos', { placeholder: 'title' });
    const labels = RES.sim.ps.slice(0, 4).map(p => `p = ${p}`).concat(['p = 16*']);
    const serie = k => RES.sim[k].slice(0, 4).concat([RES.sim.pasiva16[k]]);
    s.addChart(pres.charts.BAR, [
      { name: 'Edificio 100 × 400', labels, values: serie('100 × 400') },
      { name: 'Edificio 50 × 200', labels, values: serie('50 × 200') },
      { name: 'Edificio 20 × 50', labels, values: serie('20 × 50') },
    ], {
      x: 0.6, y: 1.55, w: 7.6, h: 5.0, barDir: 'col', barGrouping: 'clustered', barGapWidthPct: 60,
      chartColors: [HEX.accent1, HEX.accent2, HEX.accent5],
      showValue: true, dataLabelPosition: 'outEnd', dataLabelFormatCode: '0.0"×"', dataLabelFontSize: 10, dataLabelColor: HEX.dk1, dataLabelFontFace: '+mn-lt',
      valAxisMinVal: 0, valAxisMaxVal: 5, valAxisMajorUnit: 1, valAxisLabelFormatCode: '0"×"',
      valAxisLabelColor: GRIS, catAxisLabelColor: HEX.dk1, valAxisLabelFontSize: 11, catAxisLabelFontSize: 12,
      valAxisLabelFontFace: '+mn-lt', catAxisLabelFontFace: '+mn-lt',
      valGridLine: { color: 'E3E8EF', size: 0.75 }, catGridLine: { style: 'none' },
      showLegend: true, legendPos: 'b', legendFontSize: 12, legendFontFace: '+mn-lt', legendColor: HEX.dk1,
      showTitle: true, title: 'Speedup del simulador, S(p) = T(1) / T(p)', titleFontSize: 14, titleColor: HEX.dk2, titleFontFace: '+mn-lt',
      objectName: 'Gráfico speedup simulador',
    });
    T(s, '* 16 procesos con espera pasiva. 400 pasos, 8 sismos, reparto por nnz, mediana de 5.', { x: 0.6, y: 6.6, w: 7.6, h: 0.3, fontSize: 11, color: GRIS, objectName: 'Gráfico nota' });
    const cards = [
      ['3.6×', C.accent1, 'MPI con 8 procesos', `contra ${RES.sim.omp8.toFixed(1)}× de OpenMP con 8 hilos en la misma laptop (100 × 400)`],
      [`${RES.sim.comm8['100 × 400']} %`, C.accent2, 'del tiempo en comunicación', `en el edificio grande con 8 procesos; ${RES.sim.comm8['20 × 50']} % en el chico`],
      ['0.6×', C.accent6, '16 procesos con espera activa', 'más lento que 1 solo: hay más procesos que hilos (lámina 10)'],
    ];
    cards.forEach(([num, col, t, dd], i) => {
      const y = 1.55 + i * 1.72;
      tarjeta(s, 8.5, y, 4.2, 1.55, t + ' fondo');
      T(s, num, { x: 8.7, y, w: 1.45, h: 1.55, fontSize: 30, bold: true, color: col, align: 'center', valign: 'middle', objectName: t + ' cifra' });
      T(s, t, { x: 10.25, y: y + 0.15, w: 2.35, h: 0.55, fontSize: 15, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, dd, { x: 10.25, y: y + 0.7, w: 2.35, h: 0.8, fontSize: 12, objectName: t + ' detalle' });
    });
    s.addNotes([
      'IDEA: Mostrar el speedup del simulador con 3 tamaños de edificio y los dos hallazgos: MPI le gana a OpenMP en esta laptop y con 16 procesos hay un problema.',
      '',
      '• Gráfico: "Con 8 y 9 procesos llegamos a 3.5 a 4 veces. El máximo es 3.96× con 9 procesos en el edificio de 50 × 200."',
      '• Tarjeta 1: "En la misma laptop, OpenMP con 8 hilos daba 2.1×. MPI da 3.6×. En OpenMP todos los hilos se esperan en una barrera dos veces por paso; en MPI cada proceso solo espera a sus 2 vecinos." (Son mediciones de días distintos: lo confirmaremos en la máquina oficial.)',
      '• Tarjeta 2: "La comunicación pesa menos en el edificio grande: siempre se mandan 2 filas de nodos, pero el cálculo crece con el edificio."',
      '• Tarjeta 3: anticipa la lámina siguiente.',
      '',
      'SI TE PREGUNTAN qué es T(1): lo más rápido medido con 1 proceso. Aquí fue MPI con p = 1 (0.627 s), un poco más rápido que el secuencial (0.672 s); usamos ese para no inflar el speedup.',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 10 · LO QUE APRENDIMOS =================
  {
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Simulador' });
    s.addText('09 · LO QUE DICEN LOS NÚMEROS', { placeholder: 'kicker' });
    s.addText('16 procesos en 14 hilos: la espera activa lo arruina', { placeholder: 'title' });
    // Comparación espera activa / pasiva
    tarjeta(s, 0.6, 1.6, 6.3, 5.1, 'Espera panel');
    T(s, 'Edificio 100 × 400 con 16 procesos', { x: 0.9, y: 1.78, w: 5.8, h: 0.4, fontSize: 17, bold: true, color: C.text2, objectName: 'Espera título' });
    const barras = [['Espera activa (por defecto)', 0.64, C.accent2], ['Espera pasiva', 2.88, C.accent3], ['Con 9 procesos (referencia)', 3.71, C.accent1]];
    barras.forEach(([t, v, col], i) => {
      const y = 2.45 + i * 0.95, w = v / 4 * 4.2;
      T(s, t, { x: 0.9, y, w: 5.8, h: 0.3, fontSize: 13, objectName: t + ' etiqueta' });
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 0.9, y: y + 0.32, w, h: 0.42, rectRadius: 0.06, fill: { color: col }, line: { color: col, width: 0 }, objectName: t + ' barra' });
      T(s, `${v.toFixed(2)}×`, { x: 0.9 + w + 0.12, y: y + 0.32, w: 1.0, h: 0.42, fontSize: 16, bold: true, color: col, valign: 'middle', objectName: t + ' valor' });
    });
    T(s, [
      { text: 'Espera activa: ', options: { bold: true, color: C.accent2 } },
      { text: 'el proceso que espera un mensaje pregunta "¿ya llegó?" sin soltar el núcleo, y se lo quita justo al vecino que tiene que mandárselo. Con 400 pasos sincronizados, el efecto se acumula.', options: { breakLine: true } },
      { text: 'Solución: ', options: { bold: true, color: C.accent3 } },
      { text: 'mpiexec -env MSMPI_PROGRESS_SPIN_LIMIT 1', options: { fontFace: MONO, fontSize: 12 } },
    ], { x: 0.9, y: 5.25, w: 5.8, h: 1.35, fontSize: 13, paraSpaceAfter: 4, objectName: 'Espera explicación' });
    const ins = [
      [ic.cpu, 'Núcleos distintos', 'La laptop tiene 2 núcleos rápidos y 10 eficientes: por eso el techo práctico es ~4×, no 9× ni 12×.', C.accent1],
      [ic.edificio, 'Más grande, mejor escala', 'Comunicación ∝ 2 filas de nodos; cálculo ∝ nodos / p. Con 8 procesos: 28 % de comunicación en el edificio grande, 51 % en el chico.', C.accent5],
      [ic.red, 'Vecinos, no barreras', 'MPI con 8 procesos (3.6×) supera a OpenMP con 8 hilos (2.1×): cada proceso espera solo a 2 vecinos.', C.accent6],
      [ic.balance, 'Espera pasiva no siempre', 'Con 8 procesos es un poco más lenta (despertar cuesta): solo conviene con más procesos que núcleos.', C.accent4],
    ];
    ins.forEach(([img, t, dd, col], i) => {
      const y = 1.6 + i * 1.3;
      circuloIcono(s, img, 7.2, y + 0.05, 0.7, col, t);
      T(s, t, { x: 8.1, y, w: 4.6, h: 0.36, fontSize: 16, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, dd, { x: 8.1, y: y + 0.38, w: 4.6, h: 0.85, fontSize: 13, objectName: t + ' detalle' });
    });
    s.addNotes([
      'IDEA: El hallazgo más interesante de la medición: con 16 procesos (más que los 14 hilos de la laptop) el simulador es más lento que con 1. La causa es la espera activa de MS-MPI.',
      '',
      '• Barras: "Con la configuración por defecto, 0.64×. Con espera pasiva, 2.88×. Mismo código, solo cambia cómo espera MPI los mensajes."',
      '• Explicación: "MS-MPI, para responder rápido, espera los mensajes en un bucle activo. Si hay más procesos que núcleos, el que espera le quita la CPU al que debería mandarle el dato."',
      '• Las 4 tarjetas de la derecha: núcleos heterogéneos, tamaño del edificio, MPI contra OpenMP y por qué no dejamos la espera pasiva siempre.',
      '',
      'SI TE PREGUNTAN por qué el producto denso sufre menos con 16: se sincroniza solo q = 4 veces, no 400, y cada paso tiene mucho cálculo.',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 11 · MEDICIÓN (B) =================
  // (la lámina con los pasos de la demo se quitó: la demo se hace en vivo con VISUAL.bat)
  {
    pres.addSection({ title: 'Demo y cierre' });
    const s = pres.addSlide({ masterName: 'CONTENIDO', sectionTitle: 'Demo y cierre' });
    s.addText('11 · DEMO EN VIVO · LA MEDICIÓN EN SERIO (TECLA B)', { placeholder: 'kicker' });
    s.addText('El mismo sismo: de 0.59 s a 0.18 s con 8 procesos', { placeholder: 'title' });
    const hc = captura(s, 'v6_medicion.png', 0.6, 1.55, 7.6, 'Captura medición', { y0: 0.165, y1: 0.555, x0: 0.13, x1: 0.81 });
    T(s, 'Salida real del visual: edificio de 40 100 nodos, 8 sismos, 400 pasos, mediana de 3. La rayita naranja es el ideal.', { x: 0.6, y: 1.55 + hc + 0.06, w: 7.6, h: 0.5, fontSize: 11, color: GRIS, objectName: 'Captura medición pie' });
    const cards = [
      ['0.59 s', C.text2, 'Secuencial', 'Un núcleo, sin paralelismo'],
      ['0.18 s', C.accent3, 'MPI, 8 procesos', '3.3× más rápido; OpenMP 2.6×, Go 1.7×'],
      ['✓', C.accent1, 'idéntico', 'Los 4 motores dan los mismos números'],
    ];
    cards.forEach(([num, col, t, dd], i) => {
      const y = 1.55 + i * 1.72;
      tarjeta(s, 8.5, y, 4.2, 1.55, t + ' fondo');
      T(s, num, { x: 8.7, y, w: 1.65, h: 1.55, fontSize: 30, bold: true, color: col, align: 'center', valign: 'middle', objectName: t + ' cifra' });
      T(s, t, { x: 10.45, y: y + 0.2, w: 2.15, h: 0.4, fontSize: 16, bold: true, color: C.text2, objectName: t + ' título' });
      T(s, dd, { x: 10.45, y: y + 0.65, w: 2.15, h: 0.8, fontSize: 12, objectName: t + ' detalle' });
    });
    s.addNotes([
      'IDEA: El cierre de la demo y la respuesta a "¿sirve la programación paralela?": el mismo cálculo, el mismo resultado, en un tercio del tiempo.',
      '',
      'QUÉ HACER: en el visual, con el edificio muy grande, pulsar B. Tarda unos 30 s; mientras, explicar que cada motor corre solo, 400 pasos, mediana de 3.',
      'QUÉ DECIR: "El secuencial tarda 0,59 segundos. MPI con 8 procesos, 0,18: 3,3 veces más rápido. OpenMP llega a 2,6 y Go a 1,7. La rayita naranja es el ideal; no se llega por la comunicación, la memoria compartida y los pocos núcleos rápidos de la laptop. Pero el mismo sismo se calcula en un tercio del tiempo, con el mismo resultado."',
      '',
      'SI EL NÚMERO SALE DISTINTO: varía ±10-20 % entre corridas. Con batería en "Equilibrado" todo es ~2 veces más lento.',
      'Tiempo sugerido: 1.5 min',
    ].join('\n'));
  }

  // ================= 13 · CIERRE =================
  {
    const s = pres.addSlide({ masterName: 'FINAL', sectionTitle: 'Demo y cierre' });
    s.addText('Lo que sigue: S12 y S13, MPI + CUDA y clúster', { placeholder: 'title' });
    const sig = [
      [ic.red, 'S12–S13 · Clúster', 'Los mismos procesos MPI en varias computadoras (hoy todo corre en una laptop).'],
      [ic.rocket, 'S12 · Kernel CUDA', 'El K·U de cada proceso en la GPU (GTX 1660 SUPER), contrastado con cuSPARSE.'],
      [ic.exchange, 'S13 · Solapar', 'Calcular el interior de la franja mientras llegan las fronteras.'],
      [ic.check, 'Pendiente del equipo', 'Repetir make test-mpi y make bench-mpi en la máquina oficial; anotarlo en las actas.'],
    ];
    sig.forEach(([img, t, dd], i) => {
      const x = 0.7 + i * 3.08;
      circuloIcono(s, img, x, 2.0, 0.9, [C.accent2, C.accent6, C.accent5, C.accent3][i], t);
      T(s, t, { x, y: 3.1, w: 2.85, h: 0.45, fontSize: 18, bold: true, color: C.accent4, objectName: t + ' título' });
      T(s, dd, { x, y: 3.6, w: 2.85, h: 1.4, fontSize: 14, color: C.background1, objectName: t + ' detalle' });
    });
    T(s, '¿Preguntas?', { x: 0.7, y: 5.4, w: 12, h: 1.0, fontSize: 48, bold: true, color: C.background1, objectName: 'Preguntas' });
    s.addNotes([
      'IDEA: Cerrar con lo que sigue y abrir preguntas.',
      '',
      'QUÉ DECIR: "Con el H3 cerramos la memoria distribuida. En las semanas 12 y 13 llevamos los procesos MPI a un clúster de varias computadoras, porque hoy todo corre en una sola laptop. Además viene el híbrido: cada proceso MPI calcula su K·U en la GPU, y en S13 solapamos la comunicación con el cálculo."',
      '"Un pendiente del equipo: repetir las mediciones en la máquina oficial, porque estas son de mi laptop."',
      '',
      'Preguntas probables (respuestas en docs/GUIA_S9_S11.md, sección 7): ¿por qué 2D en el denso y franjas en el edificio?, ¿qué es un fantasma?, ¿por qué Sendrecv_replace?, ¿por qué SUMMA si ya está Cannon?',
      'Tiempo sugerido: 1 min + preguntas',
    ].join('\n'));
  }

  const out = process.argv[2] || 'Avance_S8_S11_MPI.pptx';
  await pres.writeFile({ fileName: out });
  await applyTheme(out, THEME);
  console.log('escrito', out);
})().catch(e => { console.error(e); process.exit(1); });
