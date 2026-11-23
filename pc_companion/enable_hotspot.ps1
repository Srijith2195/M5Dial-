Add-Type -AssemblyName System.Runtime.WindowsRuntime
$asTaskGeneric = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { 
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' 
}
function Await($WinRtTask, $ResultType) {
    $asTask = $asTaskGeneric.MakeGenericMethod($ResultType)
    $netTask = $asTask.Invoke($null, @($WinRtTask))
    $netTask.Wait(-1) | Out-Null
    $netTask.Result
}
$asTaskAction = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { 
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncAction' 
}
function AwaitAction($WinRtAction) {
    $netTask = $asTaskAction[0].Invoke($null, @($WinRtAction))
    $netTask.Wait(-1) | Out-Null
}

[Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType = WindowsRuntime] | Out-Null
[Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType = WindowsRuntime] | Out-Null

$profile = [Windows.Networking.Connectivity.NetworkInformation]::GetInternetConnectionProfile()
if (-not $profile) {
    Write-Host "No active internet connection found."
    exit 0
}

$manager = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::CreateFromConnectionProfile($profile)
$config = $manager.GetCurrentAccessPointConfiguration()

$config.Ssid = "M5Dial_PC"
$config.Passphrase = "@Schne11"
$bandType = $config.Band.GetType()
$config.Band = [System.Enum]::Parse($bandType, "TwoPointFourGigahertz")

Write-Host "Configuring PC Hotspot: M5Dial_PC (2.4 GHz)..."
AwaitAction ($manager.ConfigureAccessPointAsync($config))

if ($manager.TetheringOperationalState -ne [Windows.Networking.NetworkOperators.TetheringOperationalState]::On) {
    Write-Host "Enabling Mobile Hotspot..."
    $result = Await ($manager.StartTetheringAsync()) ([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult])
    Write-Host "Hotspot status: $($result.Status)"
} else {
    Write-Host "Mobile Hotspot is already ACTIVE."
}
