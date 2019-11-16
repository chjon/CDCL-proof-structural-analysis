#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

static int clamp (int min, int val, int max) {
    return (val < min) ? (min) : (val > max) ? (max) : (val);
}

static bool isPrefix(const std::string& str, const std::string& prefix) {
    for (int i = 0; i < str.length() && i < prefix.length(); ++i) {
        if (str[i] != prefix[i]) return false;
    }

    return true;
}

static void shuffle(int* list, int length, int shuffleFirstN) {
    shuffleFirstN = clamp(0, shuffleFirstN, length);

    for (int i = 0; i < shuffleFirstN; ++i) {
        const int indexToSwap = static_cast<int>(std::rand() / static_cast<double>(RAND_MAX) * (length - i)) + i; 
        std::swap(list[i], list[indexToSwap]);
    }
}

// Restrict the bits in a CNF encoding of a SHA1 instance
int main (int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <num bits to restrict> <input file> <output file>" << std::endl;
        return 1;
    }

    const int argNumToRestrict = atoi(argv[1]);
    const int numToRestrict = clamp(0, std::abs(argNumToRestrict), 512);
    const std::string headerPrefix = "p cnf ";

    std::ifstream cnfFile(argv[2]);
    std::ofstream outfile(argv[3]);
    std::string line;
    while (std::getline(cnfFile, line)) {
        if (isPrefix(line, headerPrefix)) {
            std::istringstream iss(line.substr(headerPrefix.length()));
            int numVariables = 0;
            int numClauses = 0;
            if (!(iss >> numVariables >> numClauses)) {
                return 1;
            }

            outfile << headerPrefix << numVariables << " " << numClauses + numToRestrict << std::endl;
        } else {
            outfile << line << std::endl;
        }
    }

    if (argNumToRestrict < 0) {
        // Randomly set n bits
        const int totalNumBits = 512;
        int randomizedOrder[totalNumBits];
        for (int i = 0; i < totalNumBits; ++i) {
            randomizedOrder[i] = i;
        }

        shuffle(randomizedOrder, totalNumBits, numToRestrict);
        for (int i = 0; i < numToRestrict; ++i) {
            const int sign = (std::rand() & 0b1) ? (1) : (-1);
            outfile << sign * (randomizedOrder[i] + 1) << " " << 0 << std::endl;
        }
    } else {
        // Force first n bits to be zero
        for (int i = 0; i < numToRestrict; ++i) {
            outfile << -(i + 1) << " " << 0 << std::endl;
        }
    }

    return 0;
}