#!/usr/bin/python2

from __future__ import print_function

import os
import sys
import boto3

def main( argv ):
    #
    # Ask condor_config_val for the default AWS region, in case we need it.
    #
    pipe = os.popen( 'condor_config_val ANNEX_DEFAULT_AWS_REGION' )
    default_region = pipe.read().strip()
    if pipe.close():
        print( "Setting default region to 'us-east-1'..." )
        default_region = 'us-east-1'

    #
    # Get the region-specific credentials and lambda function ARNs.
    #
    data_by_region = {}

    ak_file_raw = fetch_raw_from_CCV( 'ANNEX_DEFAULT_ACCESS_KEY_FILE' )
    sk_file_raw = fetch_raw_from_CCV( 'ANNEX_DEFAULT_SECRET_KEY_FILE' )
    odi_ARN_raw = fetch_raw_from_CCV( 'ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN' )
    sfr_ARN_raw = fetch_raw_from_CCV( 'ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN' )

    parse_CCV_output( default_region, data_by_region, ak_file_raw, 'ak_file' )
    parse_CCV_output( default_region, data_by_region, sk_file_raw, 'sk_file' )
    parse_CCV_output( default_region, data_by_region, odi_ARN_raw, 'function_ARNs', True )
    parse_CCV_output( default_region, data_by_region, sfr_ARN_raw, 'function_ARNs', True )

    #
    # For each region, update each of its lambda functions to use the new runtime.
    #
    for regionName, region in data_by_region.items():
        regionName = regionName.replace( '_', '-' )
        update_regional_functions( regionName, region['ak_file'], region['sk_file'], region['function_ARNs'] )

    return 0

def fetch_raw_from_CCV( config_attr ):
    pipe = os.popen( 'condor_config_val -dump ' + config_attr )
    temp = pipe.read()
    rv = pipe.close()
    if rv:
        raise RuntimeError( 'Problem (' + str(rv) + ') obtaining ' + config_attr )
    return temp

def parse_CCV_output( default_region, data_by_region, output, attribute, islist=False ):
    for line in output.split( "\n" ):
        if line.count( '=' ) == 1:
            # machine-generated, no reason to use re_split().
            k, v = line.split( ' = ', 1 )

            region = default_region
            if k.count( '.' ) == 1:
                region = k.split( '.' )[0]

            if region not in data_by_region:
                data_by_region[region] = {}
            if islist:
                if attribute not in data_by_region[region]:
                    data_by_region[region][attribute] = []
                data_by_region[region][attribute].append( v )
            else:
                data_by_region[region][attribute] = v

def update_regional_functions( region, ak_file, sk_file, function_ARNs ):
    accessKeyID = open( ak_file, 'r' ).read().strip()
    secretAccessKey = open( sk_file, 'r' ).read().strip()

    lc = boto3.client( 'lambda',
        region_name=region,
        aws_access_key_id=accessKeyID,
        aws_secret_access_key=secretAccessKey )

    runtime = 'nodejs12.x'
    for function in function_ARNs:
        print( "Upgrading function " + function + "..." )
        update = lc.update_function_configuration( FunctionName=function, Runtime=runtime )
        if( update['Runtime'] == runtime ):
            print( "... done." )
        else:
            print( "... failed." )

if __name__ == "__main__":
    try:
        exit( main( sys.argv ) )
    except IOError as io_error:
        print( "Failed to read credential file '" + io_error.filename + "': " + io_error.strerror )
        exit( 1 )
    except RuntimeError as rt_error:
        print( rt_error )
        exit( 1 )
