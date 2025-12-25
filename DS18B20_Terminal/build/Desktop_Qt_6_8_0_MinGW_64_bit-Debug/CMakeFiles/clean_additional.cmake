# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\DS18B20Terminal_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\DS18B20Terminal_autogen.dir\\ParseCache.txt"
  "DS18B20Terminal_autogen"
  )
endif()
