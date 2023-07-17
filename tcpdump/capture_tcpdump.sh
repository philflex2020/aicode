#!/bin/bash
output_dir=/tmp/pcap
base_filename=test_pcap
stop_file=/tmp/run_test_pcap

interval=60
counter=1

mkdir -p ${output_dir}
touch  ${stop_file}

while [ -f ${stop_file} ]; do
    filename="${output_dir}/${base_filename}_${counter}.pcap"
    tcpdump -w $filename -G $interval
    ((counter++))
done

#Save the above script as capture_tcpdump.sh and make it executable using the following command:

#bash
#Copy code
#chmod +x capture_tcpdump.sh
#To run the script, execute the following command:

#bash
#Copy code
#./capture_tcpdump.sh