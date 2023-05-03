#include <iostream>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <map>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <boost/random/mersenne_twister.hpp>

typedef std::vector<int> fileSizes_t;
typedef int (*generator_t)();

bool g_generate = true;
bool g_displayHelp = false;
bool g_validationPassed = true;
bool g_flushData = false;
bool g_createTopLevelDir = false;

int g_maxDirLevel = 3;
int g_maxDirCount = 5;
int g_maxFileSizeBytes = 1024 * 1024;
int g_minFileSizeBytes = 4096;
int g_repeat = -1;
int g_chunkBytes = 1024 * 1024;
int g_minChunkBytes = 1024 * 1024;
int g_maxChunkBytes = 1024 * 1024;

long long g_totalBytes = 0;

unsigned int g_seed = 0;

std::string g_dir;
std::string g_type("b");

fileSizes_t g_fileSizes;

generator_t g_generator; // holds the specific generator to use (e.g. binary, printable, etc.)

// this is used for write buffer size
boost::mt19937 g_mt19937GeneratorBufferSize;

// returns random number between min and max used for write buffer so as not
// to affect the random generator for the data being written so that validation
// does not have to worry about the different buffer sizes used
inline int randomBufferSize(int min, int max)
{
    return (g_mt19937GeneratorBufferSize() % ((max - min) + 1)) + min;
}

boost::mt19937 g_mt19937Generator;

// returns random number where 0 <= randon number <= maxNum
inline int randomNumberMax(int maxNum)
{
    return (g_mt19937Generator() % (maxNum + 1));
}

// returns random number where low <= randon number <= high
inline int randomNumberRange(int low, int high)
{
    return (randomNumberMax(high - low) + low);
}

int binaryGenerator()
{
    return randomNumberMax(255);
}

int printableGenerator()
{
    return (randomNumberMax(95) + 32);
}

int alphaGenerator()
{
    int num = randomNumberMax(51);
    return (num < 26 ? num + 65 : num + 71);
}

int numericGenerator()
{
    return (randomNumberMax(9) + 48);
}

int alphaNumericGenerator()
{
    int num = randomNumberMax(61);
    return (num < 10 ? num + 48 : (num < 36 ? num + 55 : num + 61));
}

bool setGenerator()
{
    if (1 == g_type.size()) {
        // type is single char so must be one of the following
        switch (g_type[0]) {
            case 'a':
                g_generator = &alphaGenerator;
                break;
            case 'b':
                g_generator = &binaryGenerator;
                break;
            case 'n':
                g_generator = &numericGenerator;
                break;
            case 'p':
                g_generator = &printableGenerator;
                break;
            default:
                return false;
        }
    } else if (2 == g_type.size()) {
        // type is 2 chars can only be 'an'
        if (!('a' == g_type[0] && 'n' == g_type[1])) {
            return false;
        }
        g_generator = &alphaNumericGenerator;
    } else {
        return false;
    }
    return true;
}

