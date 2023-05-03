//
// license.cpp: defines licensing decryption methods
//
#ifdef WIN32
#include "stdafx.h"
#include <Windows.h>
#include <Iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#else
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <net/if_arp.h>

#define inaddrr(x) (*(struct in_addr *) &ifr->x[sizeof sa.sin_port])
#define IFRSIZE   ((int)(size * sizeof (struct ifreq)))

#endif // ifdef WIN32

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "license.h"
#include "../../host/config/svconfig.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

#define MAC_ESCAPE_STRING "svsHillview"
#define MAC_SIZE 18

//
// function to read contents of encrypted license file
//
void CLicense::ReadLicense(const char* fname)
{
    /* Read license file */
    fp = fopen (fname, "rb");
    if (fp != NULL)
    {
        fseek (fp, 0, SEEK_END);
        csize = ftell (fp);
        rewind (fp);
        size_t ctextsize;
        INM_SAFE_ARITHMETIC(ctextsize = InmSafeInt<long>::Type(csize) + 1, INMAGE_EX(csize))
        cipher_text = new unsigned char[ctextsize];
        if (cipher_text != NULL)
        {
            fread (cipher_text, 1, csize, fp);
            *(cipher_text + csize) = '\0';
        }
        fclose (fp);
    }
}

//
// function to initialize public key for decryption
//
void CLicense::InitializeKey()
{
    char hex_modulus[] = PUBLIC_KEY;
    char* public_exponent = new char[ KEY_SIZE*2 + 1 ];

	inm_sprintf_s(public_exponent, (KEY_SIZE * 2 + 1), "%x", RSA_F4);
    BN_hex2bn(&rsa->n, hex_modulus);
    BN_hex2bn(&rsa->e, public_exponent);
    rsa->d = NULL;
    rsa->p = NULL;
    rsa->q = NULL;
    rsa->dmp1 = NULL;
    rsa->dmq1 = NULL;
    rsa->iqmp = NULL;

    delete [] public_exponent;
}

//
// this function decrypts the license file
//
unsigned char* CLicense::DecryptLicense()
{
    /* License decryption */
    INM_SAFE_ARITHMETIC(msg_chunks = InmSafeInt<long>::Type(csize)/KEY_SIZE, INMAGE_EX(csize))
    INM_SAFE_ARITHMETIC(fsize = InmSafeInt<long>::Type(msg_chunks) * MSG_SIZE, INMAGE_EX(msg_chunks))
    size_t ptextsize;
    INM_SAFE_ARITHMETIC(ptextsize = InmSafeInt<long>::Type(fsize) + 1, INMAGE_EX(fsize))
    plain_text = new unsigned char[ptextsize];
    sign_text = new unsigned char [ KEY_SIZE + 1 ];;
    fsize = 0;

    for ( int i = 0; i < msg_chunks; i++)
    {
        for (int j = 0; j < KEY_SIZE; j++)
        {
            *(sign_text + j) = *(cipher_text + (KEY_SIZE * i) + j);
        }
        *(sign_text + KEY_SIZE) = '\0';
        fsize += RSA_public_decrypt(KEY_SIZE, sign_text, (plain_text + i*MSG_SIZE), rsa, RSA_PKCS1_PADDING);
    }

    *(plain_text + fsize) = '\0';
    fsize ++;
    return plain_text;
}

//
// Parser 1
//
SVERROR CLicense::ParseLicense(const char* options, const char* id, const char* property)
{
    SVConfig* pConfig = SVConfig::GetInstance();
    SVERROR rc;
    std::string buffer;

    pConfig->ParseString( (char*)plain_text );
    if (pConfig->GetArraySize( options ) == 0)
    {
        buffer = options;
        buffer += ".";
        buffer += property;
        rc = pConfig->GetValue(buffer, parser_result );
    }
    else
    {
        buffer = options;
        buffer += "[";
        buffer += id;
        buffer += "].";
        buffer += property;
        rc = pConfig->GetValue(buffer, parser_result );
    }
    return rc;
}

//
// Parser 2
//
SVERROR CLicense::ParseLicense(const char* options)
{
    SVConfig* pConfig = SVConfig::GetInstance();
    SVERROR rc;
    char buffer[1024];

    pConfig->ParseString( (char*)plain_text );
    int count = pConfig->GetArraySize( options );
	inm_sprintf_s(buffer, ARRAYSIZE(buffer), "%d", count);
    parser_result = buffer;

    return rc;
}

