# Builds zzz_jk2hud.pk3 for the JK2 HUD (cg_hudFiles 2 or ui/jk2hud.txt).
# The pk3 holds the shaders and a copy of the JKO hudleft image under a new name, because the
# JKA and SWGL pk3 files replace gfx/hud/hudleft. The image is read from the JKO assets of the
# game folder, so the pk3 stays on this machine.
#
# usage: make_jk2hud_pk3.ps1 -GameData "C:\...\GameData"
param(
	[Parameter(Mandatory = $true)][string]$GameData,
	[string]$Out	# default: base\zzz_jk2hud.pk3 in the game folder (the game must be closed)
)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$source = Join-Path $GameData 'base\0_JKO_Assets0.pk3'
if (-not (Test-Path $source)) { $source = Join-Path $GameData 'base\0-assets0.pk3' }
if (-not (Test-Path $source)) { throw "JKO assets not found in $GameData\base" }

$out = if ($Out) { $Out } else { Join-Path $GameData 'base\zzz_jk2hud.pk3' }
if (Test-Path $out) { Remove-Item $out }

$in = [IO.Compression.ZipFile]::OpenRead($source)
$entry = $in.GetEntry('gfx/hud/hudleft.tga')
if (-not $entry) { throw "gfx/hud/hudleft.tga missing in $source" }

$zip = [IO.Compression.ZipFile]::Open($out, 'Create')

$dst = $zip.CreateEntry('gfx/hud/jk2hudleft.tga', 'Optimal')
$s = $entry.Open(); $d = $dst.Open(); $s.CopyTo($d); $d.Dispose(); $s.Dispose()

# Desann and Tavion style arcs: the strong (red) arc of JK2 with the hue of the JKA icons
# (Desann 280, Tavion 180). Tavion is also less saturated, as in JKA.
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
public static class JK2HudTint
{
	public static Bitmap Recolor( Bitmap src, double hue, double satScale )
	{
		Bitmap dst = new Bitmap( src.Width, src.Height );
		for ( int y = 0; y < src.Height; y++ )
		{
			for ( int x = 0; x < src.Width; x++ )
			{
				Color c = src.GetPixel( x, y );
				double v = Math.Max( c.R, Math.Max( c.G, c.B ) ) / 255.0;
				double s = Math.Min( 1.0, Sat( c ) * satScale );
				dst.SetPixel( x, y, FromHsv( hue, s, v ) );
			}
		}
		return dst;
	}

	static double Sat( Color c )
	{
		double max = Math.Max( c.R, Math.Max( c.G, c.B ) ), min = Math.Min( c.R, Math.Min( c.G, c.B ) );
		return max <= 0 ? 0 : ( max - min ) / max;
	}

	static Color FromHsv( double h, double s, double v )
	{
		double c = v * s, hp = h / 60.0, x = c * ( 1 - Math.Abs( hp % 2 - 1 ) ), m = v - c, r = 0, g = 0, b = 0;
		if ( hp < 1 ) { r = c; g = x; }
		else if ( hp < 2 ) { r = x; g = c; }
		else if ( hp < 3 ) { g = c; b = x; }
		else if ( hp < 4 ) { g = x; b = c; }
		else if ( hp < 5 ) { r = x; b = c; }
		else { r = c; b = x; }
		return Color.FromArgb( (int)Math.Round( ( r + m ) * 255 ), (int)Math.Round( ( g + m ) * 255 ), (int)Math.Round( ( b + m ) * 255 ) );
	}
}
'@ -ReferencedAssemblies System.Drawing

$jpg = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$quality = New-Object Drawing.Imaging.EncoderParameters 1
$quality.Param[0] = New-Object Drawing.Imaging.EncoderParameter ([Drawing.Imaging.Encoder]::Quality), 95L

$strong = $in.GetEntry('gfx/hud/saber_stylesstrong.jpg')
if (-not $strong) { throw "gfx/hud/saber_stylesstrong.jpg missing in $source" }
$ms = New-Object IO.MemoryStream
$s = $strong.Open(); $s.CopyTo($ms); $s.Dispose(); $ms.Position = 0
$strongImage = New-Object Drawing.Bitmap $ms

foreach ($style in @(@('desann', 280.0, 1.0), @('tavion', 180.0, 0.5)))
{
	$tinted = [JK2HudTint]::Recolor($strongImage, $style[1], $style[2])
	$tmp = New-Object IO.MemoryStream
	$tinted.Save($tmp, $jpg, $quality)
	$dst = $zip.CreateEntry("gfx/hud/jk2saber_$($style[0]).jpg", 'Optimal')
	$d = $dst.Open(); $tmp.WriteTo($d); $d.Dispose()
	$tinted.Dispose(); $tmp.Dispose()
}
$strongImage.Dispose(); $ms.Dispose()

$dst = $zip.CreateEntry('shaders/jk2hud.shader', 'Optimal')
$bytes = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'jk2hud.shader'))
$d = $dst.Open(); $d.Write($bytes, 0, $bytes.Length); $d.Dispose()

$zip.Dispose(); $in.Dispose()
Write-Host "built $out"