void usage()
{
    std::cout << "\nusage: gentestdata -s seed -b megabytes -d dir [-v] [additional-options] | -h\n"
              << '\n'
              << "  Generates random directories with random files with random data.\n"
              << "  Can also validate the data generated. \n"
              << '\n'
              << "    -s seed         : Required. A number greater than 0 and less than " << (std::numeric_limits<unsigned int>::max)() << '\n'
              << "                      used to seed the psuedo random number generator.\n"
              << "    -b megabytes    : Required. Total mega bytes to generate\n"
              << "    -d dir          : Required. Top level dir to generate test data under.\n"
              << "                      If dir does not exist will be prompted to create it. Use -c to create with out prompt.\n"
              << "    -v              : Optional. Verify generated test data.\n"
              << "                      If -v not specified, test data will be generated\n"
              << "    -h              : Optional. Display this help\n"
              << '\n'
              << "  Additional Options\n"
              << '\n'
              << "  The following are optional but required when validating if they were used during gneration\n"
              << '\n'
              << "    -m maxmb:minkb  : Optional. Specify the max size in mega bytes a file can be\n"
              << "                      and/or min size in kilo bytes a file can be.\n"
              << "                      Default max 1MB, Default min 4 kb\n"
              << "                      Note: use :minkb to use the default max size and set min size.\n"
              << "    -l maxlvl:maxdir: Optional. Specify the max number of directory levels to\n"
              << "                      create and the max number of directories to create per level\n"
              << "                      Default maxlvl 3, maxdir 5\n"
              << "                      Note: use :maxdir to use the default maxlvl and set maxdir.\n"
              << "    -t type         : Optional. Specifies the type of data to generate in files.\n"
              << "                      Default is binary.\n"
              << "                      type can be one of the following\n"
              << "                        b : binary data (ascii 0 - 255)\n"
              << "                        p : printable data (ascii 32 - 126)\n"
              << "                        a : alpha data (ascii 65 - 90, 97 - 122)\n"
              << "                        n : numeric data (ascii 30 - 39)\n"
              << "                        an: alpha numeric data (ascii 30 - 39,  65 - 90, 97 - 122)\n"
              << "    -r cnt          : Optional. Specifies that a given ascii value should be\n"
              << "                      repeated up to cnt times. Use this to generate compressable data\n"
              << "                      Use 0 to write the same ascii value to the whole file\n"
              << '\n'
              << "  The following are optional and not needed for validating as they are ignored during validation\n"
              << '\n'
              << "    -k kb[:kb]      : Optional. Specify the size (or range of sizes) for the write buffer\n"
              << "                      If only one kb specified it will be treated as write buffer size\n"
              << "                      If range (kb:kb) is specfied, then write size will be randomly selected\n"
              << "                      using the range. Default is 1024 KB (1MB) buffer size for all writes\n"
              << "                      NOTE: this option is not needed when verifying the data\n"
              << "    -f              : Optional. flush data after each write\n"
              << "    -c              : Optional. create top level directory if it does not exist with out prompting\n"
              << '\n'
              << "examples:\n"
              << "  generate 1GB of test data \n"
              << "  windows:\n"
              << "    gentestdata -s 100 -b 1024 -d c:\\tmp\n"
              << '\n'
              << "  linux/unix \n"
              << "    ./gentestdata -s 100 -b 1024 -d /tmp\n"
              << '\n'
              << "  to validate the data, run using the same options used to generate the data plus -v\n"
              << "  windows:\n"
              << "    gentestdata -s 100 -b 1024 -d c:\\tmp -v\n"
             << '\n'
              << "  linux/unix \n"
              << "    ./gentestdata -s 100 -b 1024 -d /tmp -v\n"
              << "\n\n";
}

