param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$proj = Split-Path -Parent $root
Push-Location $proj
try {
  $dist = Join-Path $proj 'dist'
  if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
  New-Item -ItemType Directory -Path $dist | Out-Null

  $htmlPath = Join-Path $proj 'www\index.html'
  $cssPath  = Join-Path $proj 'www\assets\main.css'
  $jsPath   = Join-Path $proj 'www\app\app.js'

  if (-not (Test-Path $htmlPath)) { throw "Missing $htmlPath" }
  if (-not (Test-Path $cssPath))  { throw "Missing $cssPath" }
  if (-not (Test-Path $jsPath))   { throw "Missing $jsPath" }

  $html = Get-Content -Raw -LiteralPath $htmlPath
  $css  = Get-Content -Raw -LiteralPath $cssPath
  $js   = Get-Content -Raw -LiteralPath $jsPath

  # Inline CSS: replace link to main.css with a style tag
  # Insert style right after <head>
  $html = [regex]::Replace($html, '(?is)(<head[^>]*>)', { param($m) $m.Groups[1].Value + "`n  <style>`n$css`n  </style>" }, 1)

  # Inline JS: replace script tag that sources bundle.min.js with inline script
  $html = [Regex]::Replace($html, '<script[^>]*src="[^"/]*bundle\.min\.js"[^>]*>\s*</script>', "<script>`n$js`n</script>")

  # Write uncompressed index.html
  $outHtml = Join-Path $dist 'index.html'
  Set-Content -LiteralPath $outHtml -Value $html -Encoding UTF8

  # Gzip to index.html.gz
  $outGz = Join-Path $dist 'index.html.gz'
  $bytes = [System.Text.Encoding]::UTF8.GetBytes($html)
  $fs = [System.IO.File]::Create($outGz)
  try {
    $gz = New-Object System.IO.Compression.GZipStream($fs, [System.IO.Compression.CompressionLevel]::Optimal)
    try {
      $gz.Write($bytes, 0, $bytes.Length)
    } finally { $gz.Dispose() }
  } finally { $fs.Dispose() }

  # Copy favicon
  $favSrc = Join-Path $proj 'www\assets\favicon.png'
  if (Test-Path $favSrc) { Copy-Item -LiteralPath $favSrc -Destination (Join-Path $dist 'favicon.png') }

  Write-Output "Built dist/index.html and index.html.gz"
  Get-ChildItem -File -Recurse $dist | Select-Object FullName, Length | Format-Table -AutoSize
} finally {
  Pop-Location
}
