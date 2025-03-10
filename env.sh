#!/bin/bash

#Copyright (c) 2015 Princeton University
#All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of Princeton University nor the
#      names of its contributors may be used to endorse or promote products
#      derived from this software without specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY PRINCETON UNIVERSITY "AS IS" AND
#ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#DISCLAIMED. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
#DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# home directory for PriME, set it to the correct path
export LRP=$(cd .. && pwd && cd primesim/)
export PRIME_PATH=${LRP}/primesim

# home directory for Pin, set it to the correct path
export PINPATH=${LRP}/software/pin-2.14-71313-gcc.4.4.7-linux
export PATH=${LRP}/software/pin-2.14-71313-gcc.4.4.7-linux:$PATH

#export PINPATH=/home/mahesh/mahesh/edinburgh/repos/softwares/pin/pin-3.2-81205-gcc-linux
#export PATH=/home/mahesh/mahesh/edinburgh/repos/softwares/pin/pin-3.2-81205-gcc-linux:$PATH

#OpenMPI Intallation Path
#export PATH="$PATH:/usr/bin"
#export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/lib64/"

# path to libmpi.so in OpenMPI library, set it to the correct path with the same 
# format as below (including those backslashes and quotes). Besides, you need to 
# make sure the library file and the default mpic++ used in the Makefile are of 
# the same version

export PATH="$PATH:${LRP}/software/ompi/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:${LRP}/software/ompi/lib/"
export OPENMPI_LIB_PATH=\"\\\"${LRP}/software/ompi/lib/libmpi.so\\\"\"

#export OPENMPI_LIB_PATH=\"\\\"/usr/local/lib/libmpi.so\\\"\"
#export OPENMPI_LIB_PATH=\"\\\"/usr/lib/libmpi.so.12\\\"\"
#export OPENMPI_LIB_PATH=\"\\\"/usr/local/lib/libmpi.so\\\"\"
#export OPENMPI_LIB_PATH=\"\\\"/usr/local/lib/libmpi.so\\\"\"
# path to libxml2 , set it to the correct path
export LIBXML2_PATH=/usr/include/libxml2

# path to PARSEC benchmarks, set it to the correct path if you want to run PARSEC
# export PARSEC_PATH=${HOME}/repos/parsec-3.0

# set path
export PATH=$PRIME_PATH/tools:$PATH
export PATH=$PRIME_PATH/tools/shared-llc:$PATH
