[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$imagesDirectory = Join-Path $RepositoryRoot 'images'
New-Item -ItemType Directory -Path $imagesDirectory -Force | Out-Null

function New-ArgbBitmap([int]$width, [int]$height) {
    return [System.Drawing.Bitmap]::new(
        $width,
        $height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function Add-RoundedRectangle(
    [System.Drawing.Drawing2D.GraphicsPath]$path,
    [float]$x,
    [float]$y,
    [float]$width,
    [float]$height,
    [float]$radius) {
    $diameter = $radius * 2
    $path.AddArc($x, $y, $diameter, $diameter, 180, 90)
    $path.AddArc($x + $width - $diameter, $y, $diameter, $diameter, 270, 90)
    $path.AddArc($x + $width - $diameter, $y + $height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($x, $y + $height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
}

function Draw-SnapViewMark(
    [System.Drawing.Graphics]$graphics,
    [float]$x,
    [float]$y,
    [float]$size) {
    $scale = $size / 256.0
    $graphics.TranslateTransform($x, $y)
    $graphics.ScaleTransform($scale, $scale)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    $backgroundPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    Add-RoundedRectangle $backgroundPath 18 18 220 220 48
    $backgroundBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        [System.Drawing.PointF]::new(28, 20),
        [System.Drawing.PointF]::new(228, 236),
        [System.Drawing.Color]::FromArgb(255, 7, 87, 200),
        [System.Drawing.Color]::FromArgb(255, 53, 194, 241))
    $blend = [System.Drawing.Drawing2D.ColorBlend]::new(3)
    $blend.Colors = @(
        [System.Drawing.Color]::FromArgb(255, 7, 87, 200),
        [System.Drawing.Color]::FromArgb(255, 20, 125, 226),
        [System.Drawing.Color]::FromArgb(255, 53, 194, 241))
    $blend.Positions = @(0.0, 0.52, 1.0)
    $backgroundBrush.InterpolationColors = $blend
    $graphics.FillPath($backgroundBrush, $backgroundPath)

    $backBoxPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    Add-RoundedRectangle $backBoxPath 46 95 116 104 10
    $backFill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(62, 190, 232, 255))
    $backStroke = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(199, 221, 245, 255), 12)
    $graphics.FillPath($backFill, $backBoxPath)
    $graphics.DrawPath($backStroke, $backBoxPath)

    $frontBoxPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    Add-RoundedRectangle $frontBoxPath 95 51 117 112 10
    $frontFill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 247, 252, 255))
    $frontStroke = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 7, 87, 200), 12)
    $graphics.FillPath($frontFill, $frontBoxPath)
    $graphics.DrawPath($frontStroke, $frontBoxPath)

    $frontAccent = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 131, 214, 250), 8)
    $frontAccent.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $frontAccent.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $frontAccent.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    [System.Drawing.PointF[]]$frontAccentPoints = @(
        [System.Drawing.PointF]::new(112, 143),
        [System.Drawing.PointF]::new(112, 68),
        [System.Drawing.PointF]::new(193, 68))
    $graphics.DrawLines($frontAccent, $frontAccentPoints)

    $backgroundBrush.Dispose()
    $backgroundPath.Dispose()
    $backBoxPath.Dispose()
    $backFill.Dispose()
    $backStroke.Dispose()
    $frontBoxPath.Dispose()
    $frontFill.Dispose()
    $frontStroke.Dispose()
    $frontAccent.Dispose()
    $graphics.ResetTransform()
}

function Save-Png([System.Drawing.Bitmap]$bitmap, [string]$path) {
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
}

function Write-Icon([System.Drawing.Bitmap[]]$bitmaps, [string]$path) {
    $streams = @()
    try {
        foreach ($bitmap in $bitmaps) {
            $stream = [System.IO.MemoryStream]::new()
            $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            $streams += $stream
        }

        $writer = [System.IO.BinaryWriter]::new([System.IO.File]::Open($path, [System.IO.FileMode]::Create))
        try {
            $writer.Write([UInt16]0)
            $writer.Write([UInt16]1)
            $writer.Write([UInt16]$streams.Count)
            $offset = 6 + (16 * $streams.Count)
            for ($index = 0; $index -lt $streams.Count; $index++) {
                $size = $bitmaps[$index].Width
                $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
                $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
                $writer.Write([byte]0)
                $writer.Write([byte]0)
                $writer.Write([UInt16]1)
                $writer.Write([UInt16]32)
                $writer.Write([UInt32]$streams[$index].Length)
                $writer.Write([UInt32]$offset)
                $offset += $streams[$index].Length
            }
            foreach ($stream in $streams) {
                $writer.Write($stream.ToArray())
            }
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        foreach ($stream in $streams) {
            $stream.Dispose()
        }
    }
}

function New-SquareAsset([int]$size) {
    $bitmap = New-ArgbBitmap $size $size
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    Draw-SnapViewMark $graphics 0 0 $size
    $graphics.Dispose()
    return $bitmap
}

function New-BrandAsset(
    [int]$width,
    [int]$height,
    [ValidateSet('Center', 'Right', 'Top')]
    [string]$markPosition = 'Center') {
    $bitmap = New-ArgbBitmap $width $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::FromArgb(255, 7, 87, 200))
    $backgroundBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        [System.Drawing.Point]::new(0, 0),
        [System.Drawing.Point]::new($width, $height),
        [System.Drawing.Color]::FromArgb(255, 7, 87, 200),
        [System.Drawing.Color]::FromArgb(255, 53, 194, 241))
    $graphics.FillRectangle($backgroundBrush, 0, 0, $width, $height)
    $markSize = [Math]::Min($height * 0.82, $width * 0.45)
    $markX = ($width - $markSize) / 2
    $markY = ($height - $markSize) / 2
    if ($markPosition -eq 'Right') {
        $markX = $width - $markSize - ($height * 0.08)
    }
    elseif ($markPosition -eq 'Top') {
        $markY = $height * 0.06
    }
    Draw-SnapViewMark $graphics $markX $markY $markSize
    $backgroundBrush.Dispose()
    $graphics.Dispose()
    return $bitmap
}

$iconSizes = @(16, 24, 32, 48, 64, 128, 256)
$iconBitmaps = @()
try {
    foreach ($size in $iconSizes) {
        $bitmap = New-SquareAsset $size
        $iconBitmaps += $bitmap
        Save-Png $bitmap (Join-Path $imagesDirectory "SnapView-$size.png")
    }
    Write-Icon $iconBitmaps (Join-Path $imagesDirectory 'SnapView.ico')
}
finally {
    foreach ($bitmap in $iconBitmaps) {
        $bitmap.Dispose()
    }
}

$storeSquareSizes = @{
    'Square44x44Logo.png' = 44
    'Square71x71Logo.png' = 71
    'Square150x150Logo.png' = 150
    'Square310x310Logo.png' = 310
    'StoreLogo.png' = 50
}
foreach ($asset in $storeSquareSizes.GetEnumerator()) {
    $bitmap = New-SquareAsset $asset.Value
    try {
        Save-Png $bitmap (Join-Path $imagesDirectory $asset.Key)
    }
    finally {
        $bitmap.Dispose()
    }
}

$wideAsset = New-BrandAsset 310 150
try {
    Save-Png $wideAsset (Join-Path $imagesDirectory 'Wide310x150Logo.png')
}
finally {
    $wideAsset.Dispose()
}

$splashAsset = New-BrandAsset 620 300
try {
    Save-Png $splashAsset (Join-Path $imagesDirectory 'SplashScreen.png')
}
finally {
    $splashAsset.Dispose()
}
