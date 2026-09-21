using namespace System.Drawing.Imaging
using namespace System.IO
using namespace Microsoft.Test.VisualVerification

[CmdletBinding(PositionalBinding=$false)]

Param(
    [Parameter(Mandatory=$true)][string]$typefacePath,
    [Parameter(Mandatory=$true)][string]$rasteriser,
    [Parameter(Mandatory=$true)][string]$masterImages,
    [Parameter(Mandatory=$true)][string]$testCases
)

$ErrorActionPreference="Stop"

Add-Type -Assembly System.Drawing
[Reflection.Assembly]::LoadFile("$PSScriptRoot/TestApiCore.dll") | Out-Null
$snapshotVerifier = New-Object -TypeName SnapshotColorVerifier

function MkDirIfNotExists() {
    Param([Parameter(Mandatory = $True)] [String] $DirectoryToCreate)
    if (Test-Path -LiteralPath $DirectoryToCreate) {
        return
    }
    New-Item -Path $DirectoryToCreate -ItemType Directory -ErrorAction Stop | Out-Null
}

function New-TemporaryDirectory {
    $parent = [Path]::GetTempPath()
    $name = [Path]::GetRandomFileName()
    New-Item -ItemType Directory -Path (Join-Path $parent $name)
}

function Compare-Images {
Param(
    [Parameter(Mandatory=$true)][string]$expectedPath,
    [Parameter(Mandatory=$true)][string]$actualPath,
    [Parameter(Mandatory=$true)][string]$diffPath
)
    $expected = [Snapshot]::FromFile($expectedPath);
    $actual = [Snapshot]::FromFile($actualPath);

    if ($expected.Width -ne $actual.Width -or $expected.Height -ne $actual.Height) {
        Write-Output "Size mismatch: expected=$expectedPath ($($expected.Width)x$($expected.Height)) actual=$actualPath ($($actual.Width)x$($actual.Height))"

        MkDirIfNotExists (Split-Path -Path $diffPath)
        Copy-Item -LiteralPath $actualPath -Destination $diffPath
        return
    }

    $diff = $actual.CompareTo($expected);

    if ($snapshotVerifier.Verify($diff) -eq $([VerificationResult]::Pass)) {
        return
    }

    Write-Output "Contents mismatch: expected=$expectedPath actual=$actualPath diff=$diffPath"

    MkDirIfNotExists (Split-Path -Path $diffPath)
    $diff.ToFile($diffPath, $([ImageFormat]::Png)) | Out-Null
}

$tmpDir = New-TemporaryDirectory

# robocopy returns 0-7 for success, 8+ for failure
$robocopyResult = robocopy $masterImages "$tmpDir/expected" /MIR /Z /UNICODE /NFL /NDL /NP /NJH /NJS /NS /NC
if ($robocopyResult -ge 8) {
    throw "Failed to copy master images. Robocopy exit code: $robocopyResult"
}

MkDirIfNotExists "$tmpDir/actual"
& "$PSScriptRoot/generate_images.ps1" -Rasteriser $rasteriser -OutputRoot "$tmpDir/actual" -TypeFace $typefacePath -TestCases $testCases

MkDirIfNotExists "$tmpDir/diff"

Get-ChildItem -Path "$tmpDir/actual" -Recurse -File | ForEach-Object {
    $actual = $_.FullName
    $tmpPath = $tmpDir.FullName
    $expected = $actual.replace($tmpPath + '\actual\', $tmpPath + '\expected\')
    $diff = $actual.replace($tmpPath + '\actual\', $tmpPath + '\diff\')

    if ((Get-FileHash -LiteralPath $expected -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $actual -Algorithm SHA256).Hash) {
        # Equal
        return
    }

    Compare-Images $expected $actual $diff
}

$count = 0
Get-ChildItem "$tmpDir/diff" -Recurse -File | ForEach-Object {
    ++$count
    Write-Output $_.FullName
}

trap {
    if ($Env:GITHUB_ACTIONS -eq "true") {
        Move-Item $tmpDir ./output
    }
}

exit $count
