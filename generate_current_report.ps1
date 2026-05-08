param(
  [string]$CsvPath = "results\current_experiment_results.csv",
  [string]$SvgPath = "results\current_speedup.svg",
  [string]$MdPath = "kmeans_experiment_report.md"
)

$ErrorActionPreference = "Stop"
$Invariant = [System.Globalization.CultureInfo]::InvariantCulture
$EnUs = [System.Globalization.CultureInfo]::GetCultureInfo("en-US")
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$CsvFullPath = Join-Path $Root $CsvPath
$SvgFullPath = Join-Path $Root $SvgPath
$MdFullPath = Join-Path $Root $MdPath

$Rows = Import-Csv $CsvFullPath | ForEach-Object {
  [pscustomobject]@{
    N = [int]$_.N
    d = [int]$_.d
    K = [int]$_.K
    max_iter = [int]$_.max_iter
    runs = [int]$_.runs
    seed = [int]$_.seed
    P = [int]$_.P
    serial_time_mean = [double]$_.serial_time_mean
    serial_time_std = [double]$_.serial_time_std
    mpi_time_mean = [double]$_.mpi_time_mean
    mpi_time_std = [double]$_.mpi_time_std
    speedup = [double]$_.speedup
    efficiency = [double]$_.efficiency
    cost = [double]$_.cost
  }
}

$Ns = @($Rows | Select-Object -ExpandProperty N -Unique | Sort-Object)
$Ps = @($Rows | Select-Object -ExpandProperty P -Unique | Sort-Object)
$First = $Rows | Select-Object -First 1

function Format-Int([int]$Value) {
  return $Value.ToString("N0", $EnUs)
}

function Format-Ms([double]$Value) {
  return $Value.ToString("0.0", $Invariant)
}

function Format-Speedup([double]$Value) {
  return $Value.ToString("0.00", $Invariant) + "x"
}

function Format-Percent([double]$Value) {
  return ($Value * 100).ToString("0.0", $Invariant) + "%"
}

$Width = 920
$Height = 600
$Left = 82
$Right = 170
$Top = 54
$Bottom = 76
$PlotWidth = $Width - $Left - $Right
$PlotHeight = $Height - $Top - $Bottom
$XMin = 1.0
$XMax = 8.0
$MaxSpeed = ($Rows | Measure-Object -Property speedup -Maximum).Maximum
$YMax = [Math]::Ceiling([Math]::Max(8.0, $MaxSpeed) * 1.05)

function Get-X([double]$P) {
  return $Left + (($P - $XMin) / ($XMax - $XMin)) * $PlotWidth
}

function Get-Y([double]$Value) {
  return $Top + $PlotHeight - ($Value / $YMax) * $PlotHeight
}

function Format-Number([double]$Value) {
  return $Value.ToString("0.00", $Invariant)
}

$Colors = @("#1f77b4", "#ff7f0e", "#2ca02c", "#d62728")
$Svg = New-Object System.Collections.Generic.List[string]
[void]$Svg.Add('<?xml version="1.0" encoding="UTF-8"?>')
[void]$Svg.Add("<svg xmlns='http://www.w3.org/2000/svg' width='$Width' height='$Height' viewBox='0 0 $Width $Height'>")
[void]$Svg.Add("<rect width='100%' height='100%' fill='#ffffff'/>")
[void]$Svg.Add("<text x='$($Width / 2)' y='30' text-anchor='middle' font-family='Arial, sans-serif' font-size='22' fill='#111'>K-Means Parallel Speedup</text>")
[void]$Svg.Add("<rect x='$Left' y='$Top' width='$PlotWidth' height='$PlotHeight' fill='#fff' stroke='#555' stroke-width='1'/>")

for ($Tick = 0; $Tick -le $YMax; $Tick++) {
  $Y = Format-Number (Get-Y $Tick)
  [void]$Svg.Add("<line x1='$Left' y1='$Y' x2='$($Left + $PlotWidth)' y2='$Y' stroke='#e6e6e6' stroke-width='1'/>")
  [void]$Svg.Add("<text x='$($Left - 12)' y='$(Format-Number ([double]$Y + 5))' text-anchor='end' font-family='Arial, sans-serif' font-size='14' fill='#333'>$Tick</text>")
}

foreach ($P in $Ps) {
  $X = Format-Number (Get-X $P)
  [void]$Svg.Add("<line x1='$X' y1='$Top' x2='$X' y2='$($Top + $PlotHeight)' stroke='#eeeeee' stroke-width='1'/>")
  [void]$Svg.Add("<text x='$X' y='$($Top + $PlotHeight + 28)' text-anchor='middle' font-family='Arial, sans-serif' font-size='14' fill='#333'>$P</text>")
}

[void]$Svg.Add("<text x='$($Left + $PlotWidth / 2)' y='$($Height - 22)' text-anchor='middle' font-family='Arial, sans-serif' font-size='16' fill='#111'>Number of Processes (P)</text>")
[void]$Svg.Add("<text transform='translate(24 $($Top + $PlotHeight / 2)) rotate(-90)' text-anchor='middle' font-family='Arial, sans-serif' font-size='16' fill='#111'>Speedup (Sp)</text>")

$IdealPoints = @()
foreach ($P in $Ps) {
  $IdealPoints += "$(Format-Number (Get-X $P)),$(Format-Number (Get-Y $P))"
}
[void]$Svg.Add("<polyline points='$($IdealPoints -join " ")' fill='none' stroke='#666' stroke-width='2.4' stroke-dasharray='7 5'/>")

