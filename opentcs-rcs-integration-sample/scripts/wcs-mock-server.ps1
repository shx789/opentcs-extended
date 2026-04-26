param(
  [int]$Port = 18081,
  [string]$LogFile = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($LogFile)) {
  $LogFile = Join-Path $PSScriptRoot "..\..\build\demo\wcs-callbacks.log"
}

$logDir = Split-Path -Parent $LogFile
if (-not [string]::IsNullOrWhiteSpace($logDir)) {
  New-Item -ItemType Directory -Path $logDir -Force | Out-Null
}
Set-Content -Path $LogFile -Value "" -Encoding UTF8

$listener = New-Object System.Net.HttpListener
$prefix = "http://127.0.0.1:$Port/"
$listener.Prefixes.Add($prefix)
$listener.Start()

try {
  while ($listener.IsListening) {
    $context = $listener.GetContext()
    $request = $context.Request
    $response = $context.Response

    $path = $request.Url.AbsolutePath
    $method = $request.HttpMethod

    if ($method -eq "GET" -and $path -eq "/health") {
      $bytes = [System.Text.Encoding]::UTF8.GetBytes("{`"status`":`"ok`"}")
      $response.StatusCode = 200
      $response.ContentType = "application/json"
      $response.OutputStream.Write($bytes, 0, $bytes.Length)
      $response.Close()
      continue
    }

    if ($method -eq "POST" -and $path -eq "/api/v1/wcs/agv/events") {
      $reader = New-Object System.IO.StreamReader($request.InputStream, $request.ContentEncoding)
      $body = $reader.ReadToEnd()
      $reader.Dispose()

      Add-Content -Path $LogFile -Value ("[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $body)

      $responseJson = @{
        code = "0"
        msg = "OK"
        data = @{
          wcs_status = "ACCEPTED"
          idem_hit = $false
        }
      } | ConvertTo-Json -Depth 4 -Compress
      $bytes = [System.Text.Encoding]::UTF8.GetBytes($responseJson)
      $response.StatusCode = 200
      $response.ContentType = "application/json"
      $response.OutputStream.Write($bytes, 0, $bytes.Length)
      $response.Close()
      continue
    }

    $response.StatusCode = 404
    $response.Close()
  }
}
finally {
  if ($listener.IsListening) {
    $listener.Stop()
  }
  $listener.Close()
}

