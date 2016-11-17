#!/bin/sh
#------------------------------------------------------------------------------
# $Id: stats-hosts.sh,v 1.2 2005/11/26 07:36:58 pbui Exp pbui $
#------------------------------------------------------------------------------

LOG="$1";
IDT="AKA";
CNT="`grep $IDT $LOG | sort | uniq | sed 's/ /__/g'`";

for LINE in $CNT; do
    LINE=`echo $LINE | sed 's/__/ /g'`;
    IP=`echo $LINE | cut -d ' ' -f 4`;
    NAME=`echo $LINE | cut -d ' ' -f 6`;
    echo "$IP = $NAME (`grep $IP $LOG | wc -l`)";
done

#------------------------------------------------------------------------------
