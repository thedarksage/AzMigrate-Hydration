Param
(
    [parameter(Mandatory=$true)]
    [String]
    $Container,
        
    [parameter(Mandatory=$true)]
    [String]
    $DestinationPath,             
                            
    [parameter(Mandatory=$true)]
    [String]
    $Storageaccountname,

    [parameter(Mandatory=$true)]
    [String]
    $key                           
)

$storage_account = New-AzStorageContext -StorageAccountName $Storageaccountname -StorageAccountKey $key

$blobs = Get-AzStorageBlob -Container $Container -Context $storage_account

foreach ($blob in $blobs)
{
    New-Item -ItemType Directory -Force -Path $DestinationPath

    Get-AzStorageBlobContent -Container $Container -Blob $blob.Name -Destination $DestinationPath -Context $storage_account
}