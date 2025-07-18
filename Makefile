lua/qalculate/qalc.so: lib.cpp
	g++ $< -shared -fPIC -o $@ -lqalculate
