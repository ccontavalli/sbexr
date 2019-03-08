#!/bin/bash

PORT=${PORT-10000}
curl -H "Content-Type: application/json" -X POST -d '{}' http://127.0.0.1:$PORT/metrics/api/list