bool parseCmd(int argc,
              char* argv[]
              )
{
    if (1 == argc) {
        usage();
        return false;
    }
    bool argsOk = true;
    int i = 1;
    while (i < argc) {
        if ('-' != argv[i][0]) {
            std::cout << "\n*** ERROR:\ninvalid option: " << argv[i] << '\n';
            argsOk = false;
        } else if ('\0' == argv[i][1]) {
            std::cout << "\n*** ERROR:\ninvalid option: " << argv[i] << '\n';
            argsOk = false;
        } else if ('-' == argv[i][1]) {
            std::cout << "\n*** ERROR:\ninvalid option: " << argv[i] << '\n';
            argsOk = false;
        } else {
            // short options or possibly in valid options
            if ('\0' != argv[i][2]) { // this is OK as argv[i][1] is not '\0' so 2 is valid
                std::cout << "\n*** ERROR:\ninvalid option: " << argv[i] << '\n';
                argsOk = false;
            } else {
                // short option
                switch (argv[i][1]) {
                    case 'b':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -b option\n";
                            argsOk = false;
                        } else {
                            try {
                                g_totalBytes = boost::lexical_cast<int>(argv[i]) * 1024 * 1024;
                                if (g_totalBytes <= 0) {
                                    g_totalBytes = 1; // to prevent reporint it as missing
                                    std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -b\n";
                                    argsOk = false;
                                }
                            } catch (...) {
                                std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -b\n";
                                argsOk = false;
                            }
                        }
                        break;
                    case 'c':
                        g_createTopLevelDir = true;
                        break;
                    case 'f':
                        g_flushData = true;
                        break;
                    case 'h':
                        g_displayHelp = true;
                        break;
                    case 'd':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -d option\n";
                            argsOk = false;
                        } else {
                            g_dir = argv[i];
                        }
                        break;
                    case 'k':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -k option\n";
                            argsOk = false;
                        } else {
                            try {
                                std::string bytes(argv[i]);
                                std::string::size_type idx = bytes.find_first_of(":");
                                if (std::string::npos != idx) {
                                    if (0 == idx) {
                                        std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -k\n";
                                        argsOk = false;
                                    } else {
                                        g_maxChunkBytes = boost::lexical_cast<int>(bytes.substr(0, idx)) * 1024;
                                        g_minChunkBytes = boost::lexical_cast<int>(bytes.substr(idx + 1)) * 1024;
                                        if (g_minChunkBytes > g_maxChunkBytes) {
                                            int tmp = g_minChunkBytes;
                                            g_minChunkBytes = g_maxChunkBytes;
                                            g_maxChunkBytes = tmp;
                                        }
                                    }
                                } else {
                                    g_maxChunkBytes = boost::lexical_cast<int>(bytes.c_str()) * 1024 * 1024;
                                    g_minChunkBytes = g_maxChunkBytes;
                                }
                                if (g_minChunkBytes <= 0 || g_maxChunkBytes <= 0) {
                                    argsOk = false;
                                    std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -k\n";
                                }
                            } catch (...) {
                                std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -k.\n";
                                argsOk = false;
                            }
                        }
                        break;
                    case 'l':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -l option\n";
                            argsOk = false;
                        } else {
                            try {
                                std::string bytes(argv[i]);
                                std::string::size_type idx = bytes.find_first_of(":");
                                if (std::string::npos != idx) {
                                    if (0 != idx) {
                                        g_maxDirLevel = boost::lexical_cast<int>(bytes.substr(0, idx));
                                    }
                                    g_maxDirCount = boost::lexical_cast<int>(bytes.substr(idx + 1));
                                } else {
                                    g_maxDirLevel = boost::lexical_cast<int>(bytes.c_str());
                                }
                                if (g_maxDirLevel <= 0 || g_maxDirCount <= 0) {
                                    std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -l\n";
                                    argsOk = false;
                                }
                            } catch (...) {
                                std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -l\n";
                                argsOk = false;
                            }
                        }
                        break;
                    case 'm':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -m option\n";
                            argsOk = false;
                        } else {
                            try {
                                std::string bytes(argv[i]);
                                std::string::size_type idx = bytes.find_first_of(":");
                                if (std::string::npos != idx) {
                                    if (0 != idx) {
                                        g_maxFileSizeBytes = boost::lexical_cast<int>(bytes.substr(0, idx)) * 1024 * 1024;
                                    }
                                    g_minFileSizeBytes = boost::lexical_cast<int>(bytes.substr(idx + 1)) * 1024;
                                    if (0 != idx) {
                                        if (g_minFileSizeBytes > g_maxFileSizeBytes) {
                                            std::size_t bytes = g_maxFileSizeBytes;
                                            g_maxFileSizeBytes = g_minFileSizeBytes;
                                            g_minFileSizeBytes = (int)bytes;
                                        }
                                    }

                                } else {
                                    g_maxFileSizeBytes = boost::lexical_cast<int>(bytes.c_str()) * 1024 * 1024;
                                }
                                if (g_minFileSizeBytes <= 0 || g_maxFileSizeBytes <= 0) {
                                    std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -m\n";
                                    argsOk = false;
                                }
                            } catch (...) {
                                std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -m\n";
                                argsOk = false;
                            }
                        }
                        break;
                    case 'r':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -r option\n";
                            argsOk = false;
                        } else {
                            try {
                                g_repeat = boost::lexical_cast<unsigned int>(argv[i]);
                            } catch (...) {
                                std::cout << "\n*** ERROR:\n" << argv[i] << " invalid value for -r\n";
                                argsOk = false;
                            }
                        }
                        break;
                    case 's':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -s option\n";
                            argsOk = false;
                        } else {
                            try {
                                g_seed = boost::lexical_cast<unsigned int>(argv[i]);
                            } catch (...) {
                                std::cout << "\n*** ERROR:\n"
                                          << argv[i]
                                          << " invalid value for -s. Must be a number greater than 0 and less than "
                                          << (std::numeric_limits<unsigned int>::max)()
                                          << '\n';
                                g_seed = 1;
                                argsOk = false;
                            }
                        }
                        break;
                    case 't':
                        ++i;
                        if (i >= argc || '-' == argv[i][0]) {
                            --i;
                            std::cout << "\n*** ERROR:\nmissing value for -t option\n";
                            argsOk = false;
                        } else {
                            g_type = argv[i];
                        }
                        break;
                    case 'v':
                        g_generate = false;
                        break;
                    default:
                        std::cout << "\n*** ERROR:\ninvalid arg: " << argv[i] << '\n';
                        argsOk = false;
                        break;
                }
            }
        }
        ++i;
    }
    if (g_displayHelp) {
        usage();
        return false;
    }
    if (0 == g_seed) {
        std::cout << "\n*** ERROR:\nmissing -s option\n";
        argsOk = false;
    }
    if (0 == g_totalBytes) {
        std::cout << "\n*** ERROR:\nmissing -b option\n";
        argsOk = false;
    }
    if (g_dir.empty()) {
        std::cout << "\n*** ERROR:\nmissing -d option\n";
        argsOk = false;
    } 
    if (!setGenerator()) {
        std::cout << "\n*** ERROR:\n" << g_type << " invalid value for -t.\n";
        argsOk = false;
    }
    if (0 == g_minFileSizeBytes) {
        std::cout << "\n*** ERROR: min file size can not be 0\n";
        argsOk = false;
    }
    if (0 == g_maxFileSizeBytes) {
        std::cout << "\n*** ERROR: max file size can not be 0\n";
        argsOk = false;
    }

    if (!argsOk) {
        usage();
    }
    return argsOk;
}

