#!/bin/sh
#Script de instalacion para iNotifyd.
#Octubre 2015
#GLP
#by eFeet

COLOR="\e[1;36m"
COLOROK="\e[1;92m"
COLORNO="\e[1;91m"
FINCOLOR="\e[00m"

echo -e "${COLOR} _     __      _   _  __           _ "
echo -e "(_) /\ \ \___ | |_(_)/ _|_   _  __| |"
echo -e "| |/  \/ / _ \| __| | |_| | | |/ _\` |"
echo -e "| / /\  / (_) | |_| |  _| |_| | (_| |"
echo -e "|_\_\ \/ \___/ \__|_|_|  \__, |\__,_|"
echo -e "                         |___/        ${FINCOLOR}"
echo
echo "! Iniciando instalacion de iNotifyd !"
echo

if [ -f install.cfg ]; then
	USERNAME=`cat install.cfg | grep user | awk -F= '{ print $2 }'`
	INSTALLPATH=`cat install.cfg | grep installpath | awk -F= '{ print $2 }'`
	LOGPATH=$INSTALLPATH/inotify/logs
	PIDPATH=$INSTALLPATH/inotify/lock
	CFGPATH=$INSTALLPATH/inotify/conf
	echo -e "Archivo de instalacion "install.cfg" -> [ ${COLOROK}OK${FINCOLOR} ]"
	echo
else
	echo
	echo -e "Archivo de instalacion "install.cfg" no exite -> [ ${COLORNO}FAILED${FINCOLOR} ]"
	echo
	exit 1
fi

if [ ! -f inotify.cfg ]
then
        # Creando y modificando archivo de configuracion.
        echo "Preparando archivo inotify.cfg"
        echo
        echo "logpath="$LOGPATH"/inotify.log" > inotify.cfg
        echo "pidpath="$PIDPATH"/inotify.pid" >> inotify.cfg
        echo "logverbose=1" >> inotify.cfg
        echo "ipconsole=180.181.129.195" >> inotify.cfg
        echo "fileignore="$CFGPATH"/ignorePaths" >> inotify.cfg
        echo "paths=/etc" >> inotify.cfg
        echo "showchanges=1" >> inotify.cfg
        echo "logsize=100" >> inotify.cfg

        if [ ! -f inotify.cfg ]
        then
                echo -e "No se pudo generar archivo "inotify.cfg" -> [ ${COLORNO}FAILED${FINCOLOR} ]"
		echo
                rm iNotifyd
                exit 1
        fi
	echo -e "Archivo "inotify.cfg" -> [ ${COLOROK}OK${FINCOLOR} ]"
	echo
else
	echo
        echo "!!ADVERTENCIA!!: Archivo "inotify.cfg" existe!. sera eliminado."
        echo "ejecute la instalacion nuevamente."
	rm inotify.cfg
	exit 1
	echo
fi

if [ -f iNotify_ ]
then
	cp iNotify_ iNotifyd
else
	echo -e "No se encontro archivo "iNotify_" -> [ ${COLORNO}FAILED${FINCOLOR} ]"
	echo
	rm inotify.cfg
	exit 1
fi

if [ -f install.cfg ]
then
	mkdir -p $INSTALLPATH/inotify/bin $INSTALLPATH/inotify/conf $INSTALLPATH/inotify/logs $INSTALLPATH/inotify/lock

	if [ $? -eq 0 ]; then
		sed -i "9s|SUSTITUIRconfig|$CFGPATH/install.cfg|" iNotifyd
		sed -i "10s|SUSTITUIRpid|$PIDPATH/inotify.pid|" iNotifyd
		sed -i "11s|SUSTITUIRlogfile|$LOGPATH/inotify.log|" iNotifyd
		sed -i "14s|SUSTITUCIONinstallpath|$INSTALLPATH/inotify/bin/iNotify|" iNotifyd
		sed -i "16s|SUSTITUIRconfig|$INSTALLPATH/inotify/conf/inotify.cfg|" iNotifyd
		sed -i "17s|SUSTITUIRuser|$USERNAME|" iNotifyd
		sed -i "19s|SUSTITUCIONpath|$INSTALLPATH/inotify|" iNotifyd
	else
		echo -e "No se pudieron crear los directorios. -> [ ${COLORNO}FAILED${FINCOLOR} ]"
		echo
		rm iNotifyd inotify.cfg
		exit 1
	fi
else
	echo -e "No se encontro archivo "install.cfg" -> [ ${COLORNO}FAILED${FINCOLOR} ]"
	echo
	exit 1
fi

grep "Default    requiretty" /etc/sudoers > /dev/null 2>&1
if [ $? -eq 0 ]; then
	echo
	echo "Recuerde agregar los permisos de sudo."
	echo
else
	echo
	echo -e "${COLOROK}!!ADVERTENCIA!!: El parametro en /etc/sudoers -> Defaults    requiretty"
	echo -e "puede causar problemas de ejecucion...${FINCOLOR}"
	echo
fi

if [ -f compile.sh ]; then
	/bin/sh compile.sh
	echo
else
        echo -e "No se encontro archivo "compile.sh" -> [ ${COLORNO}FAILED${FINCOLOR} ]"
	echo
	rm -rf $INSTALLPATH/inotify iNotifyd inotify.cfg
	exit 1
fi

if [ -f iNotify ] && [ -f iNotifyd ]; then
	mv iNotifyd /etc/init.d/
	chkconfig --add iNotifyd
	if [ $? -eq 0 ]; then
		chkconfig --level 5 iNotifyd on
		echo -e "Configuracion init 5. -> [ ${COLOROK}OK${FINCOLOR} ]"
		echo
	else
		echo -e "Configuracion init 5. -> [ ${COLORNO}FAILED${FINCOLOR} ]"
		echo
		rm -rf $INSTALLPATH/inotify iNotifyd inotify.cfg
		exit 1
	fi	
else
	echo -e "No se encuentran los archivos iNotify, iNotifyd. -> [ ${COLORNO}FAILED${FINCOLOR} ]"
	rm -rf $INSTALLPATH/inotify iNotifyd inotify.cfg
	echo
	exit 1
fi

mv inotify.cfg $INSTALLPATH/inotify/conf/
mv iNotify $INSTALLPATH/inotify/bin/iNotify
touch $INSTALLPATH/inotify/conf/ignorePaths

chmod 755 /etc/init.d/iNotifyd
chmod 755 $INSTALLPATH/inotify/bin/iNotify
chown -R $USERNAME $INSTALLPATH/inotify

rm install.cfg compile.sh iNotify_ makefile install.sh > /dev/null 2>&1
echo
echo "!! Instalacion finalizada correctamente !!"
echo 