//
// returns character pointer to parser_result
//
const char* CLicense::GetParserResult()
{
    return parser_result.c_str();
}

//
// free allocated memory and other resources
//
void CLicense::FreeResources()
{
    delete [] sign_text;
    delete [] cipher_text;
    delete [] plain_text;
    RSA_free (rsa);
}

#ifdef WIN32
// Prints the MAC address stored in a 6 byte array to stdout
static void PrintMACaddress(unsigned char MACData[])
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X\n", 
        MACData[0], MACData[1], MACData[2], MACData[3], MACData[4], MACData[5]);
}

// Fetches the MAC address and prints it
static void GetMACaddress(void)
{
    IP_ADAPTER_INFO AdapterInfo[16];
    DWORD dwBufLen = sizeof(AdapterInfo);

    DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
    PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
    do
    {
        PrintMACaddress(pAdapterInfo->Address);
        pAdapterInfo = pAdapterInfo->Next;
    }
    while(pAdapterInfo);
}

// Validates the MAC address in license file
BOOL ValidateMACaddress (const char* s, int display)
{
	IP_ADAPTER_INFO AdapterInfo[16];
	DWORD dwBufLen = sizeof(AdapterInfo);
	char mac_string[18];
	bool result = false;
	const char *parsed=s;
	unsigned int i=0,j=0;
	unsigned int number_of_mac = 1;

	char buf[18];	// 12 alphanumeric + 5 colons + 1 \0 = 18 characters in each mac address.

	while(*parsed != '\0')
	{
		if(*parsed ==',')
			++number_of_mac;
		parsed++;
	}

	char **mac_list=new char* [10];
	while(i<number_of_mac)
	{
		mac_list[i]= new char[MAC_SIZE];	
		i++;
	}
	const char *temp=s;

	for(i=0;i<number_of_mac;i++)
	{		
		j=0;
		while(*temp != ',' && *temp != '\0')
		{
			buf[j] = *temp;
			temp++;
			j++;
		}
		temp++;
		buf[j] = '\0'; // End of MAC address
		inm_strcpy_s(mac_list[i],MAC_SIZE,buf);
	}

	DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);
	PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;

    unsigned int comma = 0;
	do
	{
		if (display)
		{  
			if(comma > 0)
				printf(",");

			printf("%02X:%02X:%02X:%02X:%02X:%02X",
					pAdapterInfo->Address[0], pAdapterInfo->Address[1],
					pAdapterInfo->Address[2], pAdapterInfo->Address[3],
					pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
			comma++;
		}
		else
		{
			inm_sprintf_s(mac_string, ARRAYSIZE(mac_string), "%02X:%02X:%02X:%02X:%02X:%02X",
					pAdapterInfo->Address[0], pAdapterInfo->Address[1],
					pAdapterInfo->Address[2], pAdapterInfo->Address[3],
					pAdapterInfo->Address[4], pAdapterInfo->Address[5]);

			for(i=0; i<number_of_mac;i++)
			{
				if (!stricmp(mac_string, mac_list[i]))
				{
					result = true;
					break;
				}
			}
			if(result == true)
				break;			
		}
		pAdapterInfo = pAdapterInfo->Next;
	}
	while(pAdapterInfo);

    // CleanUp the memory allocated to mac_list[]
    i=0;
    while(i<number_of_mac)
	{
        delete[] mac_list[i];
        i++;
    }
    delete[10] mac_list;

	return result;
}

#else
//
// initialize structures to query mac address
//
net_info_t * mc_net_info_new (const char *device)
{
    net_info_t *n = (net_info_t *) malloc (sizeof(net_info_t));
    n->sock = socket (AF_INET, SOCK_DGRAM, 0);
    if (n->sock<0)
    {
        perror ("socket");
        free(n);
        return NULL;
    }

    inm_strcpy_s (n->dev.ifr_name, ARRAYSIZE(n->dev.ifr_name),device);
    if (ioctl(n->sock, SIOCGIFHWADDR, &n->dev) < 0)
    {
        perror ("set device name");
        free(n);
        return NULL;
    }
    return n;
}

