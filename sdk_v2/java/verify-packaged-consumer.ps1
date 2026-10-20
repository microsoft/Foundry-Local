# Copyright (c) Microsoft Corporation. Licensed under the MIT License.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArtifactDirectory,
    [Parameter(Mandatory)][string]$Version,
    [string]$MavenDirectory,
    [string]$MavenSettings,
    [string]$JavaDirectory
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$repo = Join-Path $root 'build\consumer-maven-repository'
$maven = if ($MavenDirectory) {
    Join-Path $MavenDirectory $(if ($IsWindows) { 'bin\mvn.cmd' } else { 'bin/mvn' })
} else { 'mvn' }
if ($JavaDirectory) { $env:JAVA_HOME = $JavaDirectory }
$mavenArguments = @('--batch-mode', '--no-transfer-progress', "-Dmaven.repo.local=$repo")
if ($MavenSettings) { $mavenArguments += @('--settings', $MavenSettings) }
$artifact = "foundry-local-sdk-$Version"
$jar = Join-Path $ArtifactDirectory "$artifact.jar"
$pom = Join-Path $ArtifactDirectory "$artifact.pom"
foreach ($file in @($jar, $pom)) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing staged artifact: $file" }
}
& $maven @mavenArguments `
    org.apache.maven.plugins:maven-install-plugin:3.1.4:install-file "-Dfile=$jar" "-DpomFile=$pom"
if ($LASTEXITCODE -ne 0) { throw "Could not install staged Maven artifact: $artifact" }
& $maven @mavenArguments `
    -f (Join-Path $root 'consumer-smoke\pom.xml') "-Dfoundry.sdk.version=$Version" clean verify
if ($LASTEXITCODE -ne 0) { throw "Packaged consumer could not compile against $artifact" }
