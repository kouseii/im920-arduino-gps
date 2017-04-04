#!/bin/sh

curl -X POST -H "Content-type: text/plain" http://localhost:8000/publish -d $1