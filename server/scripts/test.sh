#!/bin/bash

METRIC=${METRIC-random-value}
PORT=${PORT-10000}
curl -H "Content-Type: application/json" -X POST -d '{}' http://127.0.0.1:$PORT/metrics/api/get/offset/${METRIC}
