CXX = g++
CXXFLAGS = -std=c++20 -g -O0 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS = -lpthread -lsqlite3 -fsanitize=address

SOURCES = main.cpp librarian.cpp readHistory.cpp archiveMonitor.cpp dbHandler.cpp JobAnalysisUtils.cpp sha256.c
TARGET = myHistoryList

# Database files to clean
DB_FILES = tester_output.db tester_output.db-journal tester_output.db-wal tester_output.db-shm

# Default target
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

# Clean executables and database files
clean:
	rm -f $(TARGET)
	rm -f $(DB_FILES)

# Force rebuild (clean + build)
rebuild: clean $(TARGET)

.PHONY: clean rebuild