BUILD = build
DEST = lua/qalculate/qalc.so
SRC = $(wildcard *.cpp)
OBJ = $(addprefix $(BUILD)/, $(addsuffix .o, $(basename $(SRC))))

$(BUILD)/%.o: %.cpp
	mkdir -p build
	$(CXX) -c -fPIC -o $@ $< -lqalculate


$(DEST): $(OBJ)
	g++ -shared $(OBJ) -o $@ -lqalculate

clean:
	rm -rf $(BUILD)
	rm -f $(DEST)
