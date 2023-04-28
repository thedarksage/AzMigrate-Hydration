package RequestHTTP;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");

use LWP::UserAgent;
use HTTP::Request::Common;
use Common::Log;
use Common::Constants;
use Data::Dumper;
use Utilities;
use Time::HiRes qw(gettimeofday);
use Digest::SHA qw(sha256_hex hmac_sha256_hex);

my $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
my $SCOUTAPI_VERSION = "1.0";
my $DIRECTAUTH_VERSION = "1.0";

##
#	Input Params:
#		HOST => IP address / host name
#		PORT => Port to be connected
#		HTTPS_MODE => 1/0, default 1
#       TIMEOUT => 180 seconds (default)
sub new
{
    #my $class = shift;
    my ($class, %args) = @_;
	my $self = {};

	while (my($key, $value) = each %args)
	{
	    $self->{$key} = $value;
	}
    
    # create log object as class variable
    $self->{'LOG_OBJ'} = new Common::Log();

    $self->{'HTTPS_MODE'} = 1 unless defined $self->{'HTTPS_MODE'};
    $self->{'TIMEOUT'} = 180 unless defined $self->{'TIMEOUT'};
	$self->{'DIRECTORY_SEPERATOR'} = Utilities::isWindows() ? "\\" : "/";
    $self->{'CERT_PATH'} = Utilities::get_cert_root_dir();    

    # Calling method functions like self
	$self->{'PASSPHRASE'} = get_cs_passphrase($self);
	$self->{'FINGERPRINT'} = $self->{'HTTPS_MODE'} ? get_cs_fingerprint($self) : '';
	$self->{'CERTFILE'} = $self->{'HTTPS_MODE'} ? get_cert_file($self) : '';

	$self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> New class variables set in method constructor : " . $self);
    
	# Bless self as class reference
	bless($self, $class) if defined $self->{'HOST'} and defined $self->{'PORT'};
}