for ($Index = 0; $Index -lt $Ns.Count; $Index++) {
  $N = $Ns[$Index]
  $Color = $Colors[$Index % $Colors.Count]
  $Group = @($Rows | Where-Object { $_.N -eq $N } | Sort-Object P)
  $Points = @()
  foreach ($Row in $Group) {
    $Points += "$(Format-Number (Get-X $Row.P)),$(Format-Number (Get-Y $Row.speedup))"
  }
  [void]$Svg.Add("<polyline points='$($Points -join " ")' fill='none' stroke='$Color' stroke-width='3'/>")
  foreach ($Row in $Group) {
    [void]$Svg.Add("<circle cx='$(Format-Number (Get-X $Row.P))' cy='$(Format-Number (Get-Y $Row.speedup))' r='5' fill='$Color'/>")
  }
}

$LegendX = $Left + $PlotWidth + 28
$LegendY = $Top + 18
for ($Index = 0; $Index -lt $Ns.Count; $Index++) {
  $N = $Ns[$Index]
  $Color = $Colors[$Index % $Colors.Count]
  [void]$Svg.Add("<line x1='$LegendX' y1='$LegendY' x2='$($LegendX + 28)' y2='$LegendY' stroke='$Color' stroke-width='3'/>")
  [void]$Svg.Add("<circle cx='$($LegendX + 14)' cy='$LegendY' r='4.5' fill='$Color'/>")
  [void]$Svg.Add("<text x='$($LegendX + 38)' y='$($LegendY + 5)' font-family='Arial, sans-serif' font-size='14' fill='#222'>N=$(Format-Int $N)</text>")
  $LegendY += 26
}
[void]$Svg.Add("<line x1='$LegendX' y1='$LegendY' x2='$($LegendX + 28)' y2='$LegendY' stroke='#666' stroke-width='2.4' stroke-dasharray='7 5'/>")
[void]$Svg.Add("<text x='$($LegendX + 38)' y='$($LegendY + 5)' font-family='Arial, sans-serif' font-size='14' fill='#222'>Ideal (Sp=P)</text>")
[void]$Svg.Add("</svg>")
Set-Content -Path $SvgFullPath -Value $Svg -Encoding UTF8

$BestEfficiency = $Rows | Sort-Object efficiency -Descending | Select-Object -First 1
$BestSpeedup = $Rows | Sort-Object speedup -Descending | Select-Object -First 1
$BestP8Efficiency = $Rows | Where-Object { $_.P -eq 8 } | Sort-Object efficiency -Descending | Select-Object -First 1

$Md = New-Object System.Collections.Generic.List[string]
[void]$Md.Add("# K-Means Parallel Experiment Results")
[void]$Md.Add("")
[void]$Md.Add("Parameters: d=$($First.d), K=$($First.K), max_iter=$($First.max_iter), seed=$($First.seed). Each configuration was run $($First.runs) times; times in tables are means.")
[void]$Md.Add("")
[void]$Md.Add("## Summary Tables")
[void]$Md.Add("")

foreach ($N in $Ns) {
  [void]$Md.Add("### N=$(Format-Int $N) Speedup")
  [void]$Md.Add("")
  [void]$Md.Add("| Processes P | Serial mean (ms) | MPI mean (ms) | Speedup | Efficiency |")
  [void]$Md.Add("|---:|---:|---:|---:|---:|")
  $Group = @($Rows | Where-Object { $_.N -eq $N } | Sort-Object P)
  foreach ($Row in $Group) {
    [void]$Md.Add("| $($Row.P) | $(Format-Ms $Row.serial_time_mean) | $(Format-Ms $Row.mpi_time_mean) | $(Format-Speedup $Row.speedup) | $(Format-Percent $Row.efficiency) |")
  }
  [void]$Md.Add("")
}

[void]$Md.Add("## Key Findings")
[void]$Md.Add("")
[void]$Md.Add("1. The maximum speedup occurs at N=$(Format-Int $BestSpeedup.N), P=$($BestSpeedup.P): $(Format-Speedup $BestSpeedup.speedup).")
[void]$Md.Add("2. The highest efficiency occurs at N=$(Format-Int $BestEfficiency.N), P=$($BestEfficiency.P): $(Format-Percent $BestEfficiency.efficiency).")
[void]$Md.Add("3. With 8 processes, N=$(Format-Int $BestP8Efficiency.N) keeps the best efficiency: $(Format-Percent $BestP8Efficiency.efficiency).")
[void]$Md.Add("4. Runtime generally decreases as P increases, while efficiency declines due to MPI communication, scattering, and scheduling overhead.")
[void]$Md.Add("")
[void]$Md.Add("## Speedup Chart")
[void]$Md.Add("")
[void]$Md.Add("![K-Means Parallel Speedup](results/current_speedup.svg)")
[void]$Md.Add("")
[void]$Md.Add("## Raw Data Files")
[void]$Md.Add("")
[void]$Md.Add('- `results/current_experiment_results.csv`')
[void]$Md.Add('- `results/current_speedup.svg`')
[void]$Md.Add("")
Set-Content -Path $MdFullPath -Value $Md -Encoding UTF8

Write-Host "Generated $MdFullPath"
Write-Host "Generated $SvgFullPath"
