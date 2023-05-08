#include "inmcrypto.h"
#include <iostream>


int main(int argc, char ** argv)
{
    inmcrypto<inmcrypto_default_traits> cryptor ;

    // BUG BUG read from some file
    std::string k =  "@#INMAGESYSTEMS-vContinuum@#";

    if(argc != 3) {
        std::cerr <<  "usage: " << argv[0] << " -encrypt|-decrypt <input> " << std::endl;     
        return -1;
    }

    if(!strcmp(argv[1], "-encrypt")) {
        std::string enc = cryptor.encrypt(k,argv[2]);
        std::cout << enc ;
    } else if (!strcmp(argv[1],"-decrypt")) {
        std::cout << cryptor.decrypt(k,argv[2]);
    } else {
        std::cerr <<  "usage: " << argv[0] << " -encrypt|-decrypt <input> " << std::endl;
        return -1;
    }

    return 0;
}

