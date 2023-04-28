#
#=================================================================
# FILENAME
#   nls_parser.pl
#
# DESCRIPTION
#    This language NLS Parser for generating resource files.
#	
# HISTORY
#     03 Dec 2009  hpatel    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
use lib ("/home/svsystems/pm");
use XML::TreePP;
my $tpp = XML::TreePP->new();
if ( $^O =~ /MSWin32/ ) {
	$dirtoget="\\strawberry\\perl\\site\\lib\\lang\\xml\\";
} else {
	$dirtoget="/home/svsystems/pm/lang/xml";
}
opendir(IMD, $dirtoget) || die("Cannot open directory");
@thefiles= readdir(IMD);
closedir(IMD);

foreach $f (@thefiles)
{
   unless ( ($f eq ".") || ($f eq "..") )
   {
      $f = $dirtoget."/".$f;
      my $tree = $tpp->parsefile($f);
      my $text1 = $tree->{nls_resource_xml}->{traslations};
      #print ("print :: text is :: $text1\n");
      my %my_hash = %$text1;
      my $arr_ref = $my_hash{'text'};

      my $i=0;
      my $z=0;
      my $mystr = '$_ = array('."\n";

      foreach $hash_ref(@$arr_ref)
      {
	 %hash = %$hash_ref;

	 my $cntArray = scalar keys %$hash_ref;		
	 #print $cntArray;
		
	 my $cntArray1 = scalar keys %hash;		
	 #print "\n CNT->".$cntArray1."\n";
		
         foreach $key (sort(keys %hash)) {
         #print $key, '=', $hash{$key}, "\n";
                       

	 #print ("$i Key is :: $key --- value is :: $hash{$key}\n");	
	 
			
	    if($i == 1 || $i == 2)
            {
			
               #print ("$i Key is :: $key --- value is :: $hash{$key}\n");	
	       $hash{$key} =~ s/^\s+//; #remove leading spaces
	       $hash{$key} =~ s/\s+$//; #remove trailing spaces

		  if($i == 1)
                  {
		     $mystr = $mystr."\"".$hash{$key}."\"";
		  }
                  elsif($i == 2)
		  {
		     $mystr = $mystr." => \"".$hash{$key}."\", ";
		  }	
	    }
			
	       $i++; 
	       $z++;
                  if($i==3)
                  {
                     $i=0;
                     $mystr = $mystr."\n ";
                  }
            }

         #print ($hash);
         #print("\n");
    }
         $mystr = $mystr."\"-\"=>\"-\""."\n";
         $mystr = $mystr.");"."\n";


         my $langcode = $tree->{nls_resource_xml}->{config}->{native_lang}->{code};
         #my $charset = $tree->{nls_resource_xml}->{config}->{native_lang}->{"-charset"};
         my $language_name = $tree->{nls_resource_xml}->{config}->{native_lang}->{name};
         my $native_language_name = $tree->{nls_resource_xml}->{config}->{native_lang}->{native_name};
        
         # print ("print :: text is :: $langcode\n LNAME :: $language_name \n ");
	
	if ( $^O =~ /MSWin32/ ) {
	 	$LangDir = "\\home\\admin\\web\\ui\\lang";		
	} else {
	        $LangDir = "/home/svsystems/admin/web/ui/lang";
	}
         mkdir "$LangDir", 0777 unless -d "$LangDir";

	if ( $^O =~ /MSWin32/ ) {
	 	$LangFile = "\\home\\admin\\web\\ui\\lang\\".$langcode;
	} else {
         	$LangFile = "/home/svsystems/admin/web/ui/lang/".$langcode;
	}
         mkdir "$LangFile", 0777 unless -d "$LangFile";   

	if ( $^O =~ /MSWin32/ ) {
	         open FILE, ">$LangFile\\resource.php" or die $!;
	} else {
	         open FILE, ">$LangFile/resource.php" or die $!;
	}
	         
my $byteUnits = '$byteUnits', $day_of_week = '$day_of_week',$month = '$month', $datefmt = '$datefmt', $timespanfmt = '$timespanfmt';
$fileHeader = "
/*
#
#=================================================================
# FILENAME
#   resource.php
#
# DESCRIPTION
#    This language NLS resource file of english.
#	
# HISTORY
#     03 Nov 2009  hpatel    Created.
#
#=================================================================
#                 Copyright (c) InMage Systems
#=================================================================
*/\n
// shortcuts for Byte, Kilo, Mega, Giga, Tera, Peta, Exa
$byteUnits = array('B', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB');
$day_of_week = array('Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat');
$month = array('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec');
$datefmt = '%B %d, %Y at %I:%M %p';
$timespanfmt = '%s days, %s hours, %s minutes and %s seconds';
\n";

    
      $text = $fileHeader.'$_nls_conf = array(
      "allow recoding" => TRUE,
      #"charset" => "'.$charset.'",
      "Language" => "'.$language_name.'",
      "Language Native" => "'.$native_language_name.'",
      "Language Code" => "'.$langcode.'",
      "Text Direction" => "ltr",
      "Number Thousands Separator" => ",",
      "Number Decimal Separator" => ".",
      "Byte Unites" => $byteUnits,
      "WeekDays" => $day_of_week,
      "Months" => $month
      );';
	
      $text = "<? "."\n".$text."\n\n\n".$mystr."\n"."?>";
      print FILE $text;
      close FILE;
   }
}