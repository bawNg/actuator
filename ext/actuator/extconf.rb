require 'mkmf'

$CXXFLAGS += " -std=c++11 "

create_makefile 'actuator/actuator'