#I am fixing few bugs. 
# 1. NOP does not need any write backs.
#2 rp replacement
#3 core id delay
#4 remove release persistency  replacement writeback.
#remove delay from natural write back. it may have effected core 1

#balanced, addony, mremoved
optype=balanced
#RUn History
#syncbench-mbank-120cycles-nopfixed-4 ->initial run after fixing rp writeback delays
#syncbench-mbank-120cycles-nopfixed-4-1 ->same
#syncbench-mbank-120cycles-nopfixed-4-2 ->add more counters for write backs and epoch sizes, add thread 1,
#CONF_NAME=syncbench-mbank-120cycles-nopfixed-6 original with u 100
CONF_NAME=syncbench-msocket-120cycles-nopfixed-6
CONF_PATH=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/
DIMP_FILE=${HOME}/repos/primesim/output/${optype}/stat.txt
DATA_FILE=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/data_2.txt
DATA_FILE3=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/data_3.txt


if [ ! -d ${CONF_PATH} ] 
then
    mkdir -p ${CONF_PATH}
fi 

#echo "" > ${DIMP_FILE}
#> ${DIMP_FILE}
> ${DATA_FILE}

#make -B
#SyncMap is 10000

#for upd in 50 100
#do
#make -B



rate=1000
rate2=64000

#for threads in 8 16 32
for operations in 1000 4000 
do
	#for operations in 1000 4000
	for threads in 8 1 16 32
	do
			
			for bench in linkedlist hashmap bstree skiplist lfqueue
			#for bench in bstree skiplist-rotating-c
			do	
				echo "$bench,$threads,$rate,$rate2,$operations" >> ${DATA_FILE}
				for pmodel in 0 3 4 6
				do
					echo "Executing $pmodel"
					#operations=${rate2}
					#echo "$bench,$pmodel," >> ${DIMP_FILE}
					#echo "$pmodel" >> ${DATA_FILE}
					
					mpiexec --verbose --display-map --display-allocation -mca btl_sm_use_knem 0 \
					-np 1 ${HOME}/repos/primesim/bin/prime \
					${HOME}/repos/primesim/xml/config_msocket_120cycles_${pmodel}.xml \
					${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out : \
					-np 1 pin.sh -ifeellucky -t ${HOME}/repos/primesim/bin/prime.so \
					-c ${HOME}/repos/primesim/xml/config_msocket_120cycles_${pmodel}.xml \
					-o ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out \
					-- ${HOME}/repos/primesim/pbench/syncbench/bin/${bench}_${optype} -t ${threads} -i ${rate} -r ${rate2} -o ${operations} -d 10 -x 6 -u 100
					
					#cat ${DIMP_FILE} >> ${DATA_FILE3}
					DIMP_FILE=${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_stat
					cat ${DIMP_FILE} >> ${DATA_FILE}
					
					cat ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1 \
					${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0 \
					> ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out
										
					rm -f  ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_0
					
					rm -f  ${HOME}/repos/primesim/output/${optype}/${CONF_NAME}/config_${threads}_${rate}_${rate2}_${operations}_${bench}_${pmodel}.out_1
					rm -f ${DIMP_FILE}
				done
			   
			done
		
	done
done