#
#	Input Params:
#		http_method: GET/POST
#		access_params: reference hash.
#						Params include: access_method_name, access_file, access_key, auth_method (if not set, defaults to ComponentAuth), auth_version (optional)
#		content: reference hash.
#						Params include: type, content
#		headers: reference hash. for any custom headers (optional)
#	Output Params:
#		result: reference hash. with keys status, content
#			status: 0 => failure
#				  : 1 => success
#			content: response content
#	Throws
#		die exception. Handle at the callee.
#   Supports only GET/POST
sub call
{
	my ($self, $http_method, $access_params, $content, $headers) = @_;
	my %custom_headers = defined $headers ? %$headers : ();

    #bug: 5922423  - Do not log credentials even in Debug mode in CS
    # commented the below lines to address  the bug 5922423	
	#$self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> access_params in method call: " . Dumper($access_params));
	#$self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> headers in method call: " . Dumper($headers));
	#$self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> content in method call: " . Dumper($content));

	my $request_id = $self->gen_rand_nonce();
	my $version = defined $access_params->{'auth_version'} ? $access_params->{'auth_version'} : $DIRECTAUTH_VERSION;

	# Passphrase and fingerprint verification
	die "Unable to fetch passphrase\n" unless $self->{'PASSPHRASE'};
	die "Unable to fetch fingerprint\n" if ($self->{'HTTPS_MODE'} && ! $self->{'FINGERPRINT'});
	$self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> Fingerprint set in method call: " . $self->{'FINGERPRINT'});

	# Generate access signature
	my  $signature_params = 	{'RequestID' => $request_id, 'Version'	=> $version, 'Function' => $access_params->{'access_method_name'}};
	my  $access_signature = get_access_signature($self->{'PASSPHRASE'}, $http_method, $access_params->{'access_file'}, $signature_params, '', '', $self->{'FINGERPRINT'});
	die "Access signature generated as empty\n" unless $access_signature;
	$self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> Generated access_signature set in method call: " . $access_signature);

	# Build request content
	my $request_content = $content->{'content'};
	if (defined $content->{'type'})
	{
		# Setting header content type
		$custom_headers{'Content-Type'} = $content->{'type'};

		if ($content->{'type'} eq 'text/xml')
		{
			$request_content = $self->build_xml(
												{
													'requestid' => $request_id,
													'signature' => $access_signature,
													'accesskey' => $access_params->{'access_key'},
													'authmethod' => defined $access_params->{'auth_method'} ? $access_params->{'auth_method'} : Common::Constants::AUTHMETHOD_COMPAUTH,
													'authversion' => defined $access_params->{'auth_version'} ? $access_params->{'auth_version'} : $SCOUTAPI_VERSION
												},
												$content->{'content'}
												);
			$version = $SCOUTAPI_VERSION;
		}
	}

	# Adding Custom headers
	$custom_headers{'HTTP-AUTH-SIGNATURE'} = $access_signature;
	$custom_headers{'HTTP-AUTH-REQUESTID'} = $request_id;
	$custom_headers{'HTTP-AUTH-PHPAPI-NAME'} = $access_params->{'access_method_name'};
	$custom_headers{'HTTP-AUTH-VERSION'} = $version;

	$self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> Custom headers set in method call: " . Dumper(%custom_headers));

	# Request URL
    my $url = ($self->{'HTTPS_MODE'} ? "https" : "http") . "://" . $self->{'HOST'} . ":" . $self->{'PORT'} . ($access_params->{'access_file'} =~ m@^/@ ? "" : "/") . $access_params->{'access_file'};

	# Set environment variable for Certificate verification
	die "Unable to get certificate file\n" if $self->{'HTTPS_MODE'} and $self->{'CERTFILE'} eq '';
	local $ENV{HTTPS_CA_FILE} = $self->{'CERTFILE'} if $self->{'HTTPS_MODE'};

	# Creating LWP object	
	# not working on windows, sending headers in constructor. Need to set explicitly
	#my $ua = LWP::UserAgent->new(default_headers => HTTP::Headers->new(%custom_headers), timeout => $self->{'TIMEOUT'});
	my $ua = LWP::UserAgent->new(timeout => $self->{'TIMEOUT'});
	
	# Setting headers explicitly
	while (my ($header_key, $header_value) = each %custom_headers)
	{
		$ua->default_header( $header_key => $header_value );
	}
	
    my $ua_response;
    my $result = {'status' => 1, 'content' => ''};

    if (uc($http_method) eq 'POST')
    {
		$custom_headers{'Content-Type'} = 'application/x-www-form-urlencoded' unless defined $custom_headers{'Content-Type'};
		
		# For mulipart/form-data, content-type should set in post method.
        $ua_response = $ua->post($url, Content_Type => $custom_headers{'Content-Type'}, Content => $request_content);
    }
    elsif (uc($http_method) eq 'GET')
    {
        $ua_response = $ua->get($url);
    }
    else
    {
        $self->{'LOG_OBJ'}->log("EXCEPTION", "Unsupported method: $http_method");
        die "Unsupported method $http_method\n";
    }

 	unless ($ua_response->is_success)
	{
        #bug: 5922423  - Do not log credentials even in Debug mode in CS
        # commented the below lines to address  the bug 5922423		
		# $self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> LWP object data: " . Dumper($ua));
		# $self->{'LOG_OBJ'}->log("DEBUG", "RequestHTTP -> Response object data: " . Dumper($ua_response));
		
		$self->{'LOG_OBJ'}->log("EXCEPTION", "Failed to $http_method the details for $url");
		$self->{'LOG_OBJ'}->log("EXCEPTION", "--" . Dumper($ua_response->status_line));
		if ($access_params->{'access_method_name'} eq "RegisterHost")
		{
			my $ssl_negotiation = 0;
			my @substrs = ("500 SSL negotiation failed", "SSL certificate problem: self signed certificate","SSL certificate problem","(60) - Peer certificate cannot be authenticated with given CA certificates","(60) - Peer certificate cannot be authenticated with known CA certificates");
			foreach my $substr (@substrs)
			{
				if (index($ua_response->status_line, $substr) != -1) 
				{
					$ssl_negotiation = 1;
				}
			}
			if (($AMETHYST_VARS->{'CX_TYPE'} eq "2") 
				and 
				($ssl_negotiation == 1))
			{				
				 my %cert_update_status = (
					  Response => "0",
					   ErrorCode => "0000",
					   PlaceHolders => "0",
						ErrorDescription => "Success",
					  );
				my @list_services = ("tmansvc","ProcessServer","ProcessServerMonitor");
				my $return_value = tmanager::stop_services(\%cert_update_status,@list_services);
				if ($return_value == 0)
				{
					my $ps_cs_ip = $AMETHYST_VARS->{'PS_CS_IP'};
					my $ps_cs_port = $AMETHYST_VARS->{'PS_CS_PORT'};
					my $getfp_command = "csgetfingerprint.exe -i $ps_cs_ip -p $ps_cs_port";
					my $getfp_cmd_status = `$getfp_command 2>&1`;
				}
				tmanager::start_services_back(\%cert_update_status,@list_services);	
			}
		}
		
		$result->{'status'} = 0;
	}
	else
	{
		$result->{'content'} = $ua_response->{"_content"};
	}

	return $result;
}

