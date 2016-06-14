// include file for project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#pragma message("Compiling precompiled headers.\n")

// standard c/c++ libraries
#include <string>
#include <iostream>
#include <cmath>  // trig, fmod(), modf()


// STL library
#include <vector>

// boost libraries
#include <boost/noncopyable.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/container/map.hpp>
#include <boost/container/set.hpp>

// SQLAPI++ libraries
#include "SQLAPI.h" // main SQLAPI++ header