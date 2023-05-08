
try {
    $rawDisks = @(Get-Disk | Where-Object partitionstyle -eq 'raw')

    Write-Host "Uninitialized disks count : $rawDisks.Count"
    for($i = 0; $i -lt $rawDisks.Count ; $i++)
    {
        $disknum = $rawDisks[$i].Number
        $label = "Data" + $i

        Write-Host "Initialising disk $disknum with label $label"
        $dl = Get-Disk $disknum | Initialize-Disk -PartitionStyle MBR -PassThru | New-Partition -AssignDriveLetter -UseMaximumSize
        Format-Volume -driveletter $dl.Driveletter -FileSystem NTFS -NewFileSystemLabel $label -Confirm:$false -Force

        Write-Host "Initialized disk $disknum sucessfully"
    }

    Write-Host "Successfully initialized all the disks"
}
catch {
    Write-Error "Failed to initialize the raw disks"
    Write-Error "ERROR Message:  $_.Exception.Message"
    Write-Error "ERROR:: $Error | ConvertTo-json -Depth 1"
    Throw
}
