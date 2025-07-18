lua/qalc.so: qalc.cpp
	g++ $< -shared -fPIC -o $@ -lqalculate
