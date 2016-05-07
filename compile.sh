#!/bin/sh

COLOR="\e[1;36m"
COLOROK="\e[1;92m"
COLORNO="\e[1;91m"
FINCOLOR="\e[00m"

function ProgressBar {
    let _progress=(${1}*100/${2}*100)/100
    let _done=(${_progress}*4)/10
    let _left=40-$_done
    _fill=$(printf "%${_done}s")
    _empty=$(printf "%${_left}s")
printf "\rCompilando : [${_fill// /#}${_empty// /-}] ${_progress}%%"

}

_start=1
_end=100

echo
echo "Compilador de Agente iNotify"
echo 
for number in $(seq ${_start} ${_end})
do
    sleep 0.01
    ProgressBar ${number} ${_end}
done

make 2> ./compile.log 1> /dev/null

if [ -f iNotify ]
then
        echo 
        rm -rf src makefile
        echo -e "Compilacion de Agente iNotify. -> [ ${COLOROK}OK${FINCOLOR} ]"
	echo
else
        echo 
        echo -e "Compilacion de Agente iNotify. -> [ ${COLORNO}FAILED${FINCOLOR} ]"
	echo
fi

rm -f src/*.o
