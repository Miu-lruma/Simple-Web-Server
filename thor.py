#!/usr/bin/env python2.7

import multiprocessing
import os
import requests
import sys
import time

# Globals

PROCESSES = 1
REQUESTS  = 1
VERBOSE   = False
URL       = None
# Functions

def usage(status=0):
    print '''Usage: {} [-p PROCESSES -r REQUESTS -v] URL
    -h              Display help message
    -v              Display verbose output

    -p  PROCESSES   Number of processes to utilize (1)
    -r  REQUESTS    Number of requests per process (1)
    '''.format(os.path.basename(sys.argv[0]))
    sys.erequestit(status)

def do_request(pid):
    average = 0
    for request in range(REQUESTS):
        initialTime = time.time()
        requests.get(URL)
        totalTime = time.time()-initialTime
        average = average + totalTime
        print "Process: " + str(pid) + ", Request: " + str(request) + ", Elapsed Time: " + "{0:.2f}".format(totalTime) 
    average = average / REQUESTS
    print "Process: " + str(pid) +", AVERAGE   , Elapsed Time: " + "{0:.2f}".format(average) 
    throughput = 1/average
    print "Throughput: " + str(throughput)
    return average;
    pass

# Main erequestecution

if __name__ == '__main__':
    # Parse command line arguments
    averages = 0;
    args = sys.argv[1:]
    while len(args) and args[0].startswith('-') and len(args[0]) > 1:
    	arg = args.pop(0)
    	if arg == '-h':
    		usage(0)
    	elif arg == '-v':
    		VERBOSE = True
    	elif arg == '-p':
    		PROCESSES = int(args.pop(0))
    	elif arg == '-r':
    		REQUESTS = int(args.pop(0))
    	else:
    		usage(1)	
    if len(args): 
    	URL = args.pop(0)
    else:
	usage(1)
    
# Print Verbose if neccessary
    response = requests.get(URL)
    if VERBOSE:
        print response.text

# Create pool of workers and perform requests
    pool = multiprocessing.Pool(PROCESSES)
    processAverage = pool.map(do_request, range(PROCESSES))
    print "TOTAL AVERAGE ELAPSED TIME: " + "{0:.2f}".format((sum(processAverage)/len(processAverage)))
    pass

# vim: set sts=4 sw=4 ts=8 expandtab ft=python:



