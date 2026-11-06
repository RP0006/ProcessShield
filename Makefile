CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I./src

# Main application sources and objects
SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = obj

SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/ProcessManager.cpp \
       $(SRC_DIR)/ResourceMonitor.cpp \
       $(SRC_DIR)/CPUAnalyzer.cpp \
       $(SRC_DIR)/MemoryAnalyzer.cpp \
       $(SRC_DIR)/ProcessController.cpp \
       $(SRC_DIR)/DeadlockSimulator.cpp \
       $(SRC_DIR)/WaitForGraph.cpp \
       $(SRC_DIR)/BankersAlgorithm.cpp \
       $(SRC_DIR)/AlertManager.cpp \
       $(SRC_DIR)/Logger.cpp \
       $(SRC_DIR)/Agent.cpp

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

TARGET = processshield
AGENT_TARGET = processshield-agent

AGENT_SRCS = $(SRC_DIR)/agent_main.cpp \
             $(SRC_DIR)/Agent.cpp \
             $(SRC_DIR)/ProcessManager.cpp

AGENT_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(AGENT_SRCS))

TEST_BINARIES = $(TEST_DIR)/cpu_stress $(TEST_DIR)/memory_test $(TEST_DIR)/deadlock_test

.PHONY: all tests agent clean

all: $(TARGET) $(AGENT_TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)
	@echo "Build successful: $(TARGET)"

$(AGENT_TARGET): $(AGENT_OBJS)
	$(CXX) $(CXXFLAGS) $(AGENT_OBJS) -o $(AGENT_TARGET)
	@echo "Build successful: $(AGENT_TARGET)"

agent: $(AGENT_TARGET)

tests: $(TEST_BINARIES)
	@echo "All test binaries built successfully."

$(TEST_DIR)/cpu_stress: $(TEST_DIR)/cpu_stress.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

$(TEST_DIR)/memory_test: $(TEST_DIR)/memory_test.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

$(TEST_DIR)/deadlock_test: $(TEST_DIR)/deadlock_test.cpp \
                          $(SRC_DIR)/WaitForGraph.cpp \
                          $(SRC_DIR)/DeadlockSimulator.cpp \
                          $(SRC_DIR)/BankersAlgorithm.cpp \
                          $(SRC_DIR)/AlertManager.cpp \
                          $(SRC_DIR)/Logger.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(AGENT_TARGET) $(TEST_BINARIES)
	@echo "Clean completed."

