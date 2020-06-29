add_library(defuse MODULE DefUse.cpp DefUse.h httpanalyzer.cpp httpanalyzer.h)
add_library(postgresql MODULE PostgresqlAnalyzer.cpp PostgresqlAnalyzer.h)
add_library(map MODULE configurationMap.cpp configurationMap.cpp)
add_library(squid MODULE squidanalyzer.cpp squidanalyzer.h)