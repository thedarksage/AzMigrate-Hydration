function test{
	Param(
		[Parameter()]$x
	)
	Try{
		if($x){
			$x
		}else{
			Throw 'no $X passed' 
		}
	}
	Catch{
		Write-Host "$_" -fore red
	}
	Write-Host 'function has continued' -fore green
}

Test Hello # no excepetion
Test # exception