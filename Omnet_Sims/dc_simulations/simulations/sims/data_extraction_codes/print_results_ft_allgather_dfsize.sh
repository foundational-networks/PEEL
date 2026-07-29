python3 Compressor.py

echo -e "\n\nPrinting results into archive/ft_allgather_dfsize.txt\n"

cp VARIABLES_ft_allgather_dfsize.py VARIABLES.py
python3 response_time_constant_load.py > archive/ft_allgather_dfsize.txt

echo "Done!\n\n"