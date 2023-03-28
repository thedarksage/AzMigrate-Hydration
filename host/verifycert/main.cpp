
///
/// \file main.cpp
///
/// \brief
///

#include <iostream>
#include <vector>

#include "verifycert.h"
#include "terminateonheapcorruption.h"

std::string g_certTag("certificate:");
std::string g_serverTag("server:");
std::string g_fingerprintTag("fingerprints:");

enum INPUT_STATE {
    INPUT_STATE_START,
    INPUT_STATE_READING_CERT,
    INPUT_STATE_READING_SERVER,
    INPUT_STATE_READING_FINGERPRINT
};

/// \brief main function for verifycert
///
/// callers should setup a bi-driectionl pipe to verifycert
///
/// caller should send the following to verifycert
///
/// certificate:
/// <full certificate in pem format>
/// server:
/// <server name or ip>
/// fingerprints:
/// <fingeprint>
/// ...
/// <fingerprint>
///
/// NOTE fingerprints should not have any colons (':'), just the hex digits.
///
/// the caller should then read the restuls returned on stdout by verifycert
///
/// verifycert will return "passed" on stdout if the certificate passes validation otherwise error text on failure
/// always returns 0 as exit code.
///
/// e.g. in php it would look somthing like this
///    function execVerifyCmd($cmd,                     ///< full path to command to be executed
///                           $server,                  ///< server name or ip that sent the certificate
///                           $cert,                    ///< server certificate
///                           $fingerprints = array())  ///< array of fingerprints to check against
///    {
///        $proc = proc_open($cmd,
///                          array(
///                              0 => array('pipe', 'r'), // child stdin
///                              1 => array('pipe', 'w'), // child stdout
///                              2 => array('pipe', 'a')  // child stderr
///                          ),
///                          $pipes,
///                          null,
///                          null,
///                          array('suppress_errors' => true));
///
///        if (!$proc) {
///            return false;
///        }
///        $result = '';
///        $tag = "certificate:\n";
///        fwrite($pipes[0], $tag, strlen($tag));
///        fwrite($pipes[0], $cert, strlen($cert));
///        fwrite($pipes[0], "\n", 1);
///        $tag = "server:\n";
///        fwrite($pipes[0], $tag, strlen($tag));
///        fwrite($pipes[0], $server, strlen($server));
///        fwrite($pipes[0], "\n", 1);
///        if (count($fingerprints) > 0) {
///            $tag = "fingerprints:\n";
///            fwrite($pipes[0], $tag, strlen($tag));
///            foreach ($fingerprints as $fingerprint) {
///                $fingerprint = str_replace(':', '', $fingerprint); // make sure to remove any ':'
///                fwrite($pipes[0], $fingerprint, strlen($fingerprint));
///                fwrite($pipes[0], "\n", 1);
///            }
///        }
///        fclose($pipes[0]);
///        while (true) {
///            $read = array($pipes[1]);
///            $except = null;
///            $write = null;
///            $n = @stream_select($read, $write, $except, 60);
///            if ($n === 0) {
///                proc_terminate($proc);
///                break;
///            } elseif ($n > 0) {
///                $line = fread($pipes[1], 8192);
///                if (strlen($line) == 0) {
///                    break;
///                }
///                $result .= $line;
///            }
///        }
///        for ($i = 1; $i < 3; $i++) {
///            fclose($pipes[$i]);
///        }
///        return "passed" === $result;
///    }

int main(int argc, char* arv[])
{
    TerminateOnHeapCorruption();

    INPUT_STATE state = INPUT_STATE_START;

    std::string line;
    std::string certificate;
    std::string server;
    securitylib::fingerprints_t fingerprints;
    do {
        std::getline(std::cin, line);
        if (g_certTag == line) {
            state = INPUT_STATE_READING_CERT;
        } else if (g_serverTag == line) {
            state = INPUT_STATE_READING_SERVER;
        } else if (g_fingerprintTag == line) {
            state = INPUT_STATE_READING_FINGERPRINT;
        } else {
            if (!line.empty()) {
                switch (state) {
                    case INPUT_STATE_READING_CERT:
                        certificate.append(line);
                        certificate += "\n";
                        break;
                    case INPUT_STATE_READING_SERVER:
                        server = line;
                        break;
                    case INPUT_STATE_READING_FINGERPRINT:
                        fingerprints.push_back(line);
                        break;
                    default:
                        std::cout << "invalid input " << line;
                        break;
                }
            }
        }
    } while (std::cin.good());
    if (certificate.empty()) {
        std::cout << "missing certificate\n";
    } else {
        std::cout << securitylib::verifyCert(certificate, server, fingerprints);

    }
    return 0;
}
