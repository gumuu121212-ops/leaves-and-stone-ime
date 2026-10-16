param(
  [string]$Source = 'docs/assets/visual-reference.png',
  [string]$OutputDirectory = 'resource/leaves-stone'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $repoRoot $Source
$outputPath = Join-Path $repoRoot $OutputDirectory

if (-not (Test-Path -LiteralPath $sourcePath)) {
  throw "Missing approved visual reference: $sourcePath"
}

New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
Add-Type -AssemblyName System.Drawing

if (-not ('LeavesStone.AssetExtractor' -as [type])) {
  Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

namespace LeavesStone {
  public enum MaskMode { Artwork, Foliage, Sky, Ink }

  public static class AssetExtractor {
    private static byte Ramp(double value, double start, double end) {
      if (value <= start) return 0;
      if (value >= end) return 255;
      return (byte)Math.Round((value - start) * 255.0 / (end - start));
    }

    public static void Extract(string sourcePath, string outputPath,
                               int x, int y, int width, int height,
                               MaskMode mode) {
      using (var source = new Bitmap(sourcePath)) {
        var crop = Rectangle.Intersect(new Rectangle(x, y, width, height),
                                       new Rectangle(0, 0, source.Width, source.Height));
        using (var result = new Bitmap(crop.Width, crop.Height,
                                       PixelFormat.Format32bppArgb)) {
          using (var graphics = Graphics.FromImage(result)) {
            graphics.Clear(Color.Transparent);
            graphics.DrawImage(source, new Rectangle(0, 0, crop.Width, crop.Height),
                               crop, GraphicsUnit.Pixel);
          }

          var bounds = new Rectangle(0, 0, result.Width, result.Height);
          var data = result.LockBits(bounds, ImageLockMode.ReadWrite,
                                     PixelFormat.Format32bppArgb);
          try {
            int byteCount = Math.Abs(data.Stride) * data.Height;
            var pixels = new byte[byteCount];
            Marshal.Copy(data.Scan0, pixels, 0, byteCount);

            for (int row = 0; row < data.Height; ++row) {
              for (int column = 0; column < data.Width; ++column) {
                int offset = row * data.Stride + column * 4;
                double b = pixels[offset + 0];
                double g = pixels[offset + 1];
                double r = pixels[offset + 2];
                double paperDistance = Math.Sqrt(
                    (r - 247) * (r - 247) +
                    (g - 244) * (g - 244) +
                    (b - 235) * (b - 235));
                double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                byte alpha;

                if (mode == MaskMode.Foliage) {
                  bool green = g > r * 1.03 && g > b * 1.03 && b < 175;
                  alpha = green ? Ramp(paperDistance, 9, 42) : (byte)0;
                } else if (mode == MaskMode.Sky) {
                  double blueSignal = b - r;
                  alpha = b > 165 && blueSignal > 8
                              ? Ramp(blueSignal, 8, 34)
                              : (byte)0;
                } else if (mode == MaskMode.Ink) {
                  alpha = Ramp(190 - luminance, 20, 105);
                  pixels[offset + 0] = 55;
                  pixels[offset + 1] = 52;
                  pixels[offset + 2] = 48;
                } else {
                  alpha = Ramp(paperDistance, 8, 48);
                }

                int sourceX = x + column;
                int sourceY = y + row;
                bool titleBand = sourceY >= 830 && sourceY <= 925 && sourceX < 480;
                if (titleBand) alpha = 0;

                pixels[offset + 3] = alpha;
                if (alpha == 0) {
                  pixels[offset + 0] = 0;
                  pixels[offset + 1] = 0;
                  pixels[offset + 2] = 0;
                }
              }
            }
            Marshal.Copy(pixels, 0, data.Scan0, byteCount);
          } finally {
            result.UnlockBits(data);
          }

          Directory.CreateDirectory(Path.GetDirectoryName(outputPath));
          const int maxDimension = 320;
          double scale = Math.Min(1.0, Math.Min(
              (double)maxDimension / result.Width,
              (double)maxDimension / result.Height));
          int outputWidth = Math.Max(1, (int)Math.Round(result.Width * scale));
          int outputHeight = Math.Max(1, (int)Math.Round(result.Height * scale));
          using (var resized = new Bitmap(outputWidth, outputHeight,
                                          PixelFormat.Format32bppArgb)) {
            using (var graphics = Graphics.FromImage(resized)) {
              graphics.Clear(Color.Transparent);
              graphics.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
              graphics.CompositingQuality = System.Drawing.Drawing2D.CompositingQuality.HighQuality;
              graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
              graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
              graphics.DrawImage(result, new Rectangle(0, 0, outputWidth, outputHeight));
            }
            resized.Save(outputPath, ImageFormat.Png);
          }
        }
      }
    }
  }
}
'@
}

$assets = @(
  @{ Name = 'leaf-left.png';            Crop = @(180, 250, 405, 430); Mode = 'Foliage' },
  @{ Name = 'leaf-right.png';           Crop = @(665, 265, 276, 405); Mode = 'Foliage' },
  @{ Name = 'building-terracotta.png';  Crop = @(440, 400, 245, 1120); Mode = 'Artwork' },
  @{ Name = 'building-ivory.png';       Crop = @(690, 620, 251, 930); Mode = 'Artwork' },
  @{ Name = 'sky-blue.png';             Crop = @(555, 455, 240, 760); Mode = 'Sky' },
  @{ Name = 'architectural-lines.png'; Crop = @(300, 610, 641, 870); Mode = 'Ink' }
)

foreach ($asset in $assets) {
  $destination = Join-Path $outputPath $asset.Name
  [LeavesStone.AssetExtractor]::Extract(
    $sourcePath,
    $destination,
    $asset.Crop[0],
    $asset.Crop[1],
    $asset.Crop[2],
    $asset.Crop[3],
    [LeavesStone.MaskMode]::$($asset.Mode)
  )
}

Write-Host "Generated $($assets.Count) transparent Leaves & Stone assets in $outputPath"
