# Generador de la exposición

Genera `Exposicion/Simulacion_Sismica_Exposicion.pptx` (13 láminas con notas
del orador), su PDF y la guía `Simulacion_Sismica_guia_exposicion.txt`.

| Archivo | Qué es |
|---|---|
| `build.js` | Las láminas. Los números de la lámina 11 están en el objeto `RES`, al inicio |
| `contenido.js` | La guía lámina por lámina: alimenta las notas del orador y el TXT |
| `make_txt.js` | Escribe el TXT (glosario, cifras y fuentes incluidos) |
| `render.ps1` | Exporta PNG y PDF con PowerPoint (necesita PowerPoint instalado) |

## Regenerar (Git Bash, desde esta carpeta)

```bash
npm install                     # una vez: pptxgenjs, react, react-dom, react-icons, sharp
export APPLY_THEME="<ruta a apply_theme.js de la skill pptx>"
node build.js ../Exposicion/Simulacion_Sismica_Exposicion.pptx
node make_txt.js ../Exposicion/Simulacion_Sismica_guia_exposicion.txt
powershell -File render.ps1 -Pptx ../Exposicion/Simulacion_Sismica_Exposicion.pptx -OutDir png -Pdf "$(cygpath -w "$PWD/../Exposicion")\Simulacion_Sismica_Exposicion.pdf"
```

Si `APPLY_THEME` no está a mano, el cambio más simple es editar el PPTX en
PowerPoint: los gráficos están hechos con formas (no son gráficos nativos), así
que también se pueden corregir a mano y no se rompen al importarlos en Canva.

## Avance S8–S11 (MPI)

| Archivo | Qué es |
|---|---|
| `build_s11.js` | Las 12 láminas de `Exposicion/Avance_S8_S11_MPI.pptx` (notas del orador incluidas). Números en `RES` |
| `informe_s11.html` | El informe `Exposicion/Informe_avance_S8_S11.pdf` (usa `Exposicion/capturas_s11/` y `matmul-paralelo/results/figuras/`) |
| `guion_s11.html` | El guion `Exposicion/Guion_S8_S11_MPI.pdf`: qué mostrar y qué decir, con láminas (`capturas_s11/laminas`) y capturas de la demo |

```bash
export APPLY_THEME="<ruta a apply_theme.js de la skill pptx>" NODE_PATH="$(cygpath -w "$PWD/node_modules")"
node build_s11.js ../Exposicion/Avance_S8_S11_MPI.pptx
powershell -ExecutionPolicy Bypass -File render.ps1 -Pptx ../Exposicion/Avance_S8_S11_MPI.pptx -OutDir png_s11 -Pdf "$(cygpath -w "$PWD/../Exposicion")\Avance_S8_S11_MPI.pdf"
"/c/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --no-pdf-header-footer --print-to-pdf="$(cygpath -w "$PWD/../Exposicion/Informe_avance_S8_S11.pdf")" "file:///$(cygpath -m "$PWD/informe_s11.html")"
```
