cd ..

export LRP=$PWD
echo $LRP
#source "export LRP="$PWD""
mkdir software
cd software


#Open MPI
wget "https://download.open-mpi.org/release/open-mpi/v3.0/openmpi-3.0.0.tar.gz"
tar -xvf openmpi-3.0.0.tar.gz
cd openmpi-3.0.0
./configure --prefix=${LRP}/software/ompi --enable-mpi-thread-multiple
make && make all install
cd ..
#Make sure that libmpi.so exists



#PIn
wget "https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz"
tar -xvf pin-2.14-71313-gcc.4.4.7-linux.tar.gz
cd ..

#Libxml2
FILE="/usr/include/libxml2"
if [ -d "$FILE" ]; then
    echo "$FILE Path (libxml2) exist"

else
	sudo apt-get install libxml2-dev
	echo -n "Do you want to install Libxml2 (y/n)? "
	read answer
	#answer="Y"
		if [ "$answer" != "${answer#[Yy]}" ] ;then
	   		echo Yes
	    		sudo apt-get install libxml2-dev
		else
	   		echo No		 
		fi
fi

#sudo apt-get install libxml2-dev

#git clone https://github.com/dananjayamahesh/primesim.git
cd primesim
#git checkout dimp

#Make sure all the paths are correct
#########################################################


#LRP COMMON STEPS from PRIMESIM directory
#source env.sh #Only works with /bin/shh not with /bin/bash
chmod 777 env.sh
source ./env.sh

if [ $? -eq 0 ]; then
    echo "Environment is setup"
else
    echo "Please check the primesim/env.sh"
	exit 1
fi

#Please check your libxml2
echo "Building Prime and LRP"
make -B

if [ $? -eq 0 ]; then
    	 echo "LRP and Primesim build success"
else
   
	echo "LRP and Primesim build fails. Please check all the software dependnecies, primesim/env.sh and run [sh build_primesim.sh]"
	exit 1
fi

echo "Building Prime and LRP Finished"
echo "Building Benchmarks"

export PBENCH_PATH="$LRP/pbench/syncbench"
pbench_path=pbench/syncbench
cd pbench/syncbench
make -B

if [ $? -eq 0 ]; then
    	 echo "Benchmark build success"
else
   
	echo "Benchmark build fails"
	exit 1
fi

cd ../..
echo "Finished Building Benchmarks: PBench"
echo "Build-Complete"
echo "If no errors You are ready to run the experiments. See README.md inside primesim"

echo "You are in the primesim path. run [sh run_all.sh simple-test]. But before you run make sure all the paths are corerct and exit in the primesim/env.sh file"
#RUN a simple test
#sh run_all.sh simple-test
