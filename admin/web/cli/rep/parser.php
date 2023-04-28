<?php

if($arg_third == 'post_data')
{
    if($arg_forth != '--xml')
	{
		$contents =  $arg_forth;
	}
}
else
{

	$contents = file_get_contents($file_name);//Or however you what it
}

$temp_arr  = array();
$parsed_array = getArrayFromXML($contents);
if(isset($parsed_array['pair_details']['volume_replication']['source_ip']))
{
	$temp_arr = $parsed_array['pair_details']['volume_replication'];
	$parsed_array['pair_details']['volume_replication'] = "";
	$parsed_array['pair_details']['volume_replication'][0] = $temp_arr;
}
if(isset($parsed_array['pair_details']['file_replication']['source_ip']))	
{
	$temp_arr = $parsed_array['pair_details']['file_replication'];
	$parsed_array['pair_details']['file_replication'] = "";
	$parsed_array['pair_details']['file_replication'][0] = $temp_arr;
}

function getArrayFromXML($cli_xml_to_parse)
{
	$arr_from_xml = array();
	$parent_arr = array();
	$xml_parsed_arr = parseXMLContent($cli_xml_to_parse);
	foreach($xml_parsed_arr as $xml_parsed_data)
	{
		$xml_attr_value = getXMLAttributeValues($xml_parsed_data);	
		$xml_tag = $xml_parsed_data['tag'];
		if(strcasecmp($xml_parsed_data['type'],'open') == 0)
		{
			$parent_arr[$xml_parsed_data['level']-1] = &$arr_from_xml;
			$arr_from_xml = constructChildArray($arr_from_xml, $xml_tag, $xml_attr_value);
			if(isset($arr_from_xml[$xml_tag][0]) and is_array($arr_from_xml[$xml_tag][0]))
			{
				$last_element = count($arr_from_xml[$xml_tag]) - 1;
				$arr_from_xml = &$arr_from_xml[$xml_tag][$last_element];
			}
			else
			{
				$arr_from_xml = &$arr_from_xml[$xml_tag];
			}
		}
		elseif(strcasecmp($xml_parsed_data['type'],'complete') == 0)
		{
			$arr_from_xml = constructChildArray($arr_from_xml, $xml_tag, $xml_attr_value);
		}
		elseif(strcasecmp($xml_parsed_data['type'],'close') == 0)
		{
			$arr_from_xml = &$parent_arr[$xml_parsed_data['level']-1];
		}
	}
	return $arr_from_xml;
}

function parseXMLContent($cli_xml_to_parse)
{
	// Creating parser object
	$cli_parser_obj = xml_parser_create();
	
	// disabling case folding option
	xml_parser_set_option($cli_parser_obj, XML_OPTION_CASE_FOLDING, 0);
	
	// setting option to skip white spaces to pass
	xml_parser_set_option($cli_parser_obj, XML_OPTION_SKIP_WHITE, 1);	
	
	xml_parse_into_struct($cli_parser_obj, $cli_xml_to_parse, $cli_parsed_xml_arr);
	
	// freeing parser object
	xml_parser_free($cli_parser_obj);
	
	return $cli_parsed_xml_arr;
}

function constructChildArray($arr_from_xml, $xml_tag, $xml_parsed_value)
{
	if(is_array($arr_from_xml) && in_array($xml_tag,array_keys($arr_from_xml)))
	{
		if((isset($arr_from_xml[$xml_tag][0]) and is_array($arr_from_xml[$xml_tag][0])))
		{
				$arr_from_xml[$xml_tag][] = $xml_parsed_value;
		}
		else
		{
			$arr_from_xml[$xml_tag] = array($arr_from_xml[$xml_tag],$xml_parsed_value);
		}
	}
	else
	{
		$arr_from_xml[$xml_tag] = $xml_parsed_value;
	}
	return $arr_from_xml;
}

function getXMLAttributeValues($xml_parsed_data)
{
	$xml_attr_value = array();
	if(isset($xml_parsed_data['value']))
	{
		$xml_attr_value['value'] = $xml_parsed_data['value'];
	}
	
	if(isset($xml_parsed_data['attributes']))
	{ 
		foreach($xml_parsed_data['attributes'] as $xml_parsed_key => $xml_parsed_value)
		{ 
			$xml_attr_value['attr'][$xml_parsed_key] = $xml_parsed_value;
		}
	}
	return $xml_attr_value;
}

?>