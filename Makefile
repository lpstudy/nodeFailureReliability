sim_predict: mttdl_sim.o
	g++ -O3  -o sim-predict  -lgsl -lgslcblas mttdl_sim.o  
mttdl_sim.o: mttdlsim_3fuben.cpp
	g++ -O3 -Wall -std=c++11 -o mttdl_sim.o -g -c mttdlsim_3fuben.cpp

ori: ori_mttdl_sim.o
	gcc -O3 -o ori-sim-predict -lgsl -lgslcblas ori_mttdl_sim.o 
ori_mttdl_sim.o: ori_mttdlsim_3fuben.c
	gcc -O3 -Wall -o ori_mttdl_sim.o -g -c ori_mttdlsim_3fuben.c 

debug: mttdl_sim_debug.o
	g++ -O3  -o sim-predict  -lgsl -lgslcblas mttdl_sim_debug.o  
mttdl_sim_debug.o: mttdlsim_3fuben.cpp
	g++ -O3 -Wall -DTDEBUG -std=c++11 -o mttdl_sim_debug.o -g -c mttdlsim_3fuben.cpp

clean:
	rm -rf mttdl_sim.o sim-predict
