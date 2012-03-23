// This temporary utility javascript is designed to 
// experiment with various utlization queries against
// the Plumage ODS.

// usage: mongo condor_stats utilization.js

var start = new Date();
var end = new Date();

start.setMilliseconds(-60000)
print (start);
print (end);

// map function
// m = function(){
//     emit( this.mn, {count: this.cpu} );
// };

// reduce function
// r = function( key , values ){
//     var total = 0;
//     for ( var i=0; i<values.length; i++ )
//         total += values[i].count;
//     return { count : total };
// };

var cur;
// res = db.samples.machine.mapReduce(m, r, { out : "myoutput", query : {st:'Unclaimed', ts: {$gte: start, $lt: end} } } );
// cur = db.myoutput.find();
// cur.forEach( function(x) { print(tojson(x))});

//cur = db.samples.machine.find({st: {$ne: 'Claimed'}, ts: {$gte: start, $lt: end}});
//cur.forEach( function(x) { print(tojson(x))});
//print (cur.length);

//cur = db.samples.machine.find({st : {$ne: 'Unclaimed'}, ts: {$gte: start, $lt: end}});
//cur.forEach( function(x) { print(tojson(x))});
//print (cur.length);

cur = db.samples.machine.distinct('mn');
var total = cur.length;
print ("Total: "+total);

cur = db.samples.machine.distinct('mn', {st : 'Claimed', ts: {$gte: start, $lt: end}});
var claimed = cur.length;
print ("Claimed: "+claimed);

cur = db.samples.machine.distinct('mn', {st : 'Unclaimed', ts: {$gte: start, $lt: end}});
var unclaimed = cur.length;
print ("Unclaimed: "+unclaimed);

cur = db.samples.machine.distinct('mn', {st : {$nin: ['Owner','Unclaimed']}, ts: {$gte: start, $lt: end}});
var used = cur.length;
print ("Used: "+used);