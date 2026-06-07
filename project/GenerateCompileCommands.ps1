$slnDir = "$PSScriptRoot\"
$outJson = Join-Path $PSScriptRoot "compile_commands.json"
$commands = @()

$projects = Get-ChildItem -Path $PSScriptRoot -Filter "*.vcxproj" -Recurse | Where-Object { $_.FullName -notmatch "\\externals\\" -and $_.Name -notmatch "DirectXTex|imgui" }

foreach ($proj in $projects) {
    [xml]$xml = Get-Content $proj.FullName
    $ns = @{ default = "http://schemas.microsoft.com/developer/msbuild/2003" }
    
    $projDir = "$($proj.DirectoryName)\"
    
    # 解決用の関数
    function Resolve-Macro {
        param([string]$str)
        if (-not $str) { return "" }
        $str = $str -replace '\$\(SolutionDir\)', $slnDir
        $str = $str -replace '\$\(ProjectDir\)', $projDir
        return $str
    }
    
    # Debug/x64の設定からインクルードとマクロを抽出
    $includes = @()
    $defines = @()
    
    $itemDefs = Select-Xml -Xml $xml -XPath "//default:ItemDefinitionGroup" -Namespace $ns
    foreach ($def in $itemDefs) {
        $cond = $def.Node.Condition
        # Select Debug configuration to extract standard includes/defines
        if ($cond -match "Debug") {
            $clCompile = $def.Node.ClCompile
            if ($clCompile) {
                if ($clCompile.AdditionalIncludeDirectories) {
                    $incStr = Resolve-Macro -str $clCompile.AdditionalIncludeDirectories
                    $incArray = $incStr -split ';'
                    foreach ($inc in $incArray) {
                        if ($inc -and $inc -notmatch '%\(AdditionalIncludeDirectories\)') {
                            try {
                                $absInc = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($proj.DirectoryName, $inc))
                                $includes += "-I`"$absInc`""
                            } catch { }
                        }
                    }
                }
                
                if ($clCompile.PreprocessorDefinitions) {
                    $defStr = Resolve-Macro -str $clCompile.PreprocessorDefinitions
                    $defArray = $defStr -split ';'
                    foreach ($d in $defArray) {
                        if ($d -and $d -notmatch '%\(PreprocessorDefinitions\)') {
                            $defines += "-D$d"
                        }
                    }
                }
            }
        }
    }
    $includes = $includes | Select-Object -Unique
    $defines = $defines | Select-Object -Unique
    
    # Srcファイルを取得
    $items = Select-Xml -Xml $xml -XPath "//default:ItemGroup/default:ClCompile" -Namespace $ns
    foreach ($item in $items) {
        $file = $item.Node.Include
        if ($file) {
            try {
                $absFile = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($proj.DirectoryName, $file))
                
                $cmdArgs = @("clang-cl.exe", "/std:c++20", "/c", "/Zc:__cplusplus", "/TP", "`"$absFile`"") + $includes + $defines
                $cmdStr = $cmdArgs -join " "
                
                $cmdObj = [ordered]@{
                    directory = $slnDir.TrimEnd('\').Replace('\', '/')
                    command   = $cmdStr.Replace($slnDir, "./").Replace('\', '/')
                    file      = $absFile.Replace($slnDir, "./").Replace('\', '/')
                }
                $commands += $cmdObj
            } catch { }
        }
    }
    
    # Hdrファイルにも同様に定義を渡しておく(clangdはhファイルを開いたときに同dirのcppを見るが、念のため)
    $itemsH = Select-Xml -Xml $xml -XPath "//default:ItemGroup/default:ClInclude" -Namespace $ns
    foreach ($item in $itemsH) {
        $file = $item.Node.Include
        if ($file) {
            try {
                $absFile = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($proj.DirectoryName, $file))
                
                $cmdArgs = @("clang-cl.exe", "/std:c++20", "/c", "/Zc:__cplusplus", "/TP", "`"$absFile`"") + $includes + $defines
                $cmdStr = $cmdArgs -join " "
                
                $cmdObj = [ordered]@{
                    directory = $slnDir.TrimEnd('\').Replace('\', '/')
                    command   = $cmdStr.Replace($slnDir, "./").Replace('\', '/')
                    file      = $absFile.Replace($slnDir, "./").Replace('\', '/')
                }
                $commands += $cmdObj
            } catch { }
        }
    }
}

$commands | ConvertTo-Json -Depth 10 | Set-Content -Path $outJson -Encoding UTF8
Write-Output "Generated compile_commands.json with $($commands.Count) entries at $outJson"