#
#	Can be called as static method
#
sub get_access_signature
{
	my ($passphrase, $request_method, $access_file, $params, $headers, $content, $fingerprint) = @_;

	my $signature = "";
	my $signatureHMAC = "";
	my $fingerprintHMAC = '';
	my $delimiter = ':';
	my $version = "1.0";

	my $passphrase_hash = sha256_hex($passphrase);
	$fingerprintHMAC = $fingerprint ? hmac_sha256_hex($fingerprint, $passphrase_hash) : '';

	my $string_to_sign = $request_method.$delimiter.$access_file.$delimiter.$params->{'Function'}.$delimiter.$params->{'RequestID'}.$delimiter.$params->{'Version'};
	my $string_to_signHMAC = hmac_sha256_hex($string_to_sign, $passphrase_hash);

	$signature = $fingerprintHMAC . $delimiter . $string_to_signHMAC;
	$signatureHMAC = hmac_sha256_hex($signature, $passphrase_hash );

	return $signatureHMAC;
}

# Generate random nonce
sub gen_rand_nonce
{
    my $self = shift;
    my $count = 32;
	my $incudeTimeStamp = 1;
	my $random_nonce = "";

	eval
	{
		srand([gettimeofday]);

		my $nonce = '';

		if ($incudeTimeStamp) {
			$nonce .= 'ts:' .  time() . '-';
		}

		for (my $i = length($nonce) ; $i < $count; $i++) {
			$nonce .= sprintf("%02x", int(rand(256)));
		}

		# could have generated more than count so return count bytes
		$random_nonce = substr($nonce, 0, $count - 1);
	};
	if($@)
	{
		$self->{'LOG_OBJ'}->log("EXCEPTION","gen_rand_nonce : $@");
	}

	return $random_nonce;
}

sub get_cs_passphrase
{
	my $self = shift;
	my $file_path;
	my $passphrase = '';

	if (! $self->{'CERT_PATH'}) {	return ''; }

	eval
	{
		$file_path = $self->{'CERT_PATH'} . $self->{'DIRECTORY_SEPERATOR'} . 'private' . $self->{'DIRECTORY_SEPERATOR'} . 'connection.passphrase';
		$passphrase = Utilities::read_file($file_path);
	};

	if ($@)
	{
		$self->{'LOG_OBJ'}->log("EXCEPTION", "get_cs_passphrase : Could not retrieve passphrase : $@");
	}

	return $passphrase;
}

