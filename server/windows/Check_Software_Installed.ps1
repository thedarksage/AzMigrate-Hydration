# Check Software Installed

$Name=$args[0]
$VersionString=$args[1]
$FilterString = "Name='" + $Name + "'"

$Products = Get-WmiObject -Class win32_Product -Filter $FilterString
$Products
if($Products -AND $VersionString -eq "*.*")  
{
    exit 0
}
if($Products -AND $VersionString -ne "*.*") 
{
    foreach($Product in $Products) 
    {
        if($Product.Version -eq $VersionString) 
        {
            exit 2
        }
    }
    exit 3
}
exit 1