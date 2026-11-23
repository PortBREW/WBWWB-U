$ErrorActionPreference = 'Stop'
# Copy the original game's sprites/ folder (github.com/ncase/wbwwb) into this directory first.
$source = Join-Path $PSScriptRoot 'sprites'
$assets = Join-Path $PSScriptRoot 'assets'
$magick = (Get-Command magick -ErrorAction Stop).Source
New-Item -ItemType Directory -Force -Path $assets | Out-Null

# Every image the C++ port loads, by original file name.
$needed = @(
    'bg','bg_dark','bg_shade','tv','cam','cam-flash','cam-instructions',
    'body','body_red','face','face_murder','hat','hangry','hatguy','gun',
    'peace','helping','face_nervous','face_snobby','face_snobby_hmph',
    'face_angry','happy_weirdo','lovehat','lover_shirt','lover_panic',
    'gore','gore_bodies','blood','snow','laptop',
    'chyron','chyron2','chyron3',
    'cricket','candlelight','lovers_watching','cursor',
    'weapon_gun','weapon_axe','weapon_bat','weapon_shotgun',
    'quote0001','quote0002','quote0003','quote0004',
    'credits0001','credits0002','credits0003','credits0004',
    'credits0005','credits0006','credits0007','credits0008',
    'logo','end_button','facebook','twitter'
)
Get-ChildItem -LiteralPath $assets -Filter '*.bmp' | Remove-Item -Force
foreach ($name in $needed) {
    $sourceFile = Get-ChildItem -LiteralPath $source -Recurse -Filter ($name + '.png') | Select-Object -First 1
    if (-not $sourceFile) { throw "Missing original sprite: $name.png" }
    $target = Join-Path $assets ($name + '.bmp')
    & $magick $sourceFile.FullName -background '#ff00ff' -alpha background -alpha off -colors 256 -type Palette "BMP3:$target"
    if ($LASTEXITCODE -ne 0) { throw "Image conversion failed for $($sourceFile.FullName)" }
}

# Sprite-sheet frame tables, generated from the original TexturePacker JSON.
$atlasNames = @(
    'body','body_red','face','face_murder','hat','hangry','hatguy','gun',
    'peace','helping','face_nervous','face_snobby','face_snobby_hmph',
    'face_angry','happy_weirdo','lovehat','lover_shirt','lover_panic',
    'gore','gore_bodies','blood','cricket','candlelight','lovers_watching',
    'cursor','weapon_gun','weapon_axe','weapon_bat','weapon_shotgun','end_button'
)
$header = [System.Collections.Generic.List[string]]::new()
$header.Add('#pragma once')
$header.Add('#include <SDL.h>')
$header.Add('#include <cstring>')
$header.Add('inline SDL_Rect spriteFrame(const char *sheet, int index) {')
foreach ($name in $atlasNames) {
    $jsonPath = Get-ChildItem -LiteralPath $source -Recurse -Filter ($name + '.json') | Select-Object -First 1
    if (-not $jsonPath) { throw "Missing original atlas metadata: $name.json" }
    $atlas = Get-Content -LiteralPath $jsonPath.FullName -Raw | ConvertFrom-Json
    $keys = $atlas.frames.PSObject.Properties.Name | Sort-Object
    $frameRows = foreach ($key in $keys) {
        $f = $atlas.frames.$key.frame
        "{$($f.x),$($f.y),$($f.w),$($f.h)}"
    }
    $arrayName = 'spriteFrames_' + ($name -replace '[^A-Za-z0-9_]', '_')
    $header.Add("    if (std::strcmp(sheet, `"$name`")==0) {")
    $header.Add("        static const SDL_Rect $arrayName`[] = { $($frameRows -join ',') };")
    $header.Add("        if(index<0 || index>=int(sizeof($arrayName`)/sizeof($arrayName`[0]))) index=0;")
    $header.Add("        return $arrayName`[index];")
    $header.Add('    }')
}
$header.Add('    return SDL_Rect{0,0,0,0};')
$header.Add('}')
$headerPath = Join-Path $PSScriptRoot 'src\generated_sprites.h'
[System.IO.File]::WriteAllLines($headerPath, $header)
Write-Host "Converted original sprite assets into $assets"
