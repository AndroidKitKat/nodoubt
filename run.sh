#!/bin/sh

pkill -9 nodoubt
#./nodoubt-`uname -m` -v 2 -d $HOME/www -p 80 
./nodoubt-`uname -m` -v 1 -d $HOME/www -p 80 -l /var/log/nodoubt -b
