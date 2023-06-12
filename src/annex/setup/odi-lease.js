exports.handler = function( event, context, callback ) {
	console.log( "Received request:\n", JSON.stringify( event ) );

	var annexID = event.AnnexID;
	if(! annexID) {
		console.log( "Failed to find annex ID." );
		callback( "Failed to find annex ID." );
		return;
	}
	console.log( "Found annex ID: " + annexID );

	var s3BucketName = event.S3BucketName;
	if(! annexID) {
		console.log( "Failed to find S3 bucket name." );
		callback( "Failed to find S3 bucket name." );
		return;
	}
	console.log( "Found S3 bucket name: " + s3BucketName );

	var leaseExpiration = event.LeaseExpiration;
	if(! leaseExpiration) {
		console.log( "Failed to find lease expiration." );
		callback( "Failed to find lease expiration." );
		return;
	}
	console.log( "Found lease expiration: " + leaseExpiration );


	var timestamp = Math.floor( Date.now() / 1000 );
	if( timestamp <= leaseExpiration ) {
		console.log( "Lease has not expired yet (" + timestamp + " <= " + leaseExpiration + ")." );
		callback( null );
		return;
	}
	console.log( "Lease has expired: " + timestamp + " > " + leaseExpiration + "." );

	// var AWS = require( 'aws-sdk' );
	// var s3 = new AWS.S3();
	const { S3 } = require("@aws-sdk/client-s3");
	var s3 = new S3({});

	var r = {
			Bucket: s3BucketName,
			Key: "config-" + annexID + ".tar.gz"
	};
	s3.deleteObject( r, function( err, data ) {
		if( err ) {
			console.log( err, err.stack );
			callback( err, err.stack );
		} else {
			console.log( "Succesfully deleted config tarball." );

	// var ec2 = new AWS.EC2();
	const { EC2 } = require("@aws-sdk/client-ec2");
	var ec2 = new EC2({});

	// Assumes we have fewer than 1000 instances.
	var q = {
		Filters : [
			{ Name : "client-token", Values : [ annexID + "*" ] }
		]
	};

	ec2.describeInstances( q, function( err, data ) {
		var instanceIDs = [];

		if( err ) {
			console.log( err, err.stack );
			callback( err, err.stack );
		} else {
			for( var i = 0; i < data.Reservations.length; ++i ) {
				var reservation = data.Reservations[i];
				for( var j = 0; j < reservation.Instances.length; ++j ) {
					var instance = reservation.Instances[j];
					instanceIDs.push( instance.InstanceId );
					}
				}


	var params = {
		InstanceIds : instanceIDs
	};
	ec2.terminateInstances( params, function( err, data ) {
		if( err && instanceIDs.length != 0 ) {
			console.log( err, err.stack );
			callback( err, err.stack );
		} else {
			console.log( "Succesfully cancelled instances." );


	// var cwe = new AWS.CloudWatchEvents();
	const { CloudWatchEvents } = require("@aws-sdk/client-cloudwatch-events");
	var cwe = new CloudWatchEvents({});

	var params = {
		Rule : annexID,
		Ids : [ annexID ]
	};
	console.log( "Attempting to remove targets: ", params );
	cwe.removeTargets( params, function( err, data ) {
	// It is not an error to fail to remove a target that
	// doesn't exist.  *sigh*
	if( err || data == null || data.FailedEntries.length != 0 ) {
		console.log( "Failed to remove targets." );
		if( err ) {
			callback( err, err.stack );
		} else {
			callback( "Failed to remove targets.", data );
		}
	} else {
		console.log( "Successfully removed targets.", data );


	var params = { Name : annexID };
	// It's OK for this to fail if it's got targets we
	// weren't told about.  We could check for that and
	// clean up our output if the daemon ever starts
	// using multiple targets per event.
	cwe.deleteRule( params, function( err, data ) {
		if( err ) {
			console.log( err, err.stack );
			callback( err, err.stack );
		} else {
			console.log( "Succesfully deleted event." );
			callback( null, "Successfully deleted event." );


		}
	});

		}
	});

		}
	});

		}
	});

		}
	});
};
