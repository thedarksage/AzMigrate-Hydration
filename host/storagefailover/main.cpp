
//
//  File: main.cpp
//
//  Description:
//

#include <iostream>
#include <string>

#include "storagefailover.h"

void usage(char const* prog)
{
    std::cout << "\nusage: " << prog << " -h <sourceHostId> <-r | -f> [-m]\n\n"
              << " -h <sourceHostId>: host id of the source making the request\n"
              << " -r               : report pairs only\n"
              << " -f               : do fail over (if primary host asking else same as -r)\n"
              << " -m               : do migration fail over. Requires user to manual add lun\n"
              << "                    access to the source side prior to starting migration and\n"
              << "                    manual remove access to the luns on the target side after\n"
              << "                    migration has completed\n"
              << '\n';
}

bool parseCmdLine(
    int argc,
    char* argv[],
    std::string& sourceHostId,
    bool& reportOnly,
    bool& failOver,
    bool& migration)
{
    bool argsOk = true;

    reportOnly = false;
    failOver = false;
    migration = false;

    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            std::cout << "*** invalid arg: " << argv[i] << '\n';
            argsOk = false;
        } else {
            switch (argv[i][1]) {
                case 'f':
                    failOver = true;
                    break;                    
                
                case 'h':
                    ++i;
                    if (i >= argc || '-' == argv[i][0]) {
                        --i;
                        std::cout << "*** missing value for -h\n";
                        argsOk = false;
                        break;
                    }

                    sourceHostId = argv[i];
                    break;

                case 'm':
                    migration = true;

                    break;

                case 'r':
                    reportOnly = true;

                    break;

                default:
                    std::cout << "*** invalid arg: " << argv[i] << '\n';
                    argsOk = false;
                    break;
            }
        }
        ++i;
    }

    if (sourceHostId.empty()) {
        std::cout << "*** sourceHostId required (use option -h <sourceHostId>)\n";
        argsOk = false;
    }

    if (reportOnly && failOver) {
        std::cout << "*** invalid option combination. Sspecify either -r or -f not both\n";
        argsOk = false;
    }

    if (!reportOnly && !failOver) {
        std::cout << "*** missing option. You must specify  -r or -f\n";
        argsOk = false;
    }

    if (!argsOk) {
        usage(argv[0]);
    }

    return argsOk;
}

int main(int argc, char* argv[])
{
    std::string sourceHostId;
    
    bool reportOnly;
    bool failOver;
    bool migration;

    if (!parseCmdLine(argc, argv, sourceHostId, reportOnly, failOver, migration)) {
        return StorageFailoverManager::RESULT_ERROR;
    }

    StorageFailoverManager storageFailover(sourceHostId, failOver, migration);

    return storageFailover.doFailover();

}
