#pragma once

#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include <thread>
#include <memory>


#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif
#define debug_print(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

#ifndef INFO_PRINT
#define INFO_PRINT 0
#endif
#define info_print(fmt, ...)                                           \
  do { if (INFO_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)



using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::size_t;
using namespace std::chrono_literals;


