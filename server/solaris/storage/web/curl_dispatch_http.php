<?php
/*
 *=================================================================
 * FILENAME
 *    curl_dispatch_http.php
 *
 * DESCRIPTION
 *    This File is to post the HTTP Request using curl
 *
 * INPUTS:
 *         URL AND POST VALUES
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
 */

#test_log("ENTERED1 \n");
$file_name  = array_shift($argv);
$url  = array_shift($argv);
#test_log("ENTERED2".$url."\n");
$post_content  = array_shift($argv);
$post_array = explode('!##!',$post_content);
for($i=0;$i<count($post_array);$i++)
{
	$complete_post = explode("##",$post_array[$i]);
	#test_log(print_r($complete_post,TRUE));
	if($complete_post[0] != null AND $complete_post[1] != null)
	{
		$post_data[$complete_post[0]] = $complete_post[1];
	}
}

#test_log(print_r($post_data,TRUE));

$curl_obj = curl_init($url); 
curl_setopt($curl_obj, CURLOPT_URL, $url); 
curl_setopt($curl_obj,CURLOPT_POST,true); 
curl_setopt($curl_obj, CURLOPT_POSTFIELDS, $post_data); 
curl_setopt($curl_obj, CURLOPT_SSL_VERIFYPEER, false);
curl_setopt($curl_obj, CURLOPT_SSL_VERIFYHOST, false);
curl_exec($curl_obj); 
#var_dump($curl_obj);

if(curl_errno($curl_obj))
{
    test_log("FAIL");
} 
else 
{
	test_log("SUCCESS");
    
} 
curl_close($curl_obj);

function test_log ( $debugString)
{
		
		if (preg_match('/Windows/i', php_uname()))
			$PHPDEBUG_FILE="C:\\home\\svsystems\\var\\curl_monitor.log";
		else
		      $PHPDEBUG_FILE="/home/svsystems/var/curl_monitor.log";
		      
		
		$fr = fopen($PHPDEBUG_FILE, 'a+');
		
		if(!$fr) 
		{
		  #echo "Error! Couldn't open the file.";
		}
		if (!fwrite($fr, $debugString . "\n")) 
		{
		  #print "Cannot write to file ($filename)";
		}
		if(!fclose($fr)) {
		  #echo "Error! Couldn't close the file.";
		}
}
	
?>
