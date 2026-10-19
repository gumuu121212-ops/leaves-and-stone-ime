[CmdletBinding()]
param(
  [string]$Subject = 'CN=Leaves & Stone Test Signing',
  [int]$ValidYears = 2,
  [string]$PfxDirectory = (Join-Path $env:USERPROFILE 'Documents\LeavesAndStone-test-signing'),
  [string]$CerDirectory = 'output\certs',
  [securestring]$PfxPassword,
  [switch]$Force
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

$pfxFull = [System.IO.Path]::GetFullPath(
  [System.IO.Path]::Combine((Get-Location).Path, $PfxDirectory))
if ($pfxFull.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) {
  throw "The PFX directory must stay outside the repository ($repoRoot). Move -PfxDirectory to a user profile path."
}

if (-not $PfxPassword) {
  $PfxPassword = Read-Host -AsSecureString -Prompt 'Password for the generated PFX (needed to sign later)'
}
if (-not $PfxPassword -or $PfxPassword.Length -eq 0) {
  throw 'A non-empty PFX password is required.'
}

$cerFull = Join-Path $repoRoot $CerDirectory
$pfxFile = Join-Path $pfxFull 'leaves-and-stone-test-signing.pfx'
$cerFile = Join-Path $cerFull 'leaves-and-stone-test-signing.cer'

if ((Test-Path -LiteralPath $pfxFile) -and -not $Force) {
  throw "A PFX already exists at $pfxFile. Pass -Force to replace it, or pick another -PfxDirectory."
}

$notAfter = (Get-Date).AddYears($ValidYears)
# -Type CodeSigningCert already sets the code-signing EKU. The Microsoft
# SPC-indirect-data extension that some guides add is rejected by
# New-SelfSignedCertificate on current Windows builds, so only the standard EKU
# is spelled out here.
$certificate = New-SelfSignedCertificate `
  -Type CodeSigningCert `
  -Subject $Subject `
  -CertStoreLocation 'Cert:\CurrentUser\My' `
  -KeyUsage DigitalSignature `
  -KeyExportPolicy Exportable `
  -NotAfter $notAfter `
  -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3')

New-Item -ItemType Directory -Force -Path $pfxFull, $cerFull | Out-Null

$plainPassword = [System.Net.NetworkCredential]::new('', $PfxPassword).Password
Export-PfxCertificate -Cert $certificate -FilePath $pfxFile -Password $PfxPassword | Out-Null
Export-Certificate -Cert $certificate -FilePath $cerFile -Type CERT | Out-Null

$thumbprint = $certificate.Thumbprint
$cerHash = (Get-FileHash -LiteralPath $cerFile -Algorithm SHA256).Hash

Write-Output "thumbprint=$thumbprint"
Write-Output "subject=$Subject"
Write-Output "not_after=$($notAfter.ToString('yyyy-MM-dd'))"
Write-Output "pfx=$pfxFile"
Write-Output "cer=$cerFile"
Write-Output "cer_sha256=$cerHash"
Write-Output ''
Write-Output 'The PFX holds the private key. It must never be committed, uploaded, or shared.'
Write-Output 'Next steps:'
Write-Output '  1. Trust the certificate only after explicit user confirmation:'
Write-Output "     Import-Certificate -FilePath '$cerFile' -CertStoreLocation Cert:\CurrentUser\Root"
Write-Output '  2. Sign a build:'
Write-Output "     .\scripts\sign-build.ps1 -Thumbprint $thumbprint"
Write-Output '  3. Remove the certificate and private key when testing is done:'
Write-Output "     Get-ChildItem Cert:\CurrentUser\My, Cert:\CurrentUser\Root | Where-Object Thumbprint -eq $thumbprint | Remove-Item"
Write-Output "     Remove-Item '$pfxFile'"
