[CmdletBinding(PositionalBinding=$false)]

Param(
    [Parameter(Mandatory=$true, ParameterSetName="Run")][string]$typefacePath,
    [Parameter(Mandatory=$true, ParameterSetName="Run")][string]$rasteriser,
    [Parameter(Mandatory=$true, ParameterSetName="Run")][string]$outputRoot,
    [Parameter(Mandatory=$true, ParameterSetName="Run")][string]$testCases,
    [Parameter(Mandatory=$true, ParameterSetName="Version")][switch]$version
)

$ErrorActionPreference="Stop"

if ($PSCmdlet.ParameterSetName -eq "Version") {
    Get-Content (Join-Path $PSScriptRoot "VERSION")
    exit 0
}

function MkDirIfNotExists() {
    Param([Parameter(Mandatory = $True)] [String] $DirectoryToCreate)
    if (Test-Path -LiteralPath $DirectoryToCreate) {
        return
    }
    New-Item -Path $DirectoryToCreate -ItemType Directory -ErrorAction Stop | Out-Null
}

MkDirIfNotExists $outputRoot

Get-ChildItem $testCases -Name *txt | ForEach-Object {
    $dirName = $_ -replace '.txt', ''
    $outputDir = "$outputRoot/$dirName"
    MkDirIfNotExists $outputDir
    & $rasteriser "$testCases/$_" $outputDir $typefacePath
}
