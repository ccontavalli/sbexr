#!/bin/bash

METRICS=${METRICS-../test/metrics/random-value}
MAXLABELS=${MAXLABELS-4}
NUMBER=${NUMBER-100}
LABELS=(pippo pluto topolino gastone paperino paperone brigitta paperoga paperina priscilla orazio clarabella topolina)

time=`date +%s`;
for i in `seq 1 $NUMBER`; do
  labels=$(( RANDOM % MAXLABELS ))
  addlabel=""
  for (( i = 0; i < labels; i++ )); do
    elements=${#LABELS[@]}
    which=$(( RANDOM % elements ))
    addlabel="$addlabel -label=${LABELS[$which]}"
  done

  tsdb-cli --serie ../test/metrics/random-value --labelsperentry=$MAXLABELS --time=$(( time + i * 10 )) --value=$RANDOM $addlabel;
done;

