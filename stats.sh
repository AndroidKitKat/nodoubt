#!/bin/sh
#------------------------------------------------------------------------------
# $Id: stats.sh,v 1.1 2005/11/21 10:10:41 pbui Exp pbui $
#------------------------------------------------------------------------------

# This script generates report for the nodoubt webserver

#------------------------------------------------------------------------------

DIR="`dirname $0`";
LOG="/var/log/nodoubt";
STATS=stats;
WWW=$HOME/www;

#------------------------------------------------------------------------------
# Print File to Output Argument
#------------------------------------------------------------------------------

print_line () {
    echo -n "----------------------------------------" >> $1;
    echo "----------------------------------------" >> $1;
}

#------------------------------------------------------------------------------
# Prepare Statistics Directory
#------------------------------------------------------------------------------

rm -fr $WWW/$STATS;
mkdir -p $WWW/$STATS;

#------------------------------------------------------------------------------
# Generate Host Statistics
#------------------------------------------------------------------------------

RAW=$WWW/$STATS/hosts.raw
$DIR/stats-hosts.sh $LOG > $RAW

# Hosts
OUTFILE=$WWW/$STATS/hosts;
touch $OUTFILE;
print_line $OUTFILE;
echo "No Doubt - Hosts Statistics (`date`)" >> $OUTFILE;
print_line $OUTFILE;
cat $RAW >> $OUTFILE;
print_line $OUTFILE;

# Sorted Hosts 
OUTFILE=$WWW/$STATS/hosts.ascending;
touch $OUTFILE;
print_line $OUTFILE
echo "No Doubt - Sorted Hosts Statistics (`date`)" >> $OUTFILE
print_line $OUTFILE
cat $RAW | sort -n -k 2 -t '(' >> $OUTFILE
print_line $OUTFILE

# Reverse Sorted Hosts 
OUTFILE=$WWW/$STATS/hosts.descending;
touch $OUTFILE;
print_line $OUTFILE
echo "No Doubt - Reverse Sorted Hosts Statistics (`date`)" >> $OUTFILE
print_line $OUTFILE
cat $RAW | sort -r -n -k 2 -t '(' >> $OUTFILE
print_line $OUTFILE

rm $RAW;

#------------------------------------------------------------------------------
# Generate Request Statistics
#------------------------------------------------------------------------------

RAW=$WWW/$STATS/requests.raw
$DIR/stats-requests.sh $LOG > $RAW

# Requests
OUTFILE=$WWW/$STATS/requests;
touch $OUTFILE;
print_line $OUTFILE;
echo "No Doubt - Request Statistics (`date`)" >> $OUTFILE;
print_line $OUTFILE;
cat $RAW >> $OUTFILE;
print_line $OUTFILE;

# Sorted Requests
OUTFILE=$WWW/$STATS/requests.ascending;
touch $OUTFILE;
print_line $OUTFILE
echo "No Doubt - Sorted Request Statistics (`date`)" >> $OUTFILE
print_line $OUTFILE
cat $RAW | sort -n -k 2 -t '=' >> $OUTFILE
print_line $OUTFILE

# Reverse Sorted Requests
OUTFILE=$WWW/$STATS/requests.descending;
touch $OUTFILE;
print_line $OUTFILE
echo "No Doubt - Reverse Sorted Request Statistics (`date`)" >> $OUTFILE
print_line $OUTFILE
cat $RAW | sort -r -n -k 2 -t '=' >> $OUTFILE
print_line $OUTFILE

rm $RAW;
