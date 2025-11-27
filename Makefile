# --- Variables ---
CXX := clang++
CXXFLAGS := -std=c++17 -I./metal-cpp -g
LDFLAGS := -framework Cocoa -framework Metal -framework QuartzCore -framework Foundation

# Directory for artifacts (ignored by git)
BUILD_DIR := build

# The name of your app (Stays in root)
TARGET := HelloMetal

# The name of your shader library (Moved to build dir)
METALLIB := $(BUILD_DIR)/default.metallib

# Source files
SRCS := main.mm Renderer.cpp

# Object files logic:
# 1. Start with SRCS (main.mm Renderer.cpp)
# 2. Change .cpp -> .o AND .mm -> .o (main.o Renderer.o)
# 3. Prepend the build directory (build/main.o build/Renderer.o)
OBJS := $(SRCS:.cpp=.o)
OBJS := $(OBJS:.mm=.o)
OBJS := $(addprefix $(BUILD_DIR)/, $(OBJS))

# --- Rules ---

# 1. The Default Rule
all: $(TARGET) $(METALLIB)

# 2. Link the executable
# Note: This writes $(TARGET) to the root, but reads $(OBJS) from build/
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# 3. Create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 4. Compile C++ files
# The "| $(BUILD_DIR)" part ensures the dir exists before compiling
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 5. Compile Objective-C++ files
$(BUILD_DIR)/%.o: %.mm | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 6. Compile Shaders
# Generates .air and .metallib inside the build folder
$(METALLIB): Shaders.metal | $(BUILD_DIR)
	xcrun -sdk macosx metal -c Shaders.metal -o $(BUILD_DIR)/Shaders.air
	xcrun -sdk macosx metallib $(BUILD_DIR)/Shaders.air -o $(METALLIB)
	rm $(BUILD_DIR)/Shaders.air

# 7. Clean up
# Simply removes the target and the entire build folder
clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)