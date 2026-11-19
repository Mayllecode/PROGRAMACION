param(
  [Parameter(Mandatory=$true)][string]$Pptx,
  [Parameter(Mandatory=$true)][string]$OutDir,
  [int]$Width = 1600,
  [string]$Pdf = ""
)
$ErrorActionPreference = "Stop"
$Pptx = (Resolve-Path $Pptx).Path
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
Get-ChildItem $OutDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
$app = New-Object -ComObject PowerPoint.Application
try {
  # Abrir: ReadOnly, Untitled=false, WithWindow=false
  $pres = $app.Presentations.Open($Pptx, $true, $false, $false)
  $h = [int]($Width * $pres.PageSetup.SlideHeight / $pres.PageSetup.SlideWidth)
  $i = 0
  foreach ($s in $pres.Slides) {
    $i++
    $s.Export((Join-Path $OutDir ("s{0:D2}.png" -f $i)), "PNG", $Width, $h)
  }
  if ($Pdf -ne "") { $pres.SaveAs($Pdf, 32) }  # 32 = ppSaveAsPDF
  $pres.Close()
  Write-Output "exportadas $i laminas"
} finally {
  $app.Quit()
  [System.Runtime.InteropServices.Marshal]::ReleaseComObject($app) | Out-Null
}
