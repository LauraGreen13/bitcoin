#!/bin/bash

str1="utxo_no"
str2="CCoins"
output_file=$1
echo $output_file

sudo rm $output_file
touch $output_file
for i in {500..10000..100}
do
	var="$(sudo src/bench/bench_bitcoin -testnet -utxo_no="$i")"
	#echo "${var}"
	while read -r line
	do
    		if [[ $line == $str2* ]] || [[ $line == $str1* ]]
		then
			echo "$line" >> $output_file
		fi
	done <<< "${var}"
done

