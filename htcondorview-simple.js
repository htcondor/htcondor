// TODO: Assumes a single, global #data-duration #data-source. Should be classes referenced uner #this.rootid

function HTCondorViewSimple(rootid) {
	var that = this;
	$(document).ready( function() { that.initialize(rootid); });
}

HTCondorViewSimple.next_graph_id = 0;

HTCondorViewSimple.prototype.new_graph_id = function() {
	HTCondorViewSimple.next_graph_id++;
	var new_id = "htcondorviewsimple" + HTCondorViewSimple.next_graph_id;
	return new_id;
}

HTCondorViewSimple.prototype.initialize = function(rootid) {
	var that = this;
	this.rootid = rootid;
	var container = $('#'+rootid);
	if(container.length == 0) {
		console.log('HTCondor View Simple is not able to intialize. There is no element with an ID of "'+id+'".');
		return;
	}
	var new_html = this.html_tabs();
	this.graph_id = this.new_graph_id();
	new_html += '<div id="'+this.graph_id+'"></div>';
	container.html(new_html);

	// Initialize tabs
	$('ul.tabs li').click(function(){
		$('ul.tabs li').removeClass('current');
		$(this).addClass('current');
	});

	$('ul.radio-tabs input').change(function() {
		that.active_filter = undefined;
		that.alt_title = undefined;
		that.change_view();
		});

	this.change_view();

};




// Return arguments for HTCondorView's constructor. Is an array of
// 4 elements:
// 0. URL to the data
// 1. graph arguments
// 2. table arguments
// 3. select tuple (sub-array)
HTCondorViewSimple.prototype.htcview_args = function(source, duration) {
	"use strict";

	switch(source) {
	case 'submitters':
		switch(duration) {
		case "now":
			return [
				// URL
				"submitters.now.json",
				// Graph
				"title=Total Jobs&"+
				"order=JobStatus&"+
				"group=JobStatus;Count&"+
				"chart=pie",
				// Table
				"order=JobStatus&"+
				"pivot=Name;JobStatus;Count",
				// Select
				["Name"]
			];
			break;
		case "day":
		case "week":
		case "month":
			return [
				// URL
				"submitters.json",
				// Graph
				"title=Total Jobs&"+
					"order=Date&"+
					"pivot=Date;JobStatus;Count&"+
					"chart=stacked",
				// Table
				"order=Date&"+
					"pivot=Name;JobStatus;avg(Count)",
				// Select
				["Name"]
			];
		}
		break;
	case 'machines':
		switch(duration) {
		case "now":
			return [
				// URL
				"machines.now.json",
				// Graph
				"title=Machine State&"+
					"order=State&"+
					"group=State;Cpus&"+
					"chart=pie",
				// Table
				"order=Arch,OpSys&"+
					"group=Arch,OpSys;State;Cpus",
				// Select
				["Arch", "OpSys"]
			];
		case "day":
		case "week":
		case "month":
			return [
				// URL
				"machines.json",
				// Graph
				"title=Machine State&"+
					"order=Date&" +
					"pivot=Date;State;Cpus&" +
					"chart=stacked&",
				// Table
				"order=Date&" +
					"pivot=Date,Arch,OpSys;State;Cpus&" +
					"group=Arch,OpSys;avg(Unclaimed),avg(Claimed),max(Unclaimed),max(Claimed)",
				// Select
				["Arch", "OpSys"]
			];
		}
		break;
	}

}


HTCondorViewSimple.prototype.change_view = function() {
	// Purge everything.
	$("#"+this.graph_id).empty();
	this.htcondor_view = null;

	var duration = $('#data-duration input[type="radio"]:checked').val()
	var source = $('#data-source input[type="radio"]:checked').val()

	var view_args = this.htcview_args(source, duration);

	this.htcondor_view = new HTCondorView(this.graph_id, view_args[0], view_args[1], view_args[2], view_args[3]);
};


HTCondorViewSimple.prototype.html_tabs = function() {
	return "" +
	"<div style='text-align: center'>\n" +
	"<ul class='radio-tabs' id='data-source'>\n" +
	"<li><input type='radio' name='data-source' id='data-source-user' value='submitters' checked> <label for='data-source-user'>Users</label>\n" +
	"<li><input type='radio' name='data-source' id='data-source-machine' value='machines'> <label for='data-source-machine'>Pool</label>\n" +
	"</ul>\n" +
	"<ul class='radio-tabs' id='data-duration'>\n" +
	"<li><input type='radio' name='data-duration' id='data-duration-now' value='now'> <label for='data-duration-now'>Now</label>\n" +
	"<li><input type='radio' name='data-duration' id='data-duration-day' value='day' checked> <label for='data-duration-day'>Day</label>\n" +
	"<li><input type='radio' name='data-duration' id='data-duration-week' value='week'> <label for='data-duration-week'>Week</label>\n" +
	"<li><input type='radio' name='data-duration' id='data-duration-month' value='month'> <label for='data-duration-month'>Month</label>\n" +
	"</ul>\n" +
	"</div>\n";
};
