[CmdletBinding(PositionalBinding=$false)]

Param(
    [Parameter(Mandatory=$true)][string]$typefacePath,
    [Parameter(Mandatory=$true)][string]$rasteriser,
    [Parameter(Mandatory=$true)][string]$outputRoot
)

$ErrorActionPreference="Stop"

function MkDirIfNotExists() {
    Param([Parameter(Mandatory = $True)] [String] $DirectoryToCreate)
    if (Test-Path -LiteralPath $DirectoryToCreate) {
        return
    }
    New-Item -Path $DirectoryToCreate -ItemType Directory -ErrorAction Stop | Out-Null
}

MkDirIfNotExists $outputRoot

Get-ChildItem "$PSScriptRoot/../cases" -Name *txt | ForEach-Object {
    $dirName = $_ -replace '.txt', ''
    $outputDir = "$outputRoot/$dirName"
    MkDirIfNotExists $outputDir
    & $rasteriser "$PSScriptRoot/../cases/$_" $outputDir $typefacePath
}
