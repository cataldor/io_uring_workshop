#!/bin/sh

NB_CKSUM=$(find "$1" -type f -exec md5sum {} \; | awk '{print $1}' | uniq | wc -l)

if [ "$NB_CKSUM" -ne 1 ]; then
	printf "Testsuite failed!\n";
	printf "Number of checksums %d\n", "$NB_CKSUM";
else
		printf "Testsuite OK!\n";
fi