//
// function to query mac address
//
mac_t * mc_net_info_get_mac (const net_info_t *net)
{
    int i;
    mac_t *n = (mac_t *) malloc (sizeof(mac_t));
    for (i=0; i<6; i++)
    {
        n->byte[i] = net->dev.ifr_hwaddr.sa_data[i] & 0xFF;
    }
    return n;
}   

//
// convert mac address into a string
//
void mc_mac_into_string (const mac_t *mac, char *s, size_t mac_string_size)
{
    int i;
    for (i=0; i<6; i++)
    {
		inm_sprintf_s (&s[i*3], (mac_string_size - (i*3)), "%02x%s", mac->byte[i], i<5?":":"");
    }
}

//
// function to get the mac address of an interface
//
void get_mac_address (const char* device_name, char* mac_string, size_t mac_string_size)
{
    net_info_t *net;
    mac_t *mac;

    if ((net = mc_net_info_new(device_name)) != NULL)
    {
        mac = mc_net_info_get_mac(net);
		mc_mac_into_string (mac, mac_string, mac_string_size);
    }
}

bool validate_mac_address (const char* s, int display)
{
	unsigned char      *u;
	int                sockfd, size  = 1;
	struct ifreq       *ifr;
	struct ifconf      ifc;
	struct sockaddr_in sa;
	bool               result = false;
	char               mac_string[18];

	if (0 > (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP))) {
		return result;
	}

	ifc.ifc_len = IFRSIZE;
	ifc.ifc_req = NULL;

	do {
		++size;
		if (NULL == (ifc.ifc_req = (ifreq*)realloc(ifc.ifc_req, IFRSIZE))) {
			return result;
		}
		ifc.ifc_len = IFRSIZE;
		if (ioctl(sockfd, SIOCGIFCONF, &ifc)) {
			return result;
		}
	} while  (IFRSIZE <= ifc.ifc_len);

	const char *t=s;
	unsigned int i=0,j=0;
	unsigned int number_of_mac = 1;

	char buf[18];	// 12 alphanumeric + 5 colons + 1 \0 = 18 characters in each mac address.

	while(*t != '\0')
	{
		if(*t ==',')
			++number_of_mac;
		t++;		
//		printf("%c", *t);
	}

	char **mac_list=new char* [10];
	while(i<number_of_mac)
	{
		mac_list[i]= new char[MAC_SIZE];	
		i++;
	}

	const char *temp=s;

	for(i=0;i<number_of_mac;i++)
	{		
		j=0;
		while(*temp != ',' && *temp != '\0')
		{
			buf[j] = *temp;
			temp++;
			j++;
		}
		buf[j] = '\0'; // End of MAC address
		temp++;
		inm_strcpy_s(mac_list[i],MAC_SIZE,buf);
	}
	
	unsigned int comma = 0;
	ifr = ifc.ifc_req;
	for (;(char *) ifr < (char *) ifc.ifc_req + ifc.ifc_len; ++ifr) {

		if (ifr->ifr_addr.sa_data == (ifr+1)->ifr_addr.sa_data) 
			continue;

		if (0 == ioctl(sockfd, SIOCGIFHWADDR, ifr)) {
			switch (ifr->ifr_hwaddr.sa_family) {
				default:
					continue;
				case  ARPHRD_NETROM:  case  ARPHRD_ETHER:  case  ARPHRD_PPP:
				case  ARPHRD_EETHER:  case  ARPHRD_IEEE802: break;
			}

			u = (unsigned char *) &ifr->ifr_addr.sa_data;

			if (u[0] + u[1] + u[2] + u[3] + u[4] + u[5]) {
				if (display)
				{	if(comma > 0)
					printf(" ,");
					printf("%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
							u[0], u[1], u[2], u[3], u[4], u[5]);
					comma++;
				}
				else
				{
					inm_sprintf_s(mac_string, ARRAYSIZE(mac_string), "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
							u[0], u[1], u[2], u[3], u[4], u[5]);
					for(int i=0; i < number_of_mac ; i++)
						if (!strcasecmp(mac_string, mac_list[i])) {
							result = true;
							break;
						}
					if(result == true)
						break;
				}
			}
		}
	}

	close(sockfd);
	
	// CleanUp the memory allocated to mac_list[] 
	i=0;
	while(i < number_of_mac)
	{
		delete[] mac_list[i];
		i++;
	}
	delete[] mac_list;

	return result;
}
#endif	// ifdef WIN32

