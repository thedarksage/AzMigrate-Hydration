use File::Copy;

my $port = "80";
my $portfindingcommand ="netstat -anp tcp | find /i \"listening\" | find /i \":" ;

$port = $ARGV[0] if defined $ARGV[0];

open( PS, $portfindingcommand.$port." \"|" ) ;
my $process = <PS> ;

while (! $process eq '' )
{
  print "Port ".$port." is already opened...Please enter another port to be used by IIS\n" ;
  $port = <STDIN>;
  chop($port) ;
  my $parameter = $portfindingcommand.$port." \"" ;
  open( PS, "$parameter|" ) ;
  $process = <PS> ;
  print "\n" ;
}


my @LINE = ();
my $CNT = 0;


# Modifying the contents of $FILE, the amethyst.conf
# Create a empty backup file by name $FILE.bak and put the modified contents of $FILE back in $FILE.bak and rename it to $FILE

open(LOG,'>',"C:\\home\\svsystems\\etc\\amethyst.conf.bak");

open(FH,"C:\\home\\svsystems\\etc\\amethyst.conf");
while($line=<FH>)
{
          if ($line =~ /HTTP_PORT\s+=.*/)
          {
             $line = "HTTP_PORT = \"$port\"\n";
             $LINE[$CNT] = $line;
          }
          $LINE[$CNT] = $line;
          $CNT++;
}
print LOG @LINE;
close(FH);
close(LOG);

rename("C:\\home\\svsystems\\etc\\amethyst.conf.bak","C:\\home\\svsystems\\etc\\amethyst.conf");

