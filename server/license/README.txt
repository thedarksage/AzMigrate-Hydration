The license generate utility can be used to generate encrypted licenses from the plain text license files.

To build the license generation tool, make your working directory 'server/license' and run the 'build.sh' script. This will create the 'inmgenerate' executable under the directory 'server/license/output'.

Next, to create the encrypted license, run the 'inmgenerate' utility as:
'./output/inmgenerate example.conf'
where 'example.conf' is the plain text license file.

The encrypted license is created under the 'output' directory and is named 'license.dat'. To use these licenses, move it to the '/home/svsystems/etc/' directory on the CX box, or use the browser to upload this file to CX using the license upload feature if you are on another machine. Do not rename this file to anything besides 'license.dat'.

The plain text license file has a simple ini configuration type format. A license file may contain multiple licenses marked by their headers enclosed in square brackets. A license header may be one of 3 types: [cx], [vx] or [fx].

Depending on the license type/header, there are different properties specific to the license, that follow. Properties are similar to a key-value pair. A complete list of properties is listed below:

# this is a comment
# good to have just one CX license per license file
[cx]
  id = unique license ID (40 character GUID)
  mac_address = MAC address of the interface specified below, for CX validation
                (escape string for internal builds: svsHillview)
  network_interface = corresponding interface bearing the MAC address above (default: eth0)
  license_version = license file format version (1.0 for now)
  expiration_date = expiration date in UTC of this license only (ex:  99999999999)
  allows_traps =  allow sending of traps (0/1)
  allows_email = allow sending of email alerts (0/1)
  allows_trending_basic = allow basic trending (0/1)
  allows_trending_detailed = allow advanced trending (0/1)
  allows_trending_network = allow network trending (mrtg stuff, 0/1)
  limit_fx_job_groups = limit the number of file replication job groups (say 10)
  limit_fx_jobs_per_group = limit the number of file replication jobs per group (say 10)

# Note that the boolean properties above are prefixed with 'allows' and not 'allow'
# boolean properties and limit properties are not enforced yet

# have as many VX licenses per license file as required
[vx]
  id = unique license ID (40 character GUID)
  expiration_date = expiration date in UTC of this license only (ex:  99999999999)
  allows_profiling = allow this host to function in profiling mode (0/1)
  allows_replication_rules = allow this host to accept replication rules (0/1)

# boolean properties are not enforced yet

[fx]
  id = unique license ID (40 character GUID)
  expiration_date = expiration date in UTC of this license only (ex:  99999999999)
  allows_deletion = allow deletion job options for jobs from host bearing this license (0/1)

# boolean properties are not enforced yet
# end of license file


NOTE: None of the properties are quoted. For more examples of usage, check 'example.conf' in the same directory as this README.
