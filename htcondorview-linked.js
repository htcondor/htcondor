function HTCondorViewLinked(rootid) {
	"use strict";
	var that = this;
	$(document).ready( function() { that.initialize(rootid); });
}


HTCondorViewLinked.prototype.new_graph_id = function() {
	"use strict";
	return HTCondorView.prototype.new_graph_id();
};

HTCondorViewLinked.prototype.initialize = function(rootid) {
	"use strict";
	var that = this;
	this.rootid = rootid;
	var container = $('#'+rootid);
	if(container.length === 0) {
		console.log('HTCondor View Simple is not able to intialize. There is no element with an ID of "'+id+'".');
		return;
	}
	var new_html = this.html_tabs();
	this.graph_id = this.new_graph_id();
	new_html += '<div id="'+this.graph_id+'"></div>';
	container.html(new_html);

	// Initialize tabs
	$('#'+this.rootid+' ul.tabs li').click(function(){
		$('#'+this.rootid+' ul.tabs li').removeClass('current');
		$(this).addClass('current');
	});

	$('#'+this.rootid+' ul.radio-tabs input').change(function() {
		that.active_filter = undefined;
		that.alt_title = undefined;
		that.change_view();
		});

	this.change_view();

};




// Return arguments for HTCondorView's constructor.  Returns object with
// url - data,
// graph - afterquery arguments
// table - afterquery arguments
// select - tuple for selecting rows in the table
// title - title for the graph
HTCondorViewLinked.prototype.htcview_args = function(source, duration) {
	"use strict";

	switch(source) {
	case 'submitters':
		switch(duration) {
		case "now":
			return {
				url: "submitters.now.json",
				graph: "order=JobStatus&"+
					"group=JobStatus;Count&"+
					"chart=pie",
				table: "order=JobStatus&"+
					"pivot=Name;JobStatus;Count",
				select: ["Name"],
				title: "Total Jobs"
			};
		case "day":
		case "week":
		case "month":
			return {
				url: "submitters.json",
				graph: "order=Date&"+
					"pivot=Date;JobStatus;Count&"+
					"chart=stacked",
				table: "order=Date&"+
					"pivot=Name;JobStatus;avg(Count)",
				select: ["Name"],
				title: "Total Jobs"
			};
		}
		break;
	case 'machines':
		switch(duration) {
		case "now":
			return {
				url: "machines.now.json",
				graph: "order=State&"+
					"group=State;Cpus&"+
					"chart=pie",
				table: "order=Arch,OpSys&"+
					"group=Arch,OpSys;State;Cpus",
				select: ["Arch", "OpSys"],
				title: "Machine State"
			};
		case "day":
		case "week":
		case "month":
			return {
				url: "machines.json",
				graph: "order=Date&" +
					"pivot=Date;State;Cpus&" +
					"chart=stacked&",
				table: "order=Date&" +
					"pivot=Date,Arch,OpSys;State;Cpus&" +
					"group=Arch,OpSys;avg(Unclaimed),avg(Claimed),max(Unclaimed),max(Claimed)",
				select: ["Arch", "OpSys"],
				title: "Machine State"
			};
		}
		break;
	}
};


HTCondorViewLinked.prototype.change_view = function() {
	// Purge everything.
	//$("#"+this.graph_id).empty();
	//this.htcondor_view = null;

	var duration = $("#"+this.rootid+' .data-duration input[type="radio"]:checked').val();
	var source = $("#"+this.rootid+' .data-source input[type="radio"]:checked').val();

	var view_args = this.htcview_args(source, duration);

	this.htcondor_view = new HTCondorView({
		dst_id: this.graph_id,
		data_url: view_args.url,
		graph_query: view_args.graph,  
		table_query: view_args.table,
		select_tuple: view_args.select,
		title: view_args.title
	});
};


HTCondorViewLinked.prototype.html_tabs = function() {
	// We need to ensure the IDs are all unique.
	var id_su = this.new_graph_id();
	var id_sm = this.new_graph_id();
	var id_dn = this.new_graph_id();
	var id_dd = this.new_graph_id();
	var id_dw = this.new_graph_id();
	var id_dm = this.new_graph_id();
	return "" +
	"<div style='text-align: center'>\n" +
		"<ul class='radio-tabs data-source'>\n" +
			"<li><input type='radio' name='data-source' id='data-source-user"+id_su+"' value='submitters' checked> <label for='data-source-user"+id_su+"'>Users</label>\n" +
			"<li><input type='radio' name='data-source' id='data-source-machine"+id_sm+"' value='machines'> <label for='data-source-machine"+id_sm+"'>Pool</label>\n" +
		"</ul>\n" +
		"<ul class='radio-tabs data-duration'>\n" +
			"<li><input type='radio' name='data-duration' id='data-duration-now"+id_dn+"' value='now'> <label for='data-duration-now"+id_dn+"'>Now</label>\n" +
			"<li><input type='radio' name='data-duration' id='data-duration-day"+id_dd+"' value='day' checked> <label for='data-duration-day"+id_dd+"'>Day</label>\n" +
			"<li><input type='radio' name='data-duration' id='data-duration-week"+id_dw+"' value='week'> <label for='data-duration-week"+id_dw+"'>Week</label>\n" +
			"<li><input type='radio' name='data-duration' id='data-duration-month"+id_dm+"' value='month'> <label for='data-duration-month"+id_dm+"'>Month</label>\n" +
		"</ul>\n" +
	"</div>\n";
};