sub get_cert_file
{
	my $self = shift;
    my $cert_file;

	if (! $self->{'CERT_PATH'}) {	return ''; }

    $cert_file = is_cs_host($self) ? "cs.crt" : $self->{'HOST'}."_".$self->{'PORT'}.".crt";
    $cert_file = $self->{'CERT_PATH'}.$self->{'DIRECTORY_SEPERATOR'}.'certs'.$self->{'DIRECTORY_SEPERATOR'}.$cert_file;

    return '' unless (-e $cert_file);
	return $cert_file;
}

sub get_cs_fingerprint
{
    my $self = shift;
	my $fingerprint = '';
	my $file_path;

	if (! $self->{'CERT_PATH'}) {	return ''; }

	eval
	{
        $file_path = is_cs_host($self) ? "cs.fingerprint" : $self->{'HOST'}."_".$self->{'PORT'}.".fingerprint";
        $file_path = $self->{'CERT_PATH'}.$self->{'DIRECTORY_SEPERATOR'}.'fingerprints'.$self->{'DIRECTORY_SEPERATOR'}.$file_path;

		#print $file_path;
		$fingerprint = Utilities::read_file($file_path);
	};

	if ($@)
	{
		$self->{'LOG_OBJ'}->log("EXCEPTION", "get_cs_fingerprint : Could not retrieve fingerprint : $@");
	}

	return $fingerprint;
}

sub is_cs_host
{
    my $self = shift;
    my $is_cshost = 0;

	if($AMETHYST_VARS->{'CX_TYPE'} eq "1" || $AMETHYST_VARS->{'CX_TYPE'} eq "3")
	{
		#Both CS and PS
		if($AMETHYST_VARS->{'CS_IP'} eq $self->{'HOST'})
		{
            $is_cshost = 1;
		}
	}

    return $is_cshost;
}

#
#	Input Params:
#		header_params: reference hash. Params include: accesskey, requestid, signature
#		body: content of the request xml

#
sub build_xml
{
	my ($self, $header_params, $body) = @_;
	my $authentication_xml = "";

	if ($header_params->{'authmethod'} eq Common::Constants::AUTHMETHOD_COMPAUTH)
	{
		$authentication_xml = "
									<Authentication>
										<AuthMethod>".$header_params->{'authmethod'}."</AuthMethod>
										<AccessKeyID>".$header_params->{'accesskey'}."</AccessKeyID>
										<AccessSignature>".$header_params->{'signature'}."</AccessSignature>
									</Authentication>
		";
	}
	elsif ($header_params->{'authmethod'} eq Common::Constants::AUTHMETHOD_CXAUTH)
	{
		$authentication_xml = "
									<Authentication>
										<AuthMethod>".$header_params->{'authmethod'}."</AuthMethod>
										<AccessKeyID>11F3242118FF2ADD5D117CBF216F29AC578F6BA6</AccessKeyID>
										<CXUserName>admin</CXUserName>
										<CXPassword>784356513e3b6936d48a282ab6d9cb792327e612baecd8126ae8c02c772b8274</CXPassword>
									</Authentication>
		";
	}
	elsif ($header_params->{'authmethod'} eq Common::Constants::AUTHMETHOD_MESSAGEAUTH)
	{
		$authentication_xml = "
									<Authentication>
										<AuthMethod>".$header_params->{'authmethod'}."</AuthMethod>
										<AccessKeyID>11F3242118FF2ADD5D117CBF216F29AC578F6BA6</AccessKeyID>
										<AccessSignature></AccessSignature>
									</Authentication>
		";
	}

	my $request_xml = "<Request Id='".$header_params->{'requestid'}."' Version='" . $header_params->{'authversion'} . "'>
							<Header>
								$authentication_xml
							</Header>
							$body
						</Request>";
	return 	$request_xml;
}
1;