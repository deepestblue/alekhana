[CmdletBinding(PositionalBinding=$false)]

Param(
    [Parameter(Mandatory=$true)][string]$Path,
    [Parameter(Mandatory=$true)][string]$Name
)

$ErrorActionPreference="Stop"

$fontsFolder = (New-Object -ComObject Shell.Application).Namespace(0x14)
if ($fontsFolder.ParseName($name)) {
    # Install anyway
    # return
}

$fullPath = (Get-Item $path).fullname
$fontsFolder.CopyHere($fullPath, 4 + 16)