void createOrValidateDirs(int level, boost::filesystem::path& path)
{
    for (int i = 0; i < level; ++i) {
        std::string dir("dir");
        dir += boost::lexical_cast<std::string>(randomNumberRange(1, g_maxDirCount));
        path /= dir;
        if (g_generate) {
            boost::filesystem::create_directory(path);
        } else {
            if (!boost::filesystem::exists(path)) {
                std::cout << "\n*** ERROR:\n" << path << " not found\n";
                g_validationPassed = false;
                return;
            }
        }
    }
}

bool writeData(std::ofstream& outFile, char const* data, int length)
{
    outFile.write(data, length);
    if (g_flushData && outFile.good()) {
        outFile.flush();
    }
    return outFile.good();
}

bool createFile(boost::filesystem::path& fileName, unsigned int size)
{
    std::ofstream outFile(fileName.string().c_str(), std::ios::out | std::ios::binary);
    int chunkSize = g_minChunkBytes < g_maxChunkBytes? randomBufferSize(g_minChunkBytes, g_maxChunkBytes) : g_maxChunkBytes;
    std::vector<char> buffer(g_maxChunkBytes);
    unsigned int bytesWritten = 0;
    do {
        int bytesInBuffer = 0;
        do {
            int repeat (g_repeat > 0 ? (char)randomNumberMax(g_repeat) : (-1 == g_repeat ? 1 : size));
            int num = g_generator();
            do {
                buffer[bytesInBuffer] = (char)num;
                ++bytesWritten;
                ++bytesInBuffer;
                if (bytesInBuffer == chunkSize) {
                    if (!writeData(outFile, &buffer[0], bytesInBuffer)) {
                        std::cout << "\n*** ERROR:\nwriting to file " << fileName << ": " << errno << '\n';
                        g_validationPassed = false;
                        return false;
                    }
                    bytesInBuffer = 0;
                    chunkSize = g_minChunkBytes < g_maxChunkBytes? randomBufferSize(g_minChunkBytes, g_maxChunkBytes) : g_maxChunkBytes;
                }
            } while (--repeat > 0 && bytesWritten < size);
        } while (bytesInBuffer < chunkSize && bytesWritten < size);
        if (bytesInBuffer > 0) {
            if (!writeData(outFile, &buffer[0], bytesInBuffer)) {
                std::cout << "\n*** ERROR:\nwriting to file " << fileName << ": " << errno << '\n';
                g_validationPassed = false;
                return false;
            }
            chunkSize = g_minChunkBytes < g_maxChunkBytes? randomBufferSize(g_minChunkBytes, g_maxChunkBytes) : g_maxChunkBytes;
        }
    } while (bytesWritten < size);
    return true;
}

int readData(std::ifstream& inFile, char* data, int length)
{
    inFile.read(data, length);
    if (!inFile.good() && !inFile.eof()) {
        return -1;
    }
    return (int)inFile.gcount();
}

