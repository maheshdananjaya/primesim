DIMP_FILE=${HOME}/repos/primesim/output/stat.txt
DATA_FILE=${HOME}/repos/primesim/output/syncbench-fixed/data_2.txt
DATA_FILE3=${HOME}/repos/primesim/output/syncbench-fixed/data_3.txt

#echo "" > ${DIMP_FILE}
> ${DIMP_FILE}
> ${DATA_FILE}

make -B
#SyncMap is 10000

#for upd in 50 100
#do

for threads in 8
do
	for rate in 100 500 1000 2000
	do
		#for rate2 in 200 400 600 800 1000 1200 1400 1600 1800 2000 10000
		for rate2 in 200 1000 2000 4000
		do
			
			if [ $rate2 -lt $rate ] ; then 
				continue 
			fi
			
			for bench in hashmap bstree skiplist-rotating-c linkedlist
			do	
				echo "$bench,$threads,$rate,$rate2" >> ${DATA_FILE}
				for pmodel in 0 3 4
				do
					echo "Executing $pmodel"
					#echo "$bench,$pmodel," >> ${DIMP_FILE}
					#echo "$pmodel" >> ${DATA_FILE}
					
					mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 \
					-np 1 ${HOME}/repos/primesim/bin/prime \
					${HOME}/repos/primesim/xml/config_${pmodel}.xml \
					${HOME}/repos/primesim/output/syncbench-fixed/config.out : \
					-np 1 pin.sh -ifeellucky -t ${HOME}/repos/primesim/bin/prime.so \
					-c ${HOME}/repos/primesim/xml/config_${pmodel}.xml \
					-o ${HOME}/repos/primesim/output/syncbench-fixed/config.out \
					-- ${HOME}/repos/primesim/pbench/syncbench/bin/${bench} -t ${threads} -i ${rate} -r ${rate2} -o ${rate2} -d 10 -x 6 -u 50
					
					cat ${DIMP_FILE} >> ${DATA_FILE3}
					cat ${DIMP_FILE} >> ${DATA_FILE}

					
					cat ${HOME}/repos/primesim/output/syncbench-fixed/config.out_1 \
					${HOME}/repos/primesim/output/syncbench-fixed/config.out_0 \
					> ${HOME}/repos/primesim/output/syncbench-fixed/config.out
					
					cp  ${HOME}/repos/primesim/output/syncbench-fixed/config.out \
					 ${HOME}/repos/primesim/output/syncbench-fixed/config_p.out
					
					rm -f  ${HOME}/repos/primesim/output/syncbench-fixed/config.out_0
					
					rm -f  ${HOME}/repos/primesim/output/syncbench-fixed/config.out_1
				done
			   
			done
		done
	done
done
#done


