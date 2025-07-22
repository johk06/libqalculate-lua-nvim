BUILD = build
DEST = lua/qalculate/qalc.so
SRC = $(wildcard *.cpp)
OBJ = $(addprefix $(BUILD)/, $(addsuffix .o, $(basename $(SRC))))

$(BUILD)/%.o: %.cpp
	mkdir -p build
	$(CXX) -c -fPIC -o $@ $< -lqalculate -Wall


$(DEST): $(OBJ)
	g++ -shared $(OBJ) -o $@ -lqalculate -Wall

clean:
	rm -rf $(BUILD)
	rm -f $(DEST)