//
// gets all comma separated properties of a license
//
std::string get_license_properties (CLicense& license, const char* options, const char* id)
{
    std::string result;
    std::string result2;

    if (license.ParseLicense(options, id, "id").failed())
    {
        result = "-1";
    }
    else
    {
        result = license.GetParserResult();
        if (license.ParseLicense(options, id, "expiration_date").failed())
        {
            result2 = "-1";
        }
        else
        {
            result2 = license.GetParserResult();
        }
        result = result + "," + result2;

        if (!strcmp(options, "vx"))
        {
            if (license.ParseLicense(options, id, "allows_profiling").failed())
            {
                result2 = "-1";
            }
            else
            {
                result2 = license.GetParserResult();
            }
            result = result + "," + result2;

            if (license.ParseLicense(options, id, "allows_replication_rules").failed())
            {
                result2 = "-1";
            }
            else
            {
                result2 = license.GetParserResult();
            }
            result = result + "," + result2;

            if (license.ParseLicense(options, id, "allows_replication").failed())
            {
                result2 = "0";
            }
            else
            {
                result2 = license.GetParserResult();
            }
            result = result + "," + result2;

            if (license.ParseLicense(options, id, "allows_retention").failed())
            { 
                result2 = "0";
            } 
            else 
            { 
                result2 = license.GetParserResult();
            }
            result = result + "," + result2;
        }
        else if (!strcmp(options, "fx"))
        {
            if (license.ParseLicense(options, id, "allows_deletion").failed())
            {
                result2 = "-1";
            }
            else
            {
                result2 = license.GetParserResult();
            }
            result = result + "," + result2;
        }
    }
    return result;
}

//
// main function, usage: license [license file] [entity] [feature]
//
int main(int argc, char* argv[])
{
    //calling init funtion of inmsafec library
	init_inm_safe_c_apis();
	CLicense license;
    unsigned char* plain_text;
    const char* property;
    const char* options;
    const char* id;

    if(argc != 4 && argc != 2 && argc != 3)
    {
        /*
        printf("\nInmage license validation v0.1\n");
        printf("Usage: %s [options] [id] [property]\n", argv[0]);
        printf("\n");
        */
#ifdef WIN32
    ValidateMACaddress("",1);
#else
    validate_mac_address("",1);
#endif
        exit(1);
    }

#ifdef WIN32
    license.ReadLicense("c:\\home\\svsystems\\etc\\license.dat");
#else
    license.ReadLicense("/home/svsystems/etc/license.dat");
#endif // ifdef WIN32
    license.InitializeKey();
    plain_text = license.DecryptLicense();
  
    license.ParseLicense("cx", "0", "mac_address");
    const char* s = license.GetParserResult();

#ifdef WIN32
    if (!(ValidateMACaddress(s,0)))
#else
    if(!(validate_mac_address(s,0)))
#endif
    {
        if (strcmp(MAC_ESCAPE_STRING, license.GetParserResult()))
        {
            printf("%s\n", "-1");
            return 1;
        }
    }

    std::string result;
    if(argc == 4)
    {
        property = argv[3];
        options = argv[1];
        id = argv[2];
        if( license.ParseLicense(options, id, property).failed() ) 
        {
            result = "-1";
        }
        else 
        {
            result = license.GetParserResult();
        }
    }
    else if (argc == 3) 
    {
        options = argv[1];
        id = argv[2];
        result = get_license_properties (license, options, id);
    }
    else if (argc == 2)
    {
        options = argv[1];
        if( strcmp( options, "-all") == 0 )
        {
            result = (char*) plain_text ;
        }
        else if( license.ParseLicense(options).failed() )
        {
            result = "-1";
        }
        else 
        {
            std::string result2 = license.GetParserResult();
            int license_count = atoi (result2.c_str());
            if (license_count)
            {
                for (int i = 0; i < license_count; i++)
                {
                    char ln[20];
					inm_sprintf_s(ln, ARRAYSIZE(ln), "%d", i);
                    result = result + get_license_properties (license, options, ln) + ";";
                }
            }
            else
            {
                id = 0; 
                result = get_license_properties (license, options, id);
            }
        }
    }
    else
    {
        return 1;
    }

    printf("%s\n", result.c_str() );
    return 0;
}
