// This temporary utility javascript is designed to 
// experiment with various utlization queries against
// the Plumage ODS.

// usage: mongo condor_stats utilization.js
var cur;
var start = new Date();
var end = new Date();

// look back
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

// res = db.samples.machine.mapReduce(m, r, { out : "myoutput", query : {st:'Unclaimed', ts: {$gte: start, $lt: end} } } );
// cur = db.myoutput.find();
// cur.forEach( function(x) { print(tojson(x))});

//cur = db.samples.machine.find({st: {$ne: 'Claimed'}, ts: {$gte: start, $lt: end}});
//cur.forEach( function(x) { print(tojson(x))});
//print (cur.length);

//cur = db.samples.machine.find({st : {$ne: 'Unclaimed'}, ts: {$gte: start, $lt: end}});
//cur.forEach( function(x) { print(tojson(x))});
//print (cur.length);

// find each ts in the specified range
// cur = db.samples.machine.distinct('ts');
// var tstamp = cur.length;
// print ("Timestamps: "+tstamp);
// for (var i in cur)
//     var slots = db.samples.machine.distinct('n', {ts: cur[i]});
//     for (var j in slots)
//         print (slots[j]);

cur = db.samples.machine.distinct('n', {ts: {$gte: start, $lt: end}});
var total = cur.length;
print ("Total: "+total);

cur = db.samples.machine.distinct('n', {st : 'Claimed', ts: {$gte: start, $lt: end}});
var claimed = cur.length;
print ("Claimed: "+claimed);
print ((claimed/total*100).toFixed(2)+"%");

cur = db.samples.machine.distinct('n', {st : 'Unclaimed', ts: {$gte: start, $lt: end}});
var unclaimed = cur.length;
print ("Unclaimed: "+unclaimed);
print ((unclaimed/total*100).toFixed(2)+"%");

cur = db.samples.machine.distinct('n', {st : {$nin: ['Owner','Unclaimed']}, ts: {$gte: start, $lt: end}});
var used = cur.length;
print ("Used: "+used);
print ((used/total*100).toFixed(2)+"%");
