.PHONY: all clean

BUILD := build
SRC_NOR := nffcreator
SRC_NDR := ndrcreator
SRC_ENT := entryreader

all: $(BUILD)/$(SRC_NOR) $(BUILD)/$(SRC_NDR) $(BUILD)/$(SRC_ENT)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/$(SRC_NDR): $(SRC_NDR)/main.cpp | $(BUILD)
	g++ $< -o $@ -std=c++17 -lz

$(BUILD)/$(SRC_ENT): $(SRC_ENT)/main.cpp | $(BUILD)
	g++ $< -o $@ -std=c++17

$(BUILD)/$(SRC_NOR): $(SRC_NOR)/main.cpp | $(BUILD)
	g++ $< -o $@ -std=c++17

clean:
	rm -rf $(BUILD)
