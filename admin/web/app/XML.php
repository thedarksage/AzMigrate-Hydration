<?php
/*
 *================================================================= 
 * FILENAME
 *    xml.php
 *
 * DESCRIPTION
 *    This script contains Functions Related to XML.
 *        
 *
 * HISTORY
 *     <09 Sep 2008>  Author    Created.Himanshu Patel.
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
 */
class XML
{
    /*
     * Function Name: XMl
     *
     * Description:
     *               Constructor
     *               It initializes the private variable
    */
    public function XML()
    {
        return "<?xml version='1.0' encoding='iso-8859-1'?>\n";
    }
    public function StartElement($element,$attribute){
        
        if(isset($attribute)){
            
            if (count($attribute) > 1){
                
                for($i=0;$i<count($attribute);$i++) {
                    $AttrStr1 = $attribute[$i];
                    $AttrStr = $AttrStr.$AttrStr1." ";
                }
                $StartData = "<".$element." ".$AttrStr.">";
            }  
            else
            {
                $StartData = "<".$element." ".$attribute[0].">";
            }
        }
        else {
            $StartData = "<".$element.">";
        }
        
        $xmlstr = $StartData."\n";
        return $xmlstr;
    }
    public function EndElement($element){
        
        $EndData = "</".$element.">";
        $xmlstr = $EndData."\n";
        return $xmlstr;
    }

    public function AddElement($parentele,$data,$attribute)
    {
        
        if(isset($attribute)){
            
            if (count($attribute) > 1){
                
                for($i=0;$i<count($attribute);$i++) {
                    $AttrStr1 = $attribute[$i];
                    $AttrStr = $AttrStr.$AttrStr1." ";
                }
                $StartData = "<".$parentele." ".$AttrStr.">";
            }  
            else
            {
                $StartData = "<".$parentele." ".$attribute.">";
            }
        }
        else
        {
        $StartData = "<".$parentele.">";
        }
        $EndData = "</".$parentele.">\n";
        $xmlstr = "  ".$StartData.$data.$EndData;
        return $xmlstr;
    }
 }
?>