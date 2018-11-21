sim_predict: mttdl_sim.o
	gcc -O3  -o sim_predict  -lgsl -lgslcblas mttdl_sim.o  
mttdl_sim.o: mttdlsim_3fuben.c
	gcc -O3 -Wall -o mttdl_sim.o -g -c mttdlsim_3fuben.c
clean:
	rm -rf mttdl_sim.o sim_predict
