#!/bin/bash

GLOBAL_DIR_OBJS="objs"

mkdir -p ${GLOBAL_DIR_OBJS}

# prepare the depends tools
# st-1.9
if [[ -f ${GLOBAL_DIR_OBJS}/st-1.9/obj/libst.a && -f ${GLOBAL_DIR_OBJS}/st-1.9/obj/libst.so ]]; then
	echo "st-1.9t is ok.";
else
	echo "build st-1.9t"; 
	(rm -rf ${GLOBAL_DIR_OBJS}/st-1.9 && cd ${GLOBAL_DIR_OBJS} && unzip ../third_party/st-1.9.zip && cd st-1.9 && make linux-debug)
fi
