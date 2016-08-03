CXXSTD = -std=c++14
CXXOPTFLAGS = -O3
CXXWARNFLAGS = -Wall -Wno-reserved-id-macro -Wno-unused-macros -Wno-c++98-compat -Werror -Wfatal-errors
CXXFLAGS = $(CXXSTD) $(CXXOPTFLAGS) $(CXXWARNFLAGS) $(CPPFLAGS)

.PHONY: all
all: proglog

proglog: proglog.cpp

.PHONY: clean
clean:
	$(RM) proglog