void validateFile(boost::filesystem::path& fileName, unsigned int size)
{
    std::ifstream inFile(fileName.string().c_str(), std::ios::out | std::ios::binary);
    if (!inFile.good()) {
        std::cout << "\n*** ERROR:\nunable to open file: " << fileName << ": " << errno << '\n';
        g_validationPassed = false;
        return;
    }
    inFile.seekg(0, std::ios_base::end);
    if (inFile.tellg() != (std::ifstream::pos_type)size) {
        g_validationPassed = false;
        std::cout << "\n*** ERROR:\n" << fileName << " size " << inFile.tellg() << " != " << size << '\n';
        return;
    }
    inFile.seekg(0, std::ios_base::beg);
    std::vector<char> buffer(g_chunkBytes);
    unsigned int i = 0;
    do {
        int bufferIdx = 0;
        int bytesRead = readData(inFile, &buffer[0], (int)buffer.size());
        if (-1 == bytesRead) {
            g_validationPassed = false;
            std::cout << "\n*** ERROR:\nreading " << fileName << ": " << errno << '\n';
            return;
        }
        do {
            int repeat (g_repeat > 0 ? (char)randomNumberMax(g_repeat) : (-1 == g_repeat ? 1 : size));
            int num = g_generator();
            do {
                if (buffer[bufferIdx] != (char)num) {
                    g_validationPassed = false;
                    std::cout << "\n*** ERROR:\n" << fileName << " does not match at offset " << i
                              << ".\nExpecting value 0x" << std::hex << (int)num << " but read value 0x" << (int)buffer[i] << '\n';
                    return;
                }
                ++bufferIdx;
                ++i;
                if (bufferIdx == bytesRead) {
                    bufferIdx = 0;
                    bytesRead = readData(inFile, &buffer[0], (int)buffer.size());
                    if (-1 == bytesRead) {
                        g_validationPassed = false;
                        std::cout << "\n*** ERROR:\nreading " << fileName << ": " << errno << '\n';
                        return;
                    }
                }
            } while (--repeat > 0 && i < size);
        } while (bufferIdx < bytesRead && i < size);
    } while (i < size);
}

bool createTopLevelDir()
{
    try {
        std::string dir;
        for (std::string::size_type i = 0; i < g_dir.size(); ++i) {
            if ('\\' == g_dir[i] || '/' == g_dir[i]) {
                if (dir.size() > 1 && ':' != dir[dir.size() - 1]) {
                    boost::filesystem::create_directory(dir);
                }
            }
            dir += g_dir[i];
        }
        boost::filesystem::create_directory(dir);
        return true;
    } catch (std::exception const& e) {
        std::cout << "\n*** ERROR:\ncreating dir " << g_dir << ": " << e.what() << '\n';
    }
    return false;
}

bool createTopLevelDirIfNeeded()
{
    if (boost::filesystem::exists(g_dir)) {
        return true;
    }
    if (g_createTopLevelDir) {
        return createTopLevelDir();;
    }
    char yn = 'n';
    do {
        std::cout << g_dir << " does not exist: Create it [y/n]: ";
        std::cin >> yn;
    } while (!('y' == yn || 'n' == yn));
    if ('y' == yn) {
        return createTopLevelDir();
    }
    return false;
}

int main(int argc, char* argv[])
{
    if (!parseCmd(argc, argv)) {
        return -1;
    }

    g_mt19937Generator.seed(g_seed);
    g_mt19937GeneratorBufferSize.seed(g_seed);
    unsigned int fileCount = 0;
    unsigned int dirCount = 0;
    int maxSize = g_maxFileSizeBytes - g_minFileSizeBytes;
    try {
        do {
            if (!createTopLevelDirIfNeeded()) {
                std::cout << "did not find/create " << g_dir << '\n';
                return -1;
            }
            boost::filesystem::path fileName(g_dir);
            int dirLevel = randomNumberMax(g_maxDirLevel);
            unsigned int size = (unsigned int)(g_totalBytes < g_maxFileSizeBytes ? g_totalBytes : ((randomNumberMax(maxSize) +  g_minFileSizeBytes) & ~(g_minFileSizeBytes - 1)));
            g_totalBytes -= size;
            if (dirLevel > 0) {
                createOrValidateDirs(dirLevel, fileName);
                if (!g_generate && !g_validationPassed) {
                    break;
                }
            }
            std::string name("file");
            name += boost::lexical_cast<std::string>(++fileCount);
            fileName /= name;
            if (g_generate) {
                if (!createFile(fileName, size)) {
                    break;
                }
            } else {
                if (!g_validationPassed) {
                    break;
                }
                validateFile(fileName, size);
                if (!g_validationPassed) {
                    break;
                }
            }
        } while (g_totalBytes > 0);
        if (!g_validationPassed) {
            std::cout << "*** VALIDATION FAILED\n";
            return -1;
        }
        return 0;
    } catch (std::exception const& e) {
        std::cout << "\n*** ERROR:\n" << e.what() << '\n';
    }
    if (!g_validationPassed) {
        std::cout << "*** VALIDATION FAILED\n";
        return -1;
    }
    std::cout << '\n';
    return -1;
}
