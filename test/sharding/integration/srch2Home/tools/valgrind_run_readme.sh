valgrind --leak-check=full --suppressions=./suppressions_srch2.supp --error-limit=no --track-origins=yes ../../../../../build/src/server/srch2-search-server --config=$1
