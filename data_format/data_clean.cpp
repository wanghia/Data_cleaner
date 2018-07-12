#include <stdio.h>
#include <locale>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <utility>
#include <unordered_map>
#include <limits>

#include <time.h>

#include "MurmurHash3.h"

enum Oflag : int {
    NUM = 0,
    CAT = 1,
    MULTI_CAT = 2,
    TIME = 3,
    MULTI_CAT_NUM = 4,
    LABEL = 5,
    IGNORE = 99
};

enum CatnumFlag : int {
    MAX = 0,
    MIN = 1,
    MAXMIN = 2
};

void trim_tokens(std::vector<std::pair<char*, size_t>>& tokens) {
    for (auto& token : tokens) {
        if (token.second == 0u) {
            continue;
        }
        auto& ptr = token.first;
        size_t size = 0u;
        while (*ptr == ' ') {
            ++ptr;
            ++size;
        }
        char* pptr = ptr + token.second - 1;
        while (pptr != ptr && *pptr == ' ') {
            *pptr = '\0';
            ++size;
            --pptr;
        }
        token.second -= size;
    }
}

std::vector<std::pair<char*, size_t>> split(char* str, char delim) {
    if (str == nullptr) {
        return {};
    }
    std::vector<std::pair<char*,size_t>> tokens;
    std::vector<size_t> lens;
    tokens.push_back({str,0u});
    size_t size = 0u;
    while (*str != '\0') {
        ++size;
        if (*str == delim) {
            *str = '\0';
            tokens.push_back({str + 1, 0u});
            lens.push_back(size-1);
            size = 0u;
        }
        ++str;
    }
    lens.push_back(size);
    for (size_t i = 0u; i < tokens.size(); ++i) {
        tokens[i].second = lens[i];
    }
    trim_tokens(tokens);
    return tokens;
}

class FileLineReader {
public:
    ~FileLineReader() {free(_buffer);}

    char* getline(FILE* file) {
        if (file == nullptr) {
            return nullptr;
        }
        char delim = '\n';
        ssize_t ret = ::getdelim(&_buffer, &_size, delim, file);
        if (ret == -1) {
            _size = 0;
            return nullptr;
        } else if (ret >= 1) {
            if (_buffer[ret - 1] == delim) {
                _buffer[ret - 1] = '\0';
                _size = ret - 1;
            } else {
                _size = ret;
            }
            return _buffer;
        }
    }

    char* buffer() {
        return _buffer;
    }

    size_t size() {
        return _size;
    }

private:
    char* _buffer = nullptr;
    size_t _size = 0u;
};

time_t calc_time(const char* str, const char* format) {
    std::tm tmp_time = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tmp_time, format);
    if (ss.fail()) {
        std::cerr << "fail to covert time[" << str << "]" << std::endl;
        return (time_t)-1;
    }
    return std::mktime(&tmp_time);
}

