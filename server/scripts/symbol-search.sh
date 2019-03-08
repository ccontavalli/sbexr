#!/bin/bash

QUERY=${QUERY-fuffa}
PORT=${PORT-10000}
TYPE=${TYPE-symbol}
curl -H "Content-Type: application/json" -X POST -d '{"q": "'$QUERY'"}' http://127.0.0.1:$PORT/api/output/${TYPE}