int parse_feature_flags(const char* filename,
        std::vector<Oflag>& oflags,
        std::unordered_map<size_t, std::vector<char>>& delims,
        std::unordered_map<size_t, std::string>& time_formats,
        std::unordered_map<size_t, CatnumFlag>& catnum_flags) {
    if (filename == nullptr) {
        std::cerr << "empty filename";
        return -1;
    }
    FILE* file = fopen(filename, "r");

    if (file == nullptr) {
        std::cerr << "Open feature flags file [" << filename << "] failed.";
        return -1;
    }
    FileLineReader flags_reader;
    char* line = nullptr;
    constexpr size_t MVC_LEN = std::strlen("Multi-Valued Categorical");
    constexpr size_t MVCN_LEN = std::strlen("Multi-Valued CatNumerical");
    while (line = flags_reader.getline(file)) {
        if (strcmp(line, "Numerical") == 0) {
            oflags.push_back(Oflag::NUM);
        } else if (strcmp(line, "Categorical") == 0) {
            oflags.push_back(Oflag::CAT);
        } else if (strncmp(line, "Multi-Valued Categorical", MVC_LEN) == 0) {
            auto tokens = split(line, '#');
            if (tokens.size() != 2) {
                std::cerr << "For Multi-Valued Categorical feature you should specify the delim" << std::endl;
                fclose(file);
                return -1;
            }
            oflags.push_back(Oflag::MULTI_CAT);
            bool is_success = delims.insert({oflags.size()-1, {*tokens[1].first}}).second;
            if (!is_success) {
                std::cerr << "add Multi-Valued Categorical delim failed." << std::endl;
                fclose(file);
                return -1;
            }
        } else if (strncmp(line, "Multi-Valued CatNumerical", MVCN_LEN) == 0) {
            auto tokens = split(line, '#');
            if (tokens.size() != 4) {
                std::cerr << "For Multi-Valued CatNumerical you should specify Max or Min or MaxMin. and the delims(token delim and cat value delim)" << std::endl;
                fclose(file);
                return -1;
            }
            oflags.push_back(Oflag::MULTI_CAT_NUM);
            CatnumFlag cnflag = CatnumFlag::MAX;
            if (strcmp(tokens[1].first, "Max") == 0) {
                cnflag = CatnumFlag::MAX;
            } else if (strcmp(tokens[1].first, "Min") == 0) {
                cnflag = CatnumFlag::MIN;
            } else if (strcmp(tokens[1].first, "MaxMin") == 0) {
                cnflag = CatnumFlag::MAXMIN;
            } else {
                std::cerr << "It can only be Max or Min or MaxMin, but [" << tokens[1].first << "]" << std::endl;
                fclose(file);
                return -1;
            }
            bool is_success = catnum_flags.insert({oflags.size()-1, cnflag}).second;
            if (!is_success) {
                std::cerr << "add CatNumerical calculation method failed." << std::endl;
                fclose(file);
                return -1;
            }
            is_success = delims.insert({oflags.size()-1, {*tokens[2].first, *tokens[3].first}}).second;
            if (!is_success) {
                std::cerr << "add CatNumerical delim failed." << std::endl;
                fclose(file);
                return -1;
            }
        } else if (strcmp(line, "Label") == 0) {
            oflags.push_back(Oflag::LABEL);
        } else if (strncmp(line, "Time", 4) == 0) {
            auto tokens = split(line, '#');
            if (tokens.size() != 2) {
                std::cerr << "For Time you should specify a format." << std::endl;
                fclose(file);
                return -1;
            }
            oflags.push_back(Oflag::TIME);
            bool is_success = time_formats.insert({oflags.size()-1, tokens[1].first}).second;
            if (!is_success) {
                std::cerr << "add time format failed." << std::endl;
                fclose(file);
                return -1;
            }
        } else if (strcmp(line, "Ignore") == 0) {
            oflags.push_back(Oflag::IGNORE);
        } else {
            std::cerr << "unknown flag: " << line << std::endl;
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <Feature Flags>" << std::endl;
        return -1;
    }

    std::vector<Oflag> oflags;
    std::unordered_map<size_t, std::vector<char>> delims;
    std::unordered_map<size_t, std::string> time_formats;
    std::unordered_map<size_t, CatnumFlag> catnum_flags;

    if (parse_feature_flags(argv[1], oflags, delims, time_formats, catnum_flags) != 0) {
        std::cerr << "Parse feature flag file failed." << std::endl;
        return -1;
    }

    FileLineReader reader;
    unsigned int seed = 32u;

    char odelim = ' ';
    char* line = nullptr;
    while (line = reader.getline(stdin)) {
        auto tokens = split(line, '\t');
        if (tokens.size() != oflags.size()) {
            std::cerr << "Error Line NF= " << tokens.size() << std::endl;
        }
        for (size_t i = 0u; i < tokens.size(); ++i) {
            if (oflags[i] == Oflag::IGNORE) { 
                continue;
            }
            if (oflags[i] == Oflag::LABEL) { 
                std::cerr << tokens[i].first << std::endl;
                continue;
            }
            if (strcmp(tokens[i].first, "null") == 0 || tokens[i].second==0) {
                std::cout << "NaN";
                if (oflags[i] == Oflag::MULTI_CAT_NUM) {
                    if (catnum_flags[i] == CatnumFlag::MAX || catnum_flags[i] == CatnumFlag::MIN) {
                        std::cout <<  odelim << "NaN";
                    } else if (catnum_flags[i] == CatnumFlag::MAXMIN) {
                        std::cout << odelim << "NaN" << odelim << "NaN";
                    }
                }
                if (i + 1 != tokens.size()) {
                    std::cout << odelim;
                }
                continue;
            }
            if (oflags[i] == Oflag::NUM) { 
                std::cout << tokens[i].first;
            } else if (oflags[i] == Oflag::CAT) { 
                uint64_t sign = MurmurHash64A(tokens[i].first, tokens[i].second, seed);
                std::cout << sign;
            } else if (oflags[i] == Oflag::MULTI_CAT) {
                auto subtokens = split(tokens[i].first, delims[i][0]);
                for (size_t j = 0u; j < subtokens.size(); ++j) {
                    uint64_t sign = MurmurHash64A(subtokens[j].first, subtokens[j].second, seed);
                    std::cout << sign;
                    if (j + 1 != subtokens.size()) {
                        std::cout << ',';
                    }
                }
            } else if (oflags[i] == Oflag::MULTI_CAT_NUM) {
                auto subtokens = split(tokens[i].first, delims[i][0]);
                double max = std::numeric_limits<double>::lowest();
                double min = std::numeric_limits<double>::max();
                uint64_t max_sign = 0u, min_sign = 0u;
                for (size_t j = 0u; j < subtokens.size(); ++j) {
                    auto subsubtokens = split(subtokens[j].first, delims[i][1]);
                    if (subsubtokens.size() != 2) {
                        std::cerr << "There should be CAT:VALUE for CatNumerical" << std::endl;
                    }
                    uint64_t sign = 
                        MurmurHash64A(subsubtokens[0].first, subsubtokens[0].second, seed);
                    std::cout << sign;
                    if (j + 1 != subtokens.size()) {
                        std::cout << ',';
                    }
                    char* end = nullptr;
                    double num = std::strtod(subsubtokens[1].first, &end);
                    if (end == nullptr || errno != 0) {
                        std::cerr << "error value format, transform to double failed, [" << subsubtokens[1].first << "]" << std::endl;
                    }
                    if (num >= max) {
                        max = num;
                        max_sign = sign;
                    }

                    if (num <= min) {
                        min = num;
                        min_sign = sign;
                    }
                }
                if (catnum_flags[i] == CatnumFlag::MAX) {
                    std::cout << odelim << max_sign;
                } else if (catnum_flags[i] == CatnumFlag::MIN) {
                    std::cout << odelim << min_sign;
                } else if (catnum_flags[i] == CatnumFlag::MAXMIN) {
                    std::cout << odelim << max_sign << odelim << min_sign;
                }
            } else if (oflags[i] == Oflag::TIME) {
                auto t = calc_time(tokens[i].first, time_formats[i].c_str());
                std::cout << t;
            }

            if (i+1 != tokens.size()) {
                std::cout << odelim;
            }
        }
        std::cout << std::endl;
    }
}
